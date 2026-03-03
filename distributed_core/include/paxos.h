#ifndef PAXOS_H
#define PAXOS_H

#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>

/* Paxos Protocol Phase Types */
typedef enum {
    PAXOS_PHASE_PREPARE = 1,
    PAXOS_PHASE_PROMISE = 2,
    PAXOS_PHASE_ACCEPT = 3,
    PAXOS_PHASE_ACCEPTED = 4,
    PAXOS_PHASE_LEARN = 5
} paxos_phase_t;

/* Paxos Message Structure */
typedef struct {
    paxos_phase_t phase;
    uint64_t proposal_id;       // Unique proposal number (ballot number)
    uint64_t sequence_number;   // Log sequence number
    uint32_t sender_id;         // Node ID of sender
    uint32_t acceptor_id;       // Node ID of acceptor (for Promise/Accepted)
    
    // Promised/Accepted value data
    uint64_t accepted_id;       // Last accepted proposal ID
    void *value;                // Proposed/Accepted value (serialized metadata operation)
    size_t value_len;           // Length of value data
    
    // For multi-Paxos optimization
    bool is_noop;               // Whether this is a no-op proposal
} paxos_message_t;

/* Paxos Proposal State */
typedef struct {
    uint64_t proposal_id;
    uint64_t sequence_number;
    void *proposed_value;
    size_t value_len;
    
    // Quorum tracking
    uint32_t promise_count;
    uint32_t accept_count;
    uint32_t reject_count;
    
    pthread_mutex_t lock;
    pthread_cond_t quorum_reached;
    bool in_use;
    bool completed;
    bool committed;
} paxos_proposal_t;

typedef int (*paxos_broadcast_callback_t)(const paxos_message_t *msg, void *ctx);

/* Paxos Node Configuration */
typedef struct {
    uint32_t node_id;
    uint32_t total_nodes;
    uint32_t quorum_size;       // (total_nodes / 2) + 1
    
    // Current Paxos state
    uint64_t highest_proposal_seen;
    uint64_t highest_sequence_committed;
    uint64_t highest_sequence_persisted;
    uint64_t next_sequence_number;
    uint64_t last_accepted_proposal;
    
    // Active proposals (Proposer role)
    paxos_proposal_t *active_proposals;
    size_t max_proposals;
    
    pthread_mutex_t state_lock;
    
    // Callbacks
    int (*persist_callback)(uint64_t seq, void *value, size_t len);
    void (*apply_callback)(uint64_t seq, void *value, size_t len);
    paxos_broadcast_callback_t broadcast_callback;
    void *broadcast_context;
    uint32_t proposal_timeout_ms;
} paxos_node_t;

/* Paxos API Functions */

/**
 * Initialize a Paxos node
 * @param node_id Unique identifier for this node
 * @param total_nodes Total number of nodes in the cluster
 * @param persist_cb Callback to persist accepted values (WAL)
 * @param apply_cb Callback to apply committed values to state machine
 * @return Initialized paxos_node_t or NULL on error
 */
paxos_node_t *paxos_init(uint32_t node_id, uint32_t total_nodes,
                         int (*persist_cb)(uint64_t, void*, size_t),
                         void (*apply_cb)(uint64_t, void*, size_t));

/**
 * Destroy and cleanup Paxos node
 */
void paxos_destroy(paxos_node_t *node);

/**
 * Propose a new value (Leader/Proposer role)
 * @param node Paxos node
 * @param value Value to propose (serialized metadata operation)
 * @param value_len Length of value
 * @return 0 on success, -1 on failure
 */
int paxos_propose(paxos_node_t *node, void *value, size_t value_len);

/**
 * Configure network broadcast callback used by proposer to send PREPARE/ACCEPT
 * @param node Paxos node
 * @param cb Callback that broadcasts a Paxos message to all peers
 * @param ctx Opaque context passed to callback
 */
void paxos_set_broadcast_callback(paxos_node_t *node,
                                  paxos_broadcast_callback_t cb,
                                  void *ctx);

/**
 * Configure proposal timeout in milliseconds (default: 3000ms)
 */
void paxos_set_proposal_timeout(paxos_node_t *node, uint32_t timeout_ms);

/**
 * Handle incoming Paxos message (Acceptor/Learner role)
 * @param node Paxos node
 * @param msg Received message
 * @param response Output response message (caller must free)
 * @return 0 if response should be sent, -1 otherwise
 */
int paxos_handle_message(paxos_node_t *node, const paxos_message_t *msg, 
                         paxos_message_t **response);

/**
 * Generate next proposal ID (monotonically increasing, unique per node)
 */
uint64_t paxos_next_proposal_id(paxos_node_t *node);

/**
 * Check if quorum is reached for a proposal
 */
bool paxos_check_quorum(paxos_node_t *node, uint32_t count);

/**
 * Serialize Paxos message for network transmission
 * @param msg Message to serialize
 * @param buffer Output buffer (caller must free)
 * @param len Output length
 * @return 0 on success, -1 on error
 */
int paxos_serialize_message(const paxos_message_t *msg, uint8_t **buffer, size_t *len);

/**
 * Deserialize Paxos message from network data
 * @param buffer Input buffer
 * @param len Buffer length
 * @param msg Output message (caller must free value field)
 * @return 0 on success, -1 on error
 */
int paxos_deserialize_message(const uint8_t *buffer, size_t len, paxos_message_t *msg);

/**
 * Free Paxos message resources
 */
void paxos_free_message(paxos_message_t *msg);

#endif /* PAXOS_H */
