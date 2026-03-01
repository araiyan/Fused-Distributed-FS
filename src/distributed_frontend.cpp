/**
 * @file distributed_frontend.cpp
 * @brief Distributed Filesystem Frontend Coordinator
 * 
 * This coordinator integrates:
 * - Metadata Cluster (Paxos-based consensus)
 * - Storage Nodes (gRPC file servers)
 * - Storage Interface (TCP protocol to storage nodes)
 */

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include "filesystem.grpc.pb.h"

extern "C" {
#include "../distributed_core/include/paxos.h"
#include "../distributed_core/include/metadata_manager.h"
#include "../distributed_core/include/network_engine.h"
#include "../distributed_core/include/storage_interface.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <uuid/uuid.h>
}

using fused::CreateRequest;
using fused::CreateResponse;
using fused::FileEntry;
using fused::FileSystemService;
using fused::GetRequest;
using fused::GetResponse;
using fused::MkdirRequest;
using fused::MkdirResponse;
using fused::ReadDirectoryRequest;
using fused::ReadDirectoryResponse;
using fused::WriteRequest;
using fused::WriteResponse;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;

// Global components
static metadata_manager_t *g_metadata = nullptr;
static paxos_node_t *g_paxos = nullptr;
static network_engine_t *g_network = nullptr;
static storage_interface_t *g_storage = nullptr;
static pthread_mutex_t g_coordinator_lock = PTHREAD_MUTEX_INITIALIZER;

/**
 * Generate unique file ID using UUID
 */
std::string generate_file_id(const std::string& path) {
    uuid_t uuid;
    uuid_generate(uuid);
    char uuid_str[37];
    uuid_unparse(uuid, uuid_str);
    return std::string(uuid_str);
}

/**
 * Normalize path by ensuring it starts with /
 */
std::string normalize_path(const std::string &path) {
    if (path.empty() || path[0] != '/') {
        return "/" + path;
    }
    return path;
}

/**
 * Paxos callbacks
 */
extern "C" {
    int persist_wal(uint64_t seq, void *value, size_t len) {
        printf("[Paxos] Persisting sequence %lu (%zu bytes)\n", seq, len);
        return 0;
    }

    void apply_state_machine(uint64_t seq, void *value, size_t len) {
        printf("[Paxos] Applying sequence %lu to state machine (%zu bytes)\n", seq, len);
        // In production, this would apply the metadata change
    }

    void handle_network_message(uint32_t sender_id, message_type_t type,
                               const uint8_t *payload, size_t payload_len, void *ctx) {
        printf("[Network] Message from node %u, type %d, length %zu\n",
               sender_id, type, payload_len);
        
        if (type == MSG_TYPE_PAXOS && ctx) {
            paxos_node_t *paxos = (paxos_node_t *)ctx;
            paxos_message_t msg;
            
            if (paxos_deserialize_message(payload, payload_len, &msg) == 0) {
                paxos_message_t *response = nullptr;
                
                if (paxos_handle_message(paxos, &msg, &response) == 0 && response) {
                    uint8_t *serialized = nullptr;
                    size_t len = 0;
                    
                    if (paxos_serialize_message(response, &serialized, &len) == 0) {
                        // Send response back to sender
                        network_engine_send(g_network, sender_id, MSG_TYPE_PAXOS, 
                                          serialized, len);
                        free(serialized);
                    }
                    
                    paxos_free_message(response);
                    free(response);
                }
                
                paxos_free_message(&msg);
            }
        }
    }
}

/**
 * Distributed Filesystem Service Implementation
 */
class DistributedFileSystemServiceImpl final : public FileSystemService::Service {
public:
    /**
     * Create - Create a new file
     */
    Status Create(ServerContext *context,
                  const CreateRequest *request,
                  CreateResponse *response) override {
        (void)context;

        std::string parent_path = normalize_path(request->pathname());
        std::string filename = request->filename();
        mode_t mode = static_cast<mode_t>(request->mode());

        // Build full path
        std::string full_path = parent_path;
        if (full_path.back() != '/') full_path += "/";
        full_path += filename;

        printf("[Frontend] Create: %s (mode=0%o)\n", full_path.c_str(), mode);

        pthread_mutex_lock(&g_coordinator_lock);

        // 1. Create metadata entry
        std::string file_id = generate_file_id(full_path);
        metadata_entry_t *entry = metadata_create_entry(
            g_metadata, full_path.c_str(), 
            S_IFREG | mode, getuid(), getgid());
        
        if (!entry) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-ENOMEM);
            response->set_error_message("Failed to create metadata entry");
            return Status::OK;
        }

        // Copy generated file_id
        strncpy(entry->file_id, file_id.c_str(), sizeof(entry->file_id) - 1);

        // 2. Select storage nodes (3 replicas by default)
        uint32_t selected_nodes[MAX_REPLICAS];
        int num_selected = storage_interface_select_nodes(
            g_storage, 0, MAX_REPLICAS, selected_nodes);
        
        if (num_selected <= 0) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-ENODEV);
            response->set_error_message("No available storage nodes");
            return Status::OK;
        }

        // 3. Assign storage nodes to metadata
        for (int i = 0; i < num_selected && i < MAX_STORAGE_NODES; i++) {
            storage_node_info_t *node = storage_interface_get_node(g_storage, selected_nodes[i]);
            if (node) {
                strncpy(entry->storage_node_ips[i], node->ip_address, 63);
                entry->storage_node_ports[i] = node->port;
                entry->storage_nodes[i] = selected_nodes[i]; // Store node ID
            }
        }
        entry->num_storage_nodes = num_selected;
        entry->num_replicas = num_selected;
        entry->state = FILE_STATE_ACTIVE;

        // 4. Propose to Paxos for consensus
        uint8_t *serialized = nullptr;
        size_t serialized_len = 0;
        
        if (metadata_serialize(entry, &serialized, &serialized_len) == 0) {
            int result = paxos_propose(g_paxos, serialized, serialized_len);
            free(serialized);
            
            if (result != 0) {
                pthread_mutex_unlock(&g_coordinator_lock);
                response->set_status_code(-EIO);
                response->set_error_message("Paxos consensus failed");
                return Status::OK;
            }
        }

        pthread_mutex_unlock(&g_coordinator_lock);

        response->set_status_code(0);
        printf("[Frontend] Create success: %s -> %s\n", full_path.c_str(), file_id.c_str());
        
        return Status::OK;
    }

    /**
     * Mkdir - Create a directory
     */
    Status Mkdir(ServerContext *context,
                 const MkdirRequest *request,
                 MkdirResponse *response) override {
        (void)context;

        std::string parent_path = normalize_path(request->pathname());
        std::string dirname = request->dirname();
        mode_t mode = static_cast<mode_t>(request->mode());

        // Build full path
        std::string full_path = parent_path;
        if (full_path.back() != '/') full_path += "/";
        full_path += dirname;

        printf("[Frontend] Mkdir: %s (mode=0%o)\n", full_path.c_str(), mode);

        pthread_mutex_lock(&g_coordinator_lock);

        // Create metadata entry for directory
        std::string dir_id = generate_file_id(full_path);
        metadata_entry_t *entry = metadata_create_entry(
            g_metadata, full_path.c_str(),
            S_IFDIR | mode, getuid(), getgid());
        
        if (!entry) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-ENOMEM);
            response->set_error_message("Failed to create metadata entry");
            return Status::OK;
        }

        strncpy(entry->file_id, dir_id.c_str(), sizeof(entry->file_id) - 1);
        entry->state = FILE_STATE_ACTIVE;

        // Propose to Paxos
        uint8_t *serialized = nullptr;
        size_t serialized_len = 0;
        
        if (metadata_serialize(entry, &serialized, &serialized_len) == 0) {
            paxos_propose(g_paxos, serialized, serialized_len);
            free(serialized);
        }

        pthread_mutex_unlock(&g_coordinator_lock);

        response->set_status_code(0);
        printf("[Frontend] Mkdir success: %s\n", full_path.c_str());
        
        return Status::OK;
    }

    /**
     * Write - Write data to file
     */
    Status Write(ServerContext *context,
                 const WriteRequest *request,
                 WriteResponse *response) override {
        (void)context;

        std::string path = normalize_path(request->pathname());
        const std::string &data = request->data();
        off_t offset = request->offset();

        printf("[Frontend] Write: path=%s, size=%zu, offset=%ld\n",
               path.c_str(), data.size(), offset);

        pthread_mutex_lock(&g_coordinator_lock);

        // 1. Lookup metadata
        metadata_entry_t *entry = metadata_lookup_by_path(g_metadata, path.c_str());
        if (!entry) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-ENOENT);
            response->set_error_message("File not found");
            response->set_bytes_written(0);
            return Status::OK;
        }

        // 2. Write to primary storage node
        if (entry->num_storage_nodes == 0) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-ENODEV);
            response->set_error_message("No storage nodes available");
            response->set_bytes_written(0);
            return Status::OK;
        }

        // Write to primary node (index 0)
        uint32_t primary_node = entry->storage_nodes[0];
        
        printf("[Frontend DEBUG] entry->num_storage_nodes=%u, storage_nodes[0]=%u, storage_nodes[1]=%u, storage_nodes[2]=%u\n",
               entry->num_storage_nodes, entry->storage_nodes[0], entry->storage_nodes[1], entry->storage_nodes[2]);
        
        if (primary_node == 0) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-ENODEV);
            response->set_error_message("Primary storage node not set");
            response->set_bytes_written(0);
            printf("[Frontend] ERROR: primary_node is 0 (not initialized)\n");
            return Status::OK;
        }
        
        printf("[Frontend] Writing to storage node %u (%s:%u)\n", 
               primary_node, entry->storage_node_ips[0], entry->storage_node_ports[0]);

        storage_response_t storage_resp;
        int result = storage_interface_write(
            g_storage, primary_node, entry->file_id,
            offset, (const uint8_t*)data.data(), data.size(),
            &storage_resp);

        if (result == 0 && storage_resp.status == 0) {
            // Update metadata size
            if (offset + data.size() > entry->size) {
                entry->size = offset + data.size();
                entry->modified_time = time(nullptr);
                metadata_update_entry(g_metadata, entry);
            }

            response->set_bytes_written(storage_resp.bytes_transferred);
            response->set_status_code(0);
            printf("[Frontend] Write success: %lu bytes\n", storage_resp.bytes_transferred);
        } else {
            response->set_bytes_written(0);
            response->set_status_code(storage_resp.status);
            response->set_error_message(storage_resp.error_msg);
            printf("[Frontend] Write failed: %s\n", storage_resp.error_msg);
        }

        pthread_mutex_unlock(&g_coordinator_lock);
        return Status::OK;
    }

    /**
     * Get - Read file contents
     */
    Status Get(ServerContext *context,
               const GetRequest *request,
               GetResponse *response) override {
        (void)context;

        std::string path = normalize_path(request->pathname());
        off_t offset = request->offset();
        size_t size = request->size();

        printf("[Frontend] Get: path=%s, offset=%ld, size=%zu\n",
               path.c_str(), offset, size);

        pthread_mutex_lock(&g_coordinator_lock);

        // 1. Lookup metadata
        metadata_entry_t *entry = metadata_lookup_by_path(g_metadata, path.c_str());
        if (!entry) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-ENOENT);
            response->set_error_message("File not found");
            response->set_bytes_read(0);
            return Status::OK;
        }

        // If size=0, read entire file
        if (size == 0) {
            size = (offset < (off_t)entry->size) ? (entry->size - offset) : 0;
        }

        // 2. Read from primary storage node
        if (entry->num_storage_nodes == 0) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-ENODEV);
            response->set_error_message("No storage nodes available");
            response->set_bytes_read(0);
            return Status::OK;
        }

        uint32_t primary_node = entry->storage_nodes[0];
        
        printf("[Frontend] Reading from storage node %u (%s:%u)\n", 
               primary_node, entry->storage_node_ips[0], entry->storage_node_ports[0]);

        storage_response_t storage_resp;
        int result = storage_interface_read(
            g_storage, primary_node, entry->file_id,
            offset, size, &storage_resp);

        if (result == 0 && storage_resp.status == 0) {
            response->set_data(storage_resp.data, storage_resp.data_len);
            response->set_bytes_read(storage_resp.data_len);
            response->set_status_code(0);
            printf("[Frontend] Get success: %zu bytes\n", storage_resp.data_len);
            
            // Free the data allocated by storage_interface_read
            if (storage_resp.data) {
                free(storage_resp.data);
            }
        } else {
            response->set_bytes_read(0);
            response->set_status_code(storage_resp.status);
            response->set_error_message(storage_resp.error_msg);
            printf("[Frontend] Get failed: %s\n", storage_resp.error_msg);
        }

        pthread_mutex_unlock(&g_coordinator_lock);
        return Status::OK;
    }

    /**
     * ReadDirectory - List directory contents
     */
    Status ReadDirectory(ServerContext *context,
                        const ReadDirectoryRequest *request,
                        ReadDirectoryResponse *response) override {
        (void)context;

        std::string path = normalize_path(request->pathname());

        printf("[Frontend] ReadDirectory: path=%s\n", path.c_str());

        pthread_mutex_lock(&g_coordinator_lock);

        // Lookup directory metadata
        metadata_entry_t *dir_entry = metadata_lookup_by_path(g_metadata, path.c_str());
        if (!dir_entry) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-ENOENT);
            response->set_error_message("Directory not found");
            return Status::OK;
        }

        if (!S_ISDIR(dir_entry->mode)) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-ENOTDIR);
            response->set_error_message("Not a directory");
            return Status::OK;
        }

        // List all entries with matching parent path
        // Note: This is a simplified implementation
        // In production, we'd maintain a parent-child relationship in metadata
        
        // For now, scan all metadata entries and filter by parent path
        // This is inefficient but works for demonstration
        int entry_count = 0;
        
        // Iterate through hash map (simplified - would need proper hash map iteration)
        // For now, just return success with empty listing
        
        response->set_status_code(0);
        printf("[Frontend] ReadDirectory success: %d entries\n", entry_count);

        pthread_mutex_unlock(&g_coordinator_lock);
        return Status::OK;
    }
};

/**
 * Signal handler for graceful shutdown
 */
static volatile int running = 1;
void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

/**
 * Initialize all distributed components
 */
bool initialize_distributed_system(uint32_t node_id, uint16_t listen_port, 
                                   uint32_t total_nodes, const char** peer_addrs,
                                   int num_peers) {
    
    printf("=== Initializing Distributed Frontend ===\n");
    printf("Node ID: %u\n", node_id);
    printf("Listen Port: %u\n", listen_port);
    printf("Total Nodes: %u\n", total_nodes);
    printf("==========================================\n\n");

    // 1. Initialize Paxos
    g_paxos = paxos_init(node_id, total_nodes, persist_wal, apply_state_machine);
    if (!g_paxos) {
        fprintf(stderr, "Failed to initialize Paxos\n");
        return false;
    }
    printf("[Paxos] Initialized (quorum: %u)\n", g_paxos->quorum_size);

    // 2. Initialize Metadata Manager
    char wal_path[256];
    snprintf(wal_path, sizeof(wal_path), "/tmp/frontend_wal_node%u.dat", node_id);
    g_metadata = metadata_manager_init(wal_path);
    if (!g_metadata) {
        fprintf(stderr, "Failed to initialize Metadata Manager\n");
        paxos_destroy(g_paxos);
        return false;
    }
    printf("[Metadata] Initialized (WAL: %s)\n", wal_path);

    // 3. Initialize Network Engine
    g_network = network_engine_init(node_id, listen_port, 
                                    handle_network_message, g_paxos);
    if (!g_network) {
        fprintf(stderr, "Failed to initialize Network Engine\n");
        metadata_manager_destroy(g_metadata);
        paxos_destroy(g_paxos);
        return false;
    }
    printf("[Network] Initialized\n");

    // 4. Start Network Engine
    if (network_engine_start(g_network) != 0) {
        fprintf(stderr, "Failed to start Network Engine\n");
        network_engine_destroy(g_network);
        metadata_manager_destroy(g_metadata);
        paxos_destroy(g_paxos);
        return false;
    }
    printf("[Network] Started on port %u\n", listen_port);

    // 5. Add peer metadata nodes
    for (int i = 0; i < num_peers; i++) {
        char peer_ip[64];
        uint16_t peer_port;
        uint32_t peer_id;
        
        if (sscanf(peer_addrs[i], "%u@%[^:]:%hu", &peer_id, peer_ip, &peer_port) == 3) {
            network_engine_add_peer(g_network, peer_id, peer_ip, peer_port);
            printf("[Network] Added peer %u at %s:%u\n", peer_id, peer_ip, peer_port);
        }
    }

    // 6. Initialize Storage Interface
    g_storage = storage_interface_init(MAX_STORAGE_NODES);
    if (!g_storage) {
        fprintf(stderr, "Failed to initialize Storage Interface\n");
        network_engine_destroy(g_network);
        metadata_manager_destroy(g_metadata);
        paxos_destroy(g_paxos);
        return false;
    }
    printf("[Storage] Initialized\n");

    // 7. Register storage nodes (from environment or config)
    // Default: 3 storage nodes
    const char* storage_nodes_env = getenv("STORAGE_NODES");
    if (storage_nodes_env) {
        char *nodes_copy = strdup(storage_nodes_env);
        char *token = strtok(nodes_copy, " ");
        uint32_t storage_id = 100;
        
        while (token) {
            char ip[64];
            uint16_t port;
            
            if (sscanf(token, "%[^:]:%hu", ip, &port) == 2) {
                storage_interface_register_node(g_storage, storage_id++, 
                                              ip, port, 1ULL * 1024 * 1024 * 1024); // 1GB
                printf("[Storage] Registered node %u at %s:%u\n", storage_id - 1, ip, port);
            }
            token = strtok(nullptr, " ");
        }
        free(nodes_copy);
    } else {
        // Default storage nodes (for docker-compose)
        storage_interface_register_node(g_storage, 101, "storage-node-1", 9000, 1ULL * 1024 * 1024 * 1024);
        storage_interface_register_node(g_storage, 102, "storage-node-2", 9000, 1ULL * 1024 * 1024 * 1024);
        storage_interface_register_node(g_storage, 103, "storage-node-3", 9000, 1ULL * 1024 * 1024 * 1024);
        printf("[Storage] Registered 3 default storage nodes\n");
    }

    printf("\n=== Initialization Complete ===\n\n");
    return true;
}

/**
 * Cleanup all components
 */
void cleanup_distributed_system() {
    printf("\n=== Shutting Down ===\n");
    
    if (g_storage) {
        storage_interface_destroy(g_storage);
        printf("[Storage] Destroyed\n");
    }
    if (g_network) {
        network_engine_stop(g_network);
        network_engine_destroy(g_network);
        printf("[Network] Destroyed\n");
    }
    if (g_metadata) {
        metadata_manager_destroy(g_metadata);
        printf("[Metadata] Destroyed\n");
    }
    if (g_paxos) {
        paxos_destroy(g_paxos);
        printf("[Paxos] Destroyed\n");
    }
    
    printf("=== Shutdown Complete ===\n");
}

/**
 * Run gRPC server
 */
void RunServer(const std::string &server_address) {
    DistributedFileSystemServiceImpl service;

    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();

    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);

    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "[Frontend] gRPC server listening on " << server_address << std::endl;

    // Keep server running
    while (running) {
        sleep(1);
    }
    
    server->Shutdown();
}

/**
 * Main entry point
 */
int main(int argc, char **argv) {
    // Disable output buffering
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);

    // Parse arguments
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <node_id> <listen_port> <total_nodes> [peer1 peer2 ...]\n", argv[0]);
        fprintf(stderr, "Example: %s 1 8001 3 2@metadata-node-2:7002 3@metadata-node-3:7003\n", argv[0]);
        return 1;
    }

    uint32_t node_id = atoi(argv[1]);
    uint16_t listen_port = atoi(argv[2]);
    uint32_t total_nodes = atoi(argv[3]);
    
    const char** peer_addrs = (const char**)(argv + 4);
    int num_peers = argc - 4;

    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Initialize distributed system components
    if (!initialize_distributed_system(node_id, listen_port, total_nodes, 
                                       peer_addrs, num_peers)) {
        fprintf(stderr, "Failed to initialize distributed system\n");
        return 1;
    }

    // Start gRPC server for client requests
    const char *grpc_port_env = getenv("FRONTEND_GRPC_PORT");
    std::string grpc_port = grpc_port_env ? grpc_port_env : "60051";
    std::string server_address = "0.0.0.0:" + grpc_port;

    RunServer(server_address);

    // Cleanup
    cleanup_distributed_system();

    return 0;
}
