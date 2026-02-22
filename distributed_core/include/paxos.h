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
    bool completed;
    bool committed;
} paxos_proposal_t;

/* Paxos Node Configuration */
typedef struct {
    uint32_t node_id;
    uint32_t total_nodes;
    uint32_t quorum_size;       // (total_nodes / 2) + 1
    
    // Current Paxos state
    uint64_t highest_proposal_seen;
    uint64_t highest_sequence_committed;
    uint64_t last_accepted_proposal;
    
    // Active proposals (Proposer role)
    paxos_proposal_t *active_proposals;
    size_t max_proposals;
    
    pthread_mutex_t state_lock;
    
    // Callbacks
    int (*persist_callback)(uint64_t seq, void *value, size_t len);
    void (*apply_callback)(uint64_t seq, void *value, size_t len);
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
