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

