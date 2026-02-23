#include "paxos.h"
#include "metadata_manager.h"
#include "network_engine.h"
#include "storage_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <inttypes.h>

static volatile int running = 1;

void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

/* Paxos callbacks */
int persist_wal(uint64_t seq, void *value, size_t len) {
    printf("[Paxos] Persisting sequence %" PRIu64 " (%zu bytes)\n", seq, len);
    return 0;
}

void apply_state_machine(uint64_t seq, void *value, size_t len) {
    printf("[Paxos] Applying to state machine: sequence %" PRIu64 " (%zu bytes)\n", seq, len);
}

/* Network message handler */
void handle_network_message(uint32_t sender_id, message_type_t type,
                           const uint8_t *payload, size_t payload_len, void *ctx) {
    printf("[Network] Received message from node %u, type %d, length %zu\n",
           sender_id, type, payload_len);
    
    // Example: Handle Paxos messages
    if (type == MSG_TYPE_PAXOS && ctx) {
        paxos_node_t *paxos = (paxos_node_t *)ctx;
        
        paxos_message_t msg;
        if (paxos_deserialize_message(payload, payload_len, &msg) == 0) {
            paxos_message_t *response = NULL;
            
            if (paxos_handle_message(paxos, &msg, &response) == 0 && response) {
                // Send response back (would use network_engine_send in real implementation)
                paxos_free_message(response);
                free(response);
            }
            
            paxos_free_message(&msg);
        }
    }
}

int main(int argc, char *argv[]) {
    // Disable output buffering for Docker logs
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <node_id> <listen_port> <total_nodes> [peer_ip:port ...]\n", argv[0]);
        return 1;
    }
    
    uint32_t node_id = atoi(argv[1]);
    uint16_t listen_port = atoi(argv[2]);
    uint32_t total_nodes = atoi(argv[3]);
    
    printf("=== Distributed Core Test Server ===\n");
    printf("Node ID: %u\n", node_id);
    printf("Listen Port: %u\n", listen_port);
    printf("Total Nodes: %u\n", total_nodes);
    printf("=====================================\n\n");
    
    // Initialize Paxos
    paxos_node_t *paxos = paxos_init(node_id, total_nodes, persist_wal, apply_state_machine);
    if (!paxos) {
        fprintf(stderr, "Failed to initialize Paxos node\n");
        return 1;
    }
    printf("[Paxos] Initialized (quorum size: %u)\n", paxos->quorum_size);
    
    // Initialize Metadata Manager
    char wal_path[256];
    snprintf(wal_path, sizeof(wal_path), "/tmp/metadata_wal_node%u.dat", node_id);
    metadata_manager_t *metadata = metadata_manager_init(wal_path);
    if (!metadata) {
        fprintf(stderr, "Failed to initialize metadata manager\n");
        paxos_destroy(paxos);
        return 1;
    }
    printf("[Metadata] Initialized (WAL: %s)\n", wal_path);
    
    // Initialize Network Engine
    network_engine_t *network = network_engine_init(node_id, listen_port,
                                                     handle_network_message, paxos);
    if (!network) {
        fprintf(stderr, "Failed to initialize network engine\n");
        metadata_manager_destroy(metadata);
        paxos_destroy(paxos);
        return 1;
    }
    printf("[Network] Initialized\n");
    
    // Start network engine
    if (network_engine_start(network) != 0) {
        fprintf(stderr, "Failed to start network engine\n");
        network_engine_destroy(network);
        metadata_manager_destroy(metadata);
        paxos_destroy(paxos);
        return 1;
    }
    printf("[Network] Started, listening on port %u\n", listen_port);
    
    // Add peers
    for (int i = 4; i < argc; i++) {
        char *peer_str = argv[i];
        char *colon = strchr(peer_str, ':');
        if (colon) {
            *colon = '\0';
            char *ip = peer_str;
            uint16_t port = atoi(colon + 1);
            uint32_t peer_id = i - 3; // Simple peer ID assignment
            
            network_engine_add_peer(network, peer_id, ip, port);
            printf("[Network] Added peer %u at %s:%u\n", peer_id, ip, port);
        }
    }
    
    // Initialize Storage Interface
    storage_interface_t *storage = storage_interface_init(16);
    if (!storage) {
        fprintf(stderr, "Failed to initialize storage interface\n");
        network_engine_destroy(network);
        metadata_manager_destroy(metadata);
        paxos_destroy(paxos);
        return 1;
    }
    printf("[Storage] Initialized\n");
    
    // Register example storage nodes (in production, these would be discovered)
    // storage_interface_register_node(storage, 100, "127.0.0.1", 9000, 1024*1024*1024);
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("\n=== Server Running ===\n");
    printf("Press Ctrl+C to stop\n\n");
    
    // Main loop
    while (running) {
        sleep(1);
        
        // Example: Periodic heartbeat or health checks
        // In production, implement proper heartbeat mechanism
    }
    
    printf("\n=== Shutting Down ===\n");
    
    // Cleanup
    storage_interface_destroy(storage);
    network_engine_stop(network);
    network_engine_destroy(network);
    metadata_manager_destroy(metadata);
    paxos_destroy(paxos);
    
    printf("Shutdown complete\n");
    
    return 0;
}
