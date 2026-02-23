#include "paxos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>

/* Initialize Paxos node */
paxos_node_t *paxos_init(uint32_t node_id, uint32_t total_nodes,
                         int (*persist_cb)(uint64_t, void*, size_t),
                         void (*apply_cb)(uint64_t, void*, size_t)) {
    if (total_nodes < 3 || total_nodes % 2 == 0) {
        fprintf(stderr, "Paxos requires odd number of nodes >= 3\n");
        return NULL;
    }
    
    paxos_node_t *node = (paxos_node_t *)calloc(1, sizeof(paxos_node_t));
    if (!node) {
        return NULL;
    }
    
    node->node_id = node_id;
    node->total_nodes = total_nodes;
    node->quorum_size = (total_nodes / 2) + 1;
    
    node->highest_proposal_seen = 0;
    node->highest_sequence_committed = 0;
    node->last_accepted_proposal = 0;
    
    // Initialize proposal pool
    node->max_proposals = 256;
    node->active_proposals = (paxos_proposal_t *)calloc(node->max_proposals, 
                                                         sizeof(paxos_proposal_t));
    if (!node->active_proposals) {
        free(node);
        return NULL;
    }
    
    // Initialize locks
    pthread_mutex_init(&node->state_lock, NULL);
    
    for (size_t i = 0; i < node->max_proposals; i++) {
        pthread_mutex_init(&node->active_proposals[i].lock, NULL);
        pthread_cond_init(&node->active_proposals[i].quorum_reached, NULL);
    }
    
    node->persist_callback = persist_cb;
    node->apply_callback = apply_cb;
    
    return node;
}

/* Destroy Paxos node */
void paxos_destroy(paxos_node_t *node) {
    if (!node) return;
    
    pthread_mutex_destroy(&node->state_lock);
    
    for (size_t i = 0; i < node->max_proposals; i++) {
        pthread_mutex_destroy(&node->active_proposals[i].lock);
        pthread_cond_destroy(&node->active_proposals[i].quorum_reached);
        if (node->active_proposals[i].proposed_value) {
            free(node->active_proposals[i].proposed_value);
        }
    }
    
    free(node->active_proposals);
    free(node);
}

/* Generate next unique proposal ID */
uint64_t paxos_next_proposal_id(paxos_node_t *node) {
    pthread_mutex_lock(&node->state_lock);
    
    // Proposal ID format: high bits = counter, low bits = node_id
    // This ensures global uniqueness and ordering
    uint64_t proposal_id = ((node->highest_proposal_seen + 1) << 16) | node->node_id;
    node->highest_proposal_seen = proposal_id;
    
    pthread_mutex_unlock(&node->state_lock);
    
    return proposal_id;
}

/* Check if quorum is reached */
bool paxos_check_quorum(paxos_node_t *node, uint32_t count) {
    return count >= node->quorum_size;
}

/* Propose a new value (Proposer role - Phase 1: Prepare) */
int paxos_propose(paxos_node_t *node, void *value, size_t value_len) {
    if (!node || !value || value_len == 0) {
        return -1;
    }
    
    // Generate unique proposal ID
    uint64_t proposal_id = paxos_next_proposal_id(node);
    
    pthread_mutex_lock(&node->state_lock);
    uint64_t sequence = ++node->highest_sequence_committed;
    pthread_mutex_unlock(&node->state_lock);
    
    // Find free proposal slot
    paxos_proposal_t *proposal = NULL;
    for (size_t i = 0; i < node->max_proposals; i++) {
        pthread_mutex_lock(&node->active_proposals[i].lock);
        if (!node->active_proposals[i].completed) {
            proposal = &node->active_proposals[i];
            proposal->proposal_id = proposal_id;
            proposal->sequence_number = sequence;
            proposal->proposed_value = malloc(value_len);
            memcpy(proposal->proposed_value, value, value_len);
            proposal->value_len = value_len;
            proposal->promise_count = 1; // Count self
            proposal->accept_count = 0;
            proposal->reject_count = 0;
            proposal->completed = false;
            proposal->committed = false;
            pthread_mutex_unlock(&node->active_proposals[i].lock);
            break;
        }
        pthread_mutex_unlock(&node->active_proposals[i].lock);
    }
    
    if (!proposal) {
        fprintf(stderr, "No free proposal slots\n");
        return -1;
    }
    
    // NOTE: Caller must send PREPARE messages to all acceptors via network_engine
    // and handle PROMISE/REJECT responses via paxos_handle_message
    
    return 0;
}

/* Handle incoming Paxos message (Acceptor/Learner role) */
int paxos_handle_message(paxos_node_t *node, const paxos_message_t *msg,
                         paxos_message_t **response) {
    if (!node || !msg || !response) {
        return -1;
    }
    
    *response = NULL;
    
    pthread_mutex_lock(&node->state_lock);
    
    switch (msg->phase) {
        case PAXOS_PHASE_PREPARE:
            // Acceptor receives PREPARE(n)
            if (msg->proposal_id > node->highest_proposal_seen) {
                // Promise not to accept proposals < n
                node->highest_proposal_seen = msg->proposal_id;
                
                *response = (paxos_message_t *)calloc(1, sizeof(paxos_message_t));
                (*response)->phase = PAXOS_PHASE_PROMISE;
                (*response)->proposal_id = msg->proposal_id;
                (*response)->sequence_number = msg->sequence_number;
                (*response)->sender_id = node->node_id;
                (*response)->acceptor_id = node->node_id;
                (*response)->accepted_id = node->last_accepted_proposal;
                (*response)->value = NULL;
                (*response)->value_len = 0;
                
                pthread_mutex_unlock(&node->state_lock);
                return 0;
            }
            break;
            
        case PAXOS_PHASE_PROMISE:
            // Proposer receives PROMISE from acceptor
            // Update proposal with promise count
            for (size_t i = 0; i < node->max_proposals; i++) {
                pthread_mutex_lock(&node->active_proposals[i].lock);
                if (node->active_proposals[i].proposal_id == msg->proposal_id &&
                    !node->active_proposals[i].completed) {
                    node->active_proposals[i].promise_count++;
                    
                    if (paxos_check_quorum(node, node->active_proposals[i].promise_count)) {
                        // Quorum reached, proceed to ACCEPT phase
                        pthread_cond_signal(&node->active_proposals[i].quorum_reached);
                    }
                    pthread_mutex_unlock(&node->active_proposals[i].lock);
                    break;
                }
                pthread_mutex_unlock(&node->active_proposals[i].lock);
            }
            break;
            
        case PAXOS_PHASE_ACCEPT:
            // Acceptor receives ACCEPT(n, value)
            if (msg->proposal_id >= node->highest_proposal_seen) {
                node->highest_proposal_seen = msg->proposal_id;
                node->last_accepted_proposal = msg->proposal_id;
                
                // Persist to WAL before acknowledging
                if (node->persist_callback) {
                    if (node->persist_callback(msg->sequence_number, msg->value, 
                                              msg->value_len) != 0) {
                        pthread_mutex_unlock(&node->state_lock);
                        return -1;
                    }
                }
                
                *response = (paxos_message_t *)calloc(1, sizeof(paxos_message_t));
                (*response)->phase = PAXOS_PHASE_ACCEPTED;
                (*response)->proposal_id = msg->proposal_id;
                (*response)->sequence_number = msg->sequence_number;
                (*response)->sender_id = node->node_id;
                (*response)->acceptor_id = node->node_id;
                
                pthread_mutex_unlock(&node->state_lock);
                return 0;
            }
            break;
            
        case PAXOS_PHASE_ACCEPTED:
            // Proposer receives ACCEPTED from acceptor
            for (size_t i = 0; i < node->max_proposals; i++) {
                pthread_mutex_lock(&node->active_proposals[i].lock);
                if (node->active_proposals[i].proposal_id == msg->proposal_id &&
                    !node->active_proposals[i].completed) {
                    node->active_proposals[i].accept_count++;
                    
                    if (paxos_check_quorum(node, node->active_proposals[i].accept_count)) {
                        // Value is committed!
                        node->active_proposals[i].committed = true;
                        node->active_proposals[i].completed = true;
                        
                        // Apply to state machine
                        if (node->apply_callback) {
                            node->apply_callback(msg->sequence_number,
                                               node->active_proposals[i].proposed_value,
                                               node->active_proposals[i].value_len);
                        }
                        
                        pthread_cond_broadcast(&node->active_proposals[i].quorum_reached);
                    }
                    pthread_mutex_unlock(&node->active_proposals[i].lock);
                    break;
                }
                pthread_mutex_unlock(&node->active_proposals[i].lock);
            }
            break;
            
        case PAXOS_PHASE_LEARN:
            // Learner receives committed value
            if (msg->sequence_number > node->highest_sequence_committed) {
                if (node->persist_callback) {
                    node->persist_callback(msg->sequence_number, msg->value, msg->value_len);
                }
                if (node->apply_callback) {
                    node->apply_callback(msg->sequence_number, msg->value, msg->value_len);
                }
                node->highest_sequence_committed = msg->sequence_number;
            }
            break;
    }
    
    pthread_mutex_unlock(&node->state_lock);
    return (*response != NULL) ? 0 : -1;
}

/* Serialize Paxos message */
int paxos_serialize_message(const paxos_message_t *msg, uint8_t **buffer, size_t *len) {
    if (!msg || !buffer || !len) {
        return -1;
    }
    
    // Calculate total size
    *len = sizeof(paxos_message_t) - sizeof(void*) + msg->value_len;
    *buffer = (uint8_t *)malloc(*len);
    if (!*buffer) {
        return -1;
    }
    
    uint8_t *ptr = *buffer;
    
    // Copy header fields
    memcpy(ptr, msg, sizeof(paxos_message_t) - sizeof(void*));
    ptr += sizeof(paxos_message_t) - sizeof(void*);
    
    // Copy value data
    if (msg->value && msg->value_len > 0) {
        memcpy(ptr, msg->value, msg->value_len);
    }
    
    return 0;
}

/* Deserialize Paxos message */
int paxos_deserialize_message(const uint8_t *buffer, size_t len, paxos_message_t *msg) {
    if (!buffer || !msg || len < (sizeof(paxos_message_t) - sizeof(void*))) {
        return -1;
    }
    
    const uint8_t *ptr = buffer;
    
    // Copy header fields
    memcpy(msg, ptr, sizeof(paxos_message_t) - sizeof(void*));
    ptr += sizeof(paxos_message_t) - sizeof(void*);
    
    // Copy value data
    if (msg->value_len > 0) {
        msg->value = malloc(msg->value_len);
        if (!msg->value) {
            return -1;
        }
        memcpy(msg->value, ptr, msg->value_len);
    } else {
        msg->value = NULL;
    }
    
    return 0;
}

/* Free Paxos message */
void paxos_free_message(paxos_message_t *msg) {
    if (msg && msg->value) {
        free(msg->value);
        msg->value = NULL;
    }
}
