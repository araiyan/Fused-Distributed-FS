#include "paxos.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

static void paxos_release_proposal_slot(paxos_proposal_t *proposal) {
    if (!proposal) {
        return;
    }

    if (proposal->proposed_value) {
        free(proposal->proposed_value);
        proposal->proposed_value = NULL;
    }

    proposal->proposal_id = 0;
    proposal->sequence_number = 0;
    proposal->value_len = 0;
    proposal->promise_count = 0;
    proposal->accept_count = 0;
    proposal->reject_count = 0;
    proposal->in_use = false;
    proposal->completed = false;
    proposal->committed = false;
}

static int paxos_wait_for_condition(paxos_node_t *node,
                                    paxos_proposal_t *proposal,
                                    bool wait_for_commit) {
    if (!node || !proposal) {
        return -1;
    }

    struct timespec deadline;
    clock_gettime(CLOCK_REALTIME, &deadline);
    deadline.tv_sec += node->proposal_timeout_ms / 1000;
    deadline.tv_nsec += (long)(node->proposal_timeout_ms % 1000) * 1000000L;
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec += 1;
        deadline.tv_nsec -= 1000000000L;
    }

    int wait_status = 0;
    while (proposal->in_use) {
        bool satisfied = wait_for_commit
            ? proposal->committed
            : paxos_check_quorum(node, proposal->promise_count);

        if (satisfied) {
            return 0;
        }

        wait_status = pthread_cond_timedwait(&proposal->quorum_reached,
                                             &proposal->lock,
                                             &deadline);
        if (wait_status == ETIMEDOUT) {
            bool late_success = wait_for_commit
                ? proposal->committed
                : paxos_check_quorum(node, proposal->promise_count);
            return late_success ? 0 : -1;
        }

        if (wait_status != 0) {
            return -1;
        }
    }

    return -1;
}

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
    node->highest_sequence_persisted = 0;
    node->next_sequence_number = 1;
    node->last_accepted_proposal = 0;
    node->proposal_timeout_ms = 3000;
    
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
    node->broadcast_callback = NULL;
    node->broadcast_context = NULL;
    
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

    if (!node->broadcast_callback) {
        fprintf(stderr, "Paxos broadcast callback not configured\n");
        return -1;
    }
    
    // Generate unique proposal ID
    uint64_t proposal_id = paxos_next_proposal_id(node);
    
    pthread_mutex_lock(&node->state_lock);
    uint64_t sequence = node->next_sequence_number++;
    pthread_mutex_unlock(&node->state_lock);
    
    // Find free proposal slot
    paxos_proposal_t *proposal = NULL;
    for (size_t i = 0; i < node->max_proposals; i++) {
        pthread_mutex_lock(&node->active_proposals[i].lock);
        if (!node->active_proposals[i].in_use) {
            proposal = &node->active_proposals[i];
            proposal->in_use = true;
            proposal->completed = false;
            proposal->committed = false;
            proposal->proposal_id = proposal_id;
            proposal->sequence_number = sequence;
            proposal->proposed_value = malloc(value_len);
            if (!proposal->proposed_value) {
                proposal->in_use = false;
                pthread_mutex_unlock(&node->active_proposals[i].lock);
                return -1;
            }
            memcpy(proposal->proposed_value, value, value_len);
            proposal->value_len = value_len;
            proposal->promise_count = 1; // Count self
            proposal->accept_count = 0;
            proposal->reject_count = 0;
            pthread_mutex_unlock(&node->active_proposals[i].lock);
            break;
        }
        pthread_mutex_unlock(&node->active_proposals[i].lock);
    }
    
    if (!proposal) {
        fprintf(stderr, "No free proposal slots\n");
        return -1;
    }

    paxos_message_t prepare_msg;
    memset(&prepare_msg, 0, sizeof(prepare_msg));
    prepare_msg.phase = PAXOS_PHASE_PREPARE;
    prepare_msg.proposal_id = proposal_id;
    prepare_msg.sequence_number = sequence;
    prepare_msg.sender_id = node->node_id;

    if (node->broadcast_callback(&prepare_msg, node->broadcast_context) < 0) {
        pthread_mutex_lock(&proposal->lock);
        paxos_release_proposal_slot(proposal);
        pthread_mutex_unlock(&proposal->lock);
        return -1;
    }

    pthread_mutex_lock(&proposal->lock);
    if (paxos_wait_for_condition(node, proposal, false) != 0) {
        paxos_release_proposal_slot(proposal);
        pthread_mutex_unlock(&proposal->lock);
        return -1;
    }
    pthread_mutex_unlock(&proposal->lock);

    pthread_mutex_lock(&node->state_lock);
    if (proposal_id >= node->highest_proposal_seen) {
        node->highest_proposal_seen = proposal_id;
        node->last_accepted_proposal = proposal_id;
    }
    pthread_mutex_unlock(&node->state_lock);

    if (node->persist_callback && node->persist_callback(sequence, value, value_len) != 0) {
        pthread_mutex_lock(&proposal->lock);
        paxos_release_proposal_slot(proposal);
        pthread_mutex_unlock(&proposal->lock);
        return -1;
    }

    pthread_mutex_lock(&node->state_lock);
    if (sequence > node->highest_sequence_persisted) {
        node->highest_sequence_persisted = sequence;
    }
    pthread_mutex_unlock(&node->state_lock);

    pthread_mutex_lock(&proposal->lock);
    proposal->accept_count = 1; // Count self after local accept persistence
    pthread_mutex_unlock(&proposal->lock);

    paxos_message_t accept_msg;
    memset(&accept_msg, 0, sizeof(accept_msg));
    accept_msg.phase = PAXOS_PHASE_ACCEPT;
    accept_msg.proposal_id = proposal_id;
    accept_msg.sequence_number = sequence;
    accept_msg.sender_id = node->node_id;
    accept_msg.value = value;
    accept_msg.value_len = value_len;

    if (node->broadcast_callback(&accept_msg, node->broadcast_context) < 0) {
        pthread_mutex_lock(&proposal->lock);
        paxos_release_proposal_slot(proposal);
        pthread_mutex_unlock(&proposal->lock);
        return -1;
    }

    pthread_mutex_lock(&proposal->lock);
    int wait_rc = paxos_wait_for_condition(node, proposal, true);
    bool committed = proposal->committed;
    paxos_release_proposal_slot(proposal);
    pthread_mutex_unlock(&proposal->lock);

    return (wait_rc == 0 && committed) ? 0 : -1;
}

void paxos_set_broadcast_callback(paxos_node_t *node,
                                  paxos_broadcast_callback_t cb,
                                  void *ctx) {
    if (!node) {
        return;
    }

    pthread_mutex_lock(&node->state_lock);
    node->broadcast_callback = cb;
    node->broadcast_context = ctx;
    pthread_mutex_unlock(&node->state_lock);
}

void paxos_set_proposal_timeout(paxos_node_t *node, uint32_t timeout_ms) {
    if (!node || timeout_ms == 0) {
        return;
    }

    pthread_mutex_lock(&node->state_lock);
    node->proposal_timeout_ms = timeout_ms;
    pthread_mutex_unlock(&node->state_lock);
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
                    node->active_proposals[i].in_use &&
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
                if (node->persist_callback && msg->sequence_number > node->highest_sequence_persisted) {
                    if (node->persist_callback(msg->sequence_number, msg->value,
                                              msg->value_len) != 0) {
                        pthread_mutex_unlock(&node->state_lock);
                        return -1;
                    }
                    node->highest_sequence_persisted = msg->sequence_number;
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
                    node->active_proposals[i].in_use &&
                    !node->active_proposals[i].completed) {
                    node->active_proposals[i].accept_count++;
                    
                    if (paxos_check_quorum(node, node->active_proposals[i].accept_count)) {
                        // Value is committed!
                        node->active_proposals[i].committed = true;
                        node->active_proposals[i].completed = true;
                        if (msg->sequence_number > node->highest_sequence_committed) {
                            node->highest_sequence_committed = msg->sequence_number;
                        }
                        
                        // Apply to state machine
                        if (node->apply_callback) {
                            node->apply_callback(msg->sequence_number,
                                               node->active_proposals[i].proposed_value,
                                               node->active_proposals[i].value_len);
                        }

                        if (node->broadcast_callback) {
                            paxos_message_t learn_msg;
                            memset(&learn_msg, 0, sizeof(learn_msg));
                            learn_msg.phase = PAXOS_PHASE_LEARN;
                            learn_msg.proposal_id = node->active_proposals[i].proposal_id;
                            learn_msg.sequence_number = node->active_proposals[i].sequence_number;
                            learn_msg.sender_id = node->node_id;
                            learn_msg.value = node->active_proposals[i].proposed_value;
                            learn_msg.value_len = node->active_proposals[i].value_len;
                            learn_msg.accepted_id = node->active_proposals[i].proposal_id;
                            (void)node->broadcast_callback(&learn_msg, node->broadcast_context);
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
                if (node->persist_callback && msg->sequence_number > node->highest_sequence_persisted) {
                    node->persist_callback(msg->sequence_number, msg->value, msg->value_len);
                    node->highest_sequence_persisted = msg->sequence_number;
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
