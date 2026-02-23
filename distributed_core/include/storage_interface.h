#ifndef STORAGE_INTERFACE_H
#define STORAGE_INTERFACE_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

/* Storage Node Information */
typedef struct {
    uint32_t node_id;
    char ip_address[64];
    uint16_t port;
    uint64_t capacity;              // Total capacity in bytes
    uint64_t used;                  // Used space in bytes
    uint64_t available;             // Available space in bytes
    bool online;                    // Node availability status
    time_t last_heartbeat;          // Last heartbeat timestamp
} storage_node_info_t;

/* Storage Request Types */
typedef enum {
    STORAGE_REQ_WRITE = 1,
    STORAGE_REQ_READ = 2,
    STORAGE_REQ_DELETE = 3,
    STORAGE_REQ_REPLICATE = 4,
    STORAGE_REQ_HEALTH_CHECK = 5
} storage_request_type_t;

/* Storage Request */
typedef struct {
    storage_request_type_t type;
    char file_id[64];               // File identifier
    uint64_t offset;                // Offset for read/write
    uint64_t length;                // Length for read/write
    uint8_t *data;                  // Data payload (for writes)
    uint32_t target_node_id;        // Target storage node
    
    // For replication
    uint32_t source_node_id;        // Source node for replication
    uint32_t replica_nodes[8];      // Replica destinations
    uint32_t num_replicas;          // Number of replicas
} storage_request_t;

/* Storage Response */
typedef struct {
    int status;                     // 0 = success, negative = error code
    char error_msg[256];            // Error message if any
    uint8_t *data;                  // Data payload (for reads)
    size_t data_len;                // Data length
    uint64_t bytes_transferred;     // Bytes read/written
} storage_response_t;

/* Storage Interface Manager */
typedef struct {
    // Available storage nodes
    storage_node_info_t *nodes;
    uint32_t num_nodes;
    uint32_t max_nodes;
    pthread_rwlock_t nodes_lock;
    
    // Load balancing strategy
    uint32_t next_node_idx;         // Round-robin counter
    
    // Connection pool (reuse TCP connections)
    void *connection_pool;          // Hash map: node_id -> socket_fd
    pthread_mutex_t pool_lock;
    
    // Statistics
    uint64_t total_reads;
    uint64_t total_writes;
    uint64_t total_deletes;
    uint64_t bytes_written;
    uint64_t bytes_read;
} storage_interface_t;

/* Storage Interface API Functions */

/**
 * Initialize storage interface
 * @param max_nodes Maximum number of storage nodes
 * @return Initialized storage_interface_t or NULL on error
 */
storage_interface_t *storage_interface_init(uint32_t max_nodes);

/**
 * Destroy and cleanup storage interface
 */
void storage_interface_destroy(storage_interface_t *iface);

/**
 * Register a storage node
 * @param iface Storage interface
 * @param node_id Storage node ID
 * @param ip_address Node IP address
 * @param port Node port
 * @param capacity Total capacity in bytes
 * @return 0 on success, -1 on error
 */
int storage_interface_register_node(storage_interface_t *iface, uint32_t node_id,
                                    const char *ip_address, uint16_t port,
                                    uint64_t capacity);

/**
 * Unregister a storage node
 * @param iface Storage interface
 * @param node_id Storage node ID
 * @return 0 on success, -1 on error
 */
int storage_interface_unregister_node(storage_interface_t *iface, uint32_t node_id);

/**
 * Update storage node status (capacity, availability)
 * @param iface Storage interface
 * @param node_id Storage node ID
 * @param used Used space in bytes
 * @param available Available space in bytes
 * @param online Node availability
 * @return 0 on success, -1 on error
 */
int storage_interface_update_node_status(storage_interface_t *iface, uint32_t node_id,
                                         uint64_t used, uint64_t available, bool online);

/**
 * Select storage nodes for a new file (load balancing)
 * @param iface Storage interface
 * @param file_size File size in bytes
 * @param num_replicas Desired number of replicas
 * @param selected_nodes Output array of selected node IDs
 * @return Number of nodes selected, -1 on error
 */
int storage_interface_select_nodes(storage_interface_t *iface, uint64_t file_size,
                                   uint32_t num_replicas, uint32_t *selected_nodes);

/**
 * Write data to a storage node (direct transfer)
 * @param iface Storage interface
 * @param node_id Target storage node ID
 * @param file_id File identifier
 * @param offset Offset to write at
 * @param data Data to write
 * @param length Data length
 * @param response Output response
 * @return 0 on success, -1 on error
 */
int storage_interface_write(storage_interface_t *iface, uint32_t node_id,
                            const char *file_id, uint64_t offset,
                            const uint8_t *data, uint64_t length,
                            storage_response_t *response);

/**
 * Read data from a storage node
 * @param iface Storage interface
 * @param node_id Source storage node ID
 * @param file_id File identifier
 * @param offset Offset to read from
 * @param length Length to read
 * @param response Output response (contains data)
 * @return 0 on success, -1 on error
 */
int storage_interface_read(storage_interface_t *iface, uint32_t node_id,
                           const char *file_id, uint64_t offset, uint64_t length,
                           storage_response_t *response);

/**
 * Delete data from a storage node
 * @param iface Storage interface
 * @param node_id Target storage node ID
 * @param file_id File identifier
 * @param response Output response
 * @return 0 on success, -1 on error
 */
int storage_interface_delete(storage_interface_t *iface, uint32_t node_id,
                             const char *file_id, storage_response_t *response);

/**
 * Replicate data from one node to another
 * @param iface Storage interface
 * @param source_node_id Source storage node ID
 * @param target_node_id Target storage node ID
 * @param file_id File identifier
 * @param response Output response
 * @return 0 on success, -1 on error
 */
int storage_interface_replicate(storage_interface_t *iface, uint32_t source_node_id,
                                uint32_t target_node_id, const char *file_id,
                                storage_response_t *response);

/**
 * Check health of a storage node
 * @param iface Storage interface
 * @param node_id Storage node ID
 * @return true if healthy, false otherwise
 */
bool storage_interface_health_check(storage_interface_t *iface, uint32_t node_id);

/**
 * Get storage node by ID
 * @param iface Storage interface
 * @param node_id Storage node ID
 * @return Storage node info or NULL if not found
 */
storage_node_info_t *storage_interface_get_node(storage_interface_t *iface, uint32_t node_id);

/**
 * Get all healthy storage nodes
 * @param iface Storage interface
 * @param nodes Output array (caller must allocate)
 * @param max_nodes Maximum number of nodes to return
 * @return Number of healthy nodes
 */
int storage_interface_get_healthy_nodes(storage_interface_t *iface,
                                        storage_node_info_t *nodes, uint32_t max_nodes);

/**
 * Free storage response resources
 */
void storage_response_free(storage_response_t *response);

#endif /* STORAGE_INTERFACE_H */
