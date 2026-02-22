#ifndef NETWORK_ENGINE_H
#define NETWORK_ENGINE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define MAX_PEERS 32
#define MAX_PENDING_CONNECTIONS 128
#define RECV_BUFFER_SIZE 65536

/* Network Message Types */
typedef enum {
    MSG_TYPE_PAXOS = 1,
    MSG_TYPE_METADATA_QUERY = 2,
    MSG_TYPE_METADATA_RESPONSE = 3,
    MSG_TYPE_HEARTBEAT = 4,
    MSG_TYPE_REPLICATION = 5,
    MSG_TYPE_STORAGE_REQUEST = 6,
    MSG_TYPE_STORAGE_RESPONSE = 7
} message_type_t;

/* Network Message Header */
typedef struct {
    uint32_t magic;                 // Magic number for validation (0xFEEDFACE)
    message_type_t type;            // Message type
    uint32_t sender_id;             // Node ID of sender
    uint32_t sequence;              // Sequence number
    uint32_t payload_len;           // Length of payload
    uint32_t checksum;              // Checksum of payload
} __attribute__((packed)) message_header_t;

/* Network Message */
typedef struct {
    message_header_t header;
    uint8_t *payload;               // Actual message data
} network_message_t;

/* Peer Connection */
typedef struct {
    uint32_t node_id;               // Peer node ID
    char ip_address[64];            // Peer IP address
    uint16_t port;                  // Peer port
    int socket_fd;                  // Socket file descriptor
    bool connected;                 // Connection status
    time_t last_heartbeat;          // Last heartbeat timestamp
    pthread_mutex_t send_lock;      // Lock for sending
} peer_connection_t;

/* Network Engine */
typedef struct {
    uint32_t node_id;               // This node's ID
    uint16_t listen_port;           // Port to listen on
    int listen_fd;                  // Listening socket fd
    
    // Peer connections
    peer_connection_t peers[MAX_PEERS];
    uint32_t num_peers;
    pthread_rwlock_t peers_lock;
    
    // Event loop thread
    pthread_t event_thread;
    bool running;
    
    // Message handlers (callbacks)
    void (*message_handler)(uint32_t sender_id, message_type_t type, 
                           const uint8_t *payload, size_t payload_len, void *ctx);
    void *handler_context;
    
    // Connection events
    void (*on_peer_connected)(uint32_t peer_id, void *ctx);
    void (*on_peer_disconnected)(uint32_t peer_id, void *ctx);
    
    // Statistics
    uint64_t messages_sent;
    uint64_t messages_received;
    uint64_t bytes_sent;
    uint64_t bytes_received;
} network_engine_t;

/* Network Engine API Functions */

/**
 * Initialize network engine
 * @param node_id This node's unique ID
 * @param listen_port Port to listen on
 * @param handler Message handler callback
 * @param ctx Context for callbacks
 * @return Initialized network_engine_t or NULL on error
 */
network_engine_t *network_engine_init(uint32_t node_id, uint16_t listen_port,
                                      void (*handler)(uint32_t, message_type_t, 
                                                     const uint8_t*, size_t, void*),
                                      void *ctx);

/**
 * Destroy and cleanup network engine
 */
void network_engine_destroy(network_engine_t *engine);

/**
 * Start network engine (begins listening and event loop)
 * @param engine Network engine
 * @return 0 on success, -1 on error
 */
int network_engine_start(network_engine_t *engine);

/**
 * Stop network engine
 * @param engine Network engine
 */
void network_engine_stop(network_engine_t *engine);

/**
 * Add a peer to connect to
 * @param engine Network engine
 * @param node_id Peer node ID
 * @param ip_address Peer IP address
 * @param port Peer port
 * @return 0 on success, -1 on error
 */
int network_engine_add_peer(network_engine_t *engine, uint32_t node_id,
                            const char *ip_address, uint16_t port);

/**
 * Remove a peer
 * @param engine Network engine
 * @param node_id Peer node ID
 * @return 0 on success, -1 on error
 */
int network_engine_remove_peer(network_engine_t *engine, uint32_t node_id);

/**
 * Send message to a specific peer (non-blocking)
 * @param engine Network engine
 * @param peer_id Destination peer node ID
 * @param type Message type
 * @param payload Message payload
 * @param payload_len Payload length
 * @return 0 on success, -1 on error
 */
int network_engine_send(network_engine_t *engine, uint32_t peer_id,
                       message_type_t type, const uint8_t *payload, size_t payload_len);

/**
 * Broadcast message to all peers
 * @param engine Network engine
 * @param type Message type
 * @param payload Message payload
 * @param payload_len Payload length
 * @return Number of successful sends, -1 on error
 */
int network_engine_broadcast(network_engine_t *engine, message_type_t type,
                            const uint8_t *payload, size_t payload_len);

/**
 * Check if peer is connected
 * @param engine Network engine
 * @param peer_id Peer node ID
 * @return true if connected, false otherwise
 */
bool network_engine_is_peer_connected(network_engine_t *engine, uint32_t peer_id);

/**
 * Get list of connected peers
 * @param engine Network engine
 * @param peer_ids Output array of peer IDs (caller must allocate)
 * @param max_peers Maximum number of peers to return
 * @return Number of connected peers
 */
int network_engine_get_connected_peers(network_engine_t *engine, 
                                       uint32_t *peer_ids, uint32_t max_peers);

/**
 * Serialize network message (header + payload)
 * @param msg Message to serialize
 * @param buffer Output buffer (caller must free)
 * @param len Output length
 * @return 0 on success, -1 on error
 */
int network_serialize_message(const network_message_t *msg, uint8_t **buffer, size_t *len);

/**
 * Deserialize network message
 * @param buffer Input buffer
 * @param len Buffer length
 * @param msg Output message (caller must free payload)
 * @return 0 on success, -1 on error
 */
int network_deserialize_message(const uint8_t *buffer, size_t len, network_message_t *msg);

/**
 * Free network message resources
 */
void network_free_message(network_message_t *msg);

/**
 * Set TCP socket to non-blocking mode
 */
int network_set_nonblocking(int fd);

/**
 * Set TCP socket options (keepalive, nodelay, etc.)
 */
int network_set_socket_options(int fd);

#endif /* NETWORK_ENGINE_H */
