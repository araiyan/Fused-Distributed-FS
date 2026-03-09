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
#include <chrono>
#include <unordered_map>
#include <unordered_set>

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
using fused::RemoveRequest;
using fused::RemoveResponse;
using fused::ReadDirectoryRequest;
using fused::ReadDirectoryResponse;
using fused::RmdirRequest;
using fused::RmdirResponse;
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

typedef struct metadata_entry_node {
    char key[64];
    metadata_entry_t *entry;
    struct metadata_entry_node *next;
} metadata_entry_node_t;

#define FRONTEND_METADATA_HASH_MAP_SIZE 1024
typedef struct {
    metadata_entry_node_t *buckets[FRONTEND_METADATA_HASH_MAP_SIZE];
} metadata_hash_map_t;

std::string trim_trailing_slash(const std::string &path) {
    if (path == "/") {
        return path;
    }
    size_t end = path.size();
    while (end > 1 && path[end - 1] == '/') {
        end--;
    }
    return path.substr(0, end);
}

bool is_direct_child_path(const std::string &dir_path, const std::string &candidate,
                          std::string &child_name_out) {
    if (candidate.empty() || candidate == dir_path) {
        return false;
    }

    std::string normalized_dir = trim_trailing_slash(dir_path);
    std::string prefix = (normalized_dir == "/") ? "/" : (normalized_dir + "/");

    if (candidate.rfind(prefix, 0) != 0) {
        return false;
    }

    std::string remainder = candidate.substr(prefix.size());
    if (remainder.empty()) {
        return false;
    }

    if (remainder.find('/') != std::string::npos) {
        return false;
    }

    child_name_out = remainder;
    return true;
}

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

bool is_valid_name_component(const std::string &name) {
    if (name.empty() || name == "." || name == "..") {
        return false;
    }
    return name.find('/') == std::string::npos;
}

std::string join_path(const std::string &parent, const std::string &name) {
    std::string normalized_parent = trim_trailing_slash(normalize_path(parent));
    if (normalized_parent == "/") {
        return "/" + name;
    }
    return normalized_parent + "/" + name;
}

bool path_is_existing_directory(const std::string &path) {
    std::string normalized = trim_trailing_slash(normalize_path(path));
    if (normalized == "/") {
        return true;
    }

    metadata_entry_t *entry = metadata_lookup_by_path(g_metadata, normalized.c_str());
    return entry && entry->state != FILE_STATE_DELETED && S_ISDIR(entry->mode);
}

bool directory_has_children(const std::string &dir_path) {
    std::string normalized_dir = trim_trailing_slash(normalize_path(dir_path));
    metadata_hash_map_t *map = (metadata_hash_map_t *)g_metadata->hash_map;

    for (int i = 0; i < FRONTEND_METADATA_HASH_MAP_SIZE; i++) {
        metadata_entry_node_t *node = map->buckets[i];
        while (node) {
            metadata_entry_t *entry = node->entry;
            if (entry && entry->state != FILE_STATE_DELETED) {
                std::string child_name;
                std::string entry_path = trim_trailing_slash(entry->path);
                if (is_direct_child_path(normalized_dir, entry_path, child_name)) {
                    return true;
                }
            }
            node = node->next;
        }
    }

    return false;
}

bool commit_metadata_with_paxos(const metadata_entry_t *entry, const char *operation) {
    if (!entry || !g_metadata) {
        return false;
    }

    uint8_t *serialized = nullptr;
    size_t serialized_len = 0;
    if (metadata_serialize(entry, &serialized, &serialized_len) != 0) {
        return false;
    }

    int paxos_result = paxos_propose(g_paxos, serialized, serialized_len);
    free(serialized);

    if (paxos_result != 0) {
        printf("[Frontend] Paxos proposal failed during %s\n",
               operation ? operation : "operation");
        return false;
    }

    return true;
}

/**
 * Paxos callbacks
 */
extern "C" {
    int persist_wal(uint64_t seq, void *value, size_t len) {
        printf("[Paxos] Persisting sequence %lu (%zu bytes)\n", seq, len);
        if (!g_metadata || !value || len == 0) {
            return -1;
        }

        metadata_entry_t *entry = nullptr;
        if (metadata_deserialize((const uint8_t *)value, len, &entry) != 0 || !entry) {
            return -1;
        }

        uint64_t lsn = wal_append(g_metadata, WAL_OP_UPDATE, entry);
        pthread_rwlock_destroy(&entry->lock);
        free(entry);

        return (lsn == UINT64_MAX) ? -1 : 0;
    }

    int paxos_broadcast_message(const paxos_message_t *msg, void *ctx) {
        if (!msg || !ctx) {
            return -1;
        }

        network_engine_t *network = (network_engine_t *)ctx;
        uint8_t *serialized = nullptr;
        size_t serialized_len = 0;

        if (paxos_serialize_message(msg, &serialized, &serialized_len) != 0) {
            return -1;
        }

        int sent = network_engine_broadcast(network, MSG_TYPE_PAXOS, serialized, serialized_len);
        free(serialized);

        return (sent < 0) ? -1 : 0;
    }

    void apply_state_machine(uint64_t seq, void *value, size_t len) {
        printf("[Paxos] Applying sequence %lu to state machine (%zu bytes)\n", seq, len);
        if (!g_metadata || !value || len == 0) {
            return;
        }

        metadata_entry_t *incoming = nullptr;
        if (metadata_deserialize((const uint8_t *)value, len, &incoming) != 0 || !incoming) {
            return;
        }

        metadata_apply_entry(g_metadata, incoming);

        pthread_rwlock_destroy(&incoming->lock);
        free(incoming);
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

        std::string parent_path = trim_trailing_slash(normalize_path(request->pathname()));
        std::string filename = request->filename();
        mode_t mode = static_cast<mode_t>(request->mode());

        if (!is_valid_name_component(filename)) {
            response->set_status_code(-EINVAL);
            response->set_error_message("Invalid filename");
            return Status::OK;
        }

        if (!path_is_existing_directory(parent_path)) {
            response->set_status_code(-ENOENT);
            response->set_error_message("Parent directory not found");
            return Status::OK;
        }

        // Build full path
        std::string full_path = join_path(parent_path, filename);

        metadata_entry_t *existing = metadata_lookup_by_path(g_metadata, full_path.c_str());
        if (existing && existing->state != FILE_STATE_DELETED) {
            response->set_status_code(-EEXIST);
            response->set_error_message("File already exists");
            return Status::OK;
        }

        printf("[Frontend] Create: %s (mode=0%o)\n", full_path.c_str(), mode);

        pthread_mutex_lock(&g_coordinator_lock);

        // 1. Build metadata entry for consensus proposal
        std::string file_id = generate_file_id(full_path);
        metadata_entry_t proposed_entry;
        memset(&proposed_entry, 0, sizeof(proposed_entry));
        strncpy(proposed_entry.file_id, file_id.c_str(), sizeof(proposed_entry.file_id) - 1);
        strncpy(proposed_entry.path, full_path.c_str(), sizeof(proposed_entry.path) - 1);
        proposed_entry.state = FILE_STATE_ACTIVE;
        proposed_entry.size = 0;
        proposed_entry.mode = S_IFREG | mode;
        proposed_entry.uid = getuid();
        proposed_entry.gid = getgid();
        proposed_entry.created_time = time(nullptr);
        proposed_entry.modified_time = proposed_entry.created_time;
        proposed_entry.accessed_time = proposed_entry.created_time;
        proposed_entry.version = 1;
        proposed_entry.stripe_size = 4194304;

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
                strncpy(proposed_entry.storage_node_ips[i], node->ip_address, 63);
                proposed_entry.storage_node_ports[i] = node->port;
                proposed_entry.storage_nodes[i] = selected_nodes[i]; // Store node ID
            }
        }
        proposed_entry.num_storage_nodes = num_selected;
        proposed_entry.num_replicas = num_selected;
        proposed_entry.primary_node_idx = 0;

        // 4. Propose to Paxos for consensus
        if (!commit_metadata_with_paxos(&proposed_entry, "create")) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-EIO);
            response->set_error_message("Failed to commit metadata");
            return Status::OK;
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

        std::string parent_path = trim_trailing_slash(normalize_path(request->pathname()));
        std::string dirname = request->dirname();
        mode_t mode = static_cast<mode_t>(request->mode());

        if (!is_valid_name_component(dirname)) {
            response->set_status_code(-EINVAL);
            response->set_error_message("Invalid directory name");
            return Status::OK;
        }

        if (!path_is_existing_directory(parent_path)) {
            response->set_status_code(-ENOENT);
            response->set_error_message("Parent directory not found");
            return Status::OK;
        }

        // Build full path
        std::string full_path = join_path(parent_path, dirname);

        metadata_entry_t *existing = metadata_lookup_by_path(g_metadata, full_path.c_str());
        if (existing && existing->state != FILE_STATE_DELETED) {
            response->set_status_code(-EEXIST);
            response->set_error_message("Directory already exists");
            return Status::OK;
        }

        printf("[Frontend] Mkdir: %s (mode=0%o)\n", full_path.c_str(), mode);

        pthread_mutex_lock(&g_coordinator_lock);

        // Build metadata entry for directory
        std::string dir_id = generate_file_id(full_path);
        metadata_entry_t proposed_entry;
        memset(&proposed_entry, 0, sizeof(proposed_entry));
        strncpy(proposed_entry.file_id, dir_id.c_str(), sizeof(proposed_entry.file_id) - 1);
        strncpy(proposed_entry.path, full_path.c_str(), sizeof(proposed_entry.path) - 1);
        proposed_entry.mode = S_IFDIR | mode;
        proposed_entry.uid = getuid();
        proposed_entry.gid = getgid();
        proposed_entry.state = FILE_STATE_ACTIVE;
        proposed_entry.created_time = time(nullptr);
        proposed_entry.modified_time = proposed_entry.created_time;
        proposed_entry.accessed_time = proposed_entry.created_time;
        proposed_entry.version = 1;
        proposed_entry.stripe_size = 4194304;

        // Propose to Paxos
        if (!commit_metadata_with_paxos(&proposed_entry, "mkdir")) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-EIO);
            response->set_error_message("Failed to commit metadata");
            return Status::OK;
        }

        pthread_mutex_unlock(&g_coordinator_lock);

        response->set_status_code(0);
        printf("[Frontend] Mkdir success: %s\n", full_path.c_str());
        
        return Status::OK;
    }

    /**
     * Remove - Delete a file
     */
    Status Remove(ServerContext *context,
                  const RemoveRequest *request,
                  RemoveResponse *response) override {
        (void)context;

        std::string path = trim_trailing_slash(normalize_path(request->pathname()));
        if (path == "/") {
            response->set_status_code(-EINVAL);
            response->set_error_message("Cannot remove root path");
            return Status::OK;
        }

        printf("[Frontend] Remove: path=%s\n", path.c_str());

        pthread_mutex_lock(&g_coordinator_lock);

        metadata_entry_t *entry = metadata_lookup_by_path(g_metadata, path.c_str());
        if (!entry || entry->state == FILE_STATE_DELETED) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-ENOENT);
            response->set_error_message("File not found");
            return Status::OK;
        }

        if (S_ISDIR(entry->mode)) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-EISDIR);
            response->set_error_message("Path is a directory; use rmdir");
            return Status::OK;
        }

        if (entry->num_storage_nodes > 0) {
            uint32_t quorum_required = (entry->num_storage_nodes / 2) + 1;
            uint32_t success_count = 0;

            for (uint32_t i = 0; i < entry->num_storage_nodes; i++) {
                uint32_t node_id = entry->storage_nodes[i];
                if (node_id == 0) {
                    continue;
                }

                storage_response_t del_resp{};
                int del_result = storage_interface_delete(g_storage, node_id, entry->file_id, &del_resp);
                if (del_result == 0 && del_resp.status == 0) {
                    success_count++;
                }
            }

            if (success_count < quorum_required) {
                pthread_mutex_unlock(&g_coordinator_lock);
                response->set_status_code(-EIO);
                response->set_error_message("Delete quorum not reached");
                printf("[Frontend] Remove failed: quorum %u/%u\n", success_count, entry->num_storage_nodes);
                return Status::OK;
            }
        }

        metadata_entry_t proposed_entry;
        memset(&proposed_entry, 0, sizeof(proposed_entry));
        size_t copy_len = sizeof(metadata_entry_t) - sizeof(pthread_rwlock_t);
        memcpy(&proposed_entry, entry, copy_len);
        proposed_entry.state = FILE_STATE_DELETED;
        proposed_entry.modified_time = time(nullptr);
        proposed_entry.version = entry->version + 1;

        if (!commit_metadata_with_paxos(&proposed_entry, "remove")) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-EIO);
            response->set_error_message("Failed to commit metadata");
            return Status::OK;
        }

        pthread_mutex_unlock(&g_coordinator_lock);
        response->set_status_code(0);
        printf("[Frontend] Remove success: %s\n", path.c_str());
        return Status::OK;
    }

    /**
     * Rmdir - Delete an empty directory
     */
    Status Rmdir(ServerContext *context,
                 const RmdirRequest *request,
                 RmdirResponse *response) override {
        (void)context;

        std::string path = trim_trailing_slash(normalize_path(request->pathname()));
        if (path == "/") {
            response->set_status_code(-EINVAL);
            response->set_error_message("Cannot remove root directory");
            return Status::OK;
        }

        printf("[Frontend] Rmdir: path=%s\n", path.c_str());

        pthread_mutex_lock(&g_coordinator_lock);

        metadata_entry_t *entry = metadata_lookup_by_path(g_metadata, path.c_str());
        if (!entry || entry->state == FILE_STATE_DELETED) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-ENOENT);
            response->set_error_message("Directory not found");
            return Status::OK;
        }

        if (!S_ISDIR(entry->mode)) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-ENOTDIR);
            response->set_error_message("Path is not a directory");
            return Status::OK;
        }

        if (directory_has_children(path)) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-ENOTEMPTY);
            response->set_error_message("Directory is not empty");
            return Status::OK;
        }

        metadata_entry_t proposed_entry;
        memset(&proposed_entry, 0, sizeof(proposed_entry));
        size_t copy_len = sizeof(metadata_entry_t) - sizeof(pthread_rwlock_t);
        memcpy(&proposed_entry, entry, copy_len);
        proposed_entry.state = FILE_STATE_DELETED;
        proposed_entry.modified_time = time(nullptr);
        proposed_entry.version = entry->version + 1;

        if (!commit_metadata_with_paxos(&proposed_entry, "rmdir")) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-EIO);
            response->set_error_message("Failed to commit metadata");
            return Status::OK;
        }

        pthread_mutex_unlock(&g_coordinator_lock);
        response->set_status_code(0);
        printf("[Frontend] Rmdir success: %s\n", path.c_str());
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

        // 2. Write to storage replicas and require quorum success
        if (entry->num_storage_nodes == 0) {
            pthread_mutex_unlock(&g_coordinator_lock);
            response->set_status_code(-ENODEV);
            response->set_error_message("No storage nodes available");
            response->set_bytes_written(0);
            return Status::OK;
        }

        uint32_t quorum_required = (entry->num_storage_nodes / 2) + 1;
        uint32_t success_count = 0;
        uint64_t bytes_written = 0;
        uint64_t original_size = entry->size;
        off_t write_offset = offset;

        if (write_offset == 0 && entry->size > 0) {
            write_offset = entry->size;
        }

        for (uint32_t i = 0; i < entry->num_storage_nodes; i++) {
            uint32_t node_id = entry->storage_nodes[i];
            if (node_id == 0) {
                continue;
            }

            storage_response_t replica_resp;
            int write_result = storage_interface_write(
                g_storage, node_id, entry->file_id,
                write_offset, (const uint8_t *)data.data(), data.size(),
                &replica_resp);

            if (write_result == 0 && replica_resp.status == 0) {
                success_count++;
                bytes_written = replica_resp.bytes_transferred;
            }
        }

        if (success_count >= quorum_required) {
            metadata_entry_t proposed_entry;
            memset(&proposed_entry, 0, sizeof(proposed_entry));
            size_t copy_len = sizeof(metadata_entry_t) - sizeof(pthread_rwlock_t);
            memcpy(&proposed_entry, entry, copy_len);

            if (write_offset + data.size() > proposed_entry.size) {
                proposed_entry.size = write_offset + data.size();
            }
            proposed_entry.modified_time = time(nullptr);
            proposed_entry.version = entry->version + 1;

            bool metadata_committed = commit_metadata_with_paxos(&proposed_entry, "write");

            if (!metadata_committed) {
                bool rollback_safe = (original_size == 0 && write_offset == 0);
                if (rollback_safe) {
                    for (uint32_t i = 0; i < entry->num_storage_nodes; i++) {
                        uint32_t node_id = entry->storage_nodes[i];
                        if (node_id == 0) {
                            continue;
                        }
                        storage_response_t cleanup_resp{};
                        (void)storage_interface_delete(g_storage, node_id, entry->file_id, &cleanup_resp);
                    }
                    printf("[Frontend] Write rollback attempted on replicas for %s\n", path.c_str());
                }

                response->set_bytes_written(0);
                response->set_status_code(-EIO);
                response->set_error_message(
                    rollback_safe
                        ? "Write quorum reached but metadata consensus failed (rollback attempted)"
                        : "Write quorum reached but metadata consensus failed");
                printf("[Frontend] Write failed: metadata consensus failure\n");
                pthread_mutex_unlock(&g_coordinator_lock);
                return Status::OK;
            }

            response->set_bytes_written(bytes_written);
            response->set_status_code(0);
            printf("[Frontend] Write success: quorum %u/%u\n", success_count, entry->num_storage_nodes);
        } else {
            response->set_bytes_written(0);
            response->set_status_code(-EIO);
            response->set_error_message("Write quorum not reached");
            printf("[Frontend] Write failed: quorum %u/%u\n", success_count, entry->num_storage_nodes);
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

            bool no_forward = (context->client_metadata().find("x-no-forward") !=
                               context->client_metadata().end());

            if (!no_forward) {
                int self_id = 0;
                const char *node_id_env = getenv("NODE_ID");
                if (node_id_env) {
                    self_id = atoi(node_id_env);
                }

                int total_nodes = 3;
                const char *total_nodes_env = getenv("TOTAL_NODES");
                if (total_nodes_env) {
                    int parsed_total_nodes = atoi(total_nodes_env);
                    if (parsed_total_nodes > 0) {
                        total_nodes = parsed_total_nodes;
                    }
                }

                for (int peer_id = 1; peer_id <= total_nodes; peer_id++) {
                    if (peer_id == self_id) {
                        continue;
                    }

                    std::string peer_addr = "frontend-" + std::to_string(peer_id) +
                                            ":" + std::to_string(60050 + peer_id);

                    auto channel = grpc::CreateChannel(peer_addr,
                        grpc::InsecureChannelCredentials());
                    auto stub = FileSystemService::NewStub(channel);

                    GetRequest forward_req;
                    forward_req.set_pathname(path);
                    forward_req.set_offset(offset);
                    forward_req.set_size(request->size());

                    GetResponse forward_resp;
                    grpc::ClientContext forward_ctx;
                    forward_ctx.AddMetadata("x-no-forward", "1");
                    forward_ctx.set_deadline(std::chrono::system_clock::now() +
                                             std::chrono::milliseconds(1500));

                    grpc::Status peer_status = stub->Get(&forward_ctx, forward_req, &forward_resp);
                    if (peer_status.ok() && forward_resp.status_code() == 0) {
                        response->CopyFrom(forward_resp);
                        printf("[Frontend] Get served via peer frontend-%d\n", peer_id);
                        return Status::OK;
                    }
                }
            }

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

        uint32_t quorum_required = (entry->num_storage_nodes / 2) + 1;
        std::unordered_map<std::string, uint32_t> value_counts;
        std::string quorum_value;
        bool quorum_found = false;

        for (uint32_t i = 0; i < entry->num_storage_nodes; i++) {
            uint32_t node_id = entry->storage_nodes[i];
            if (node_id == 0) {
                continue;
            }

            storage_response_t replica_resp{};
            int read_result = storage_interface_read(
                g_storage, node_id, entry->file_id,
                offset, size, &replica_resp);

            if (read_result == 0 && replica_resp.status == 0 && replica_resp.data) {
                std::string payload((const char *)replica_resp.data, replica_resp.data_len);
                uint32_t count = ++value_counts[payload];
                if (count >= quorum_required) {
                    quorum_value = payload;
                    quorum_found = true;
                }
            }

            if (replica_resp.data) {
                free(replica_resp.data);
            }

            if (quorum_found) {
                break;
            }
        }

        if (quorum_found) {
            response->set_data(quorum_value.data(), quorum_value.size());
            response->set_bytes_read(quorum_value.size());
            response->set_status_code(0);
            printf("[Frontend] Get success: quorum %u/%u\n", quorum_required, entry->num_storage_nodes);
        } else {
            response->set_bytes_read(0);
            response->set_status_code(-EIO);
            response->set_error_message("Read quorum not reached");
            printf("[Frontend] Get failed: read quorum not reached\n");
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

        std::string path = trim_trailing_slash(normalize_path(request->pathname()));

        printf("[Frontend] ReadDirectory: path=%s\n", path.c_str());

        pthread_mutex_lock(&g_coordinator_lock);

        std::string dir_path = trim_trailing_slash(path);

        if (dir_path != "/") {
            metadata_entry_t *dir_entry = metadata_lookup_by_path(g_metadata, dir_path.c_str());
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
        }

        metadata_hash_map_t *map = (metadata_hash_map_t *)g_metadata->hash_map;
        std::unordered_set<std::string> seen_entries;
        int entry_count = 0;

        for (int i = 0; i < FRONTEND_METADATA_HASH_MAP_SIZE; i++) {
            metadata_entry_node_t *node = map->buckets[i];
            while (node) {
                metadata_entry_t *entry = node->entry;
                if (entry && entry->state != FILE_STATE_DELETED) {
                    std::string child_name;
                    std::string entry_path = trim_trailing_slash(entry->path);
                    if (is_direct_child_path(dir_path, entry_path, child_name) &&
                        seen_entries.insert(child_name).second) {
                        FileEntry *resp_entry = response->add_entries();
                        resp_entry->set_name(child_name);
                        resp_entry->set_is_directory(S_ISDIR(entry->mode));
                        resp_entry->set_size(entry->size);
                        resp_entry->set_mtime(entry->modified_time);
                        entry_count++;
                    }
                }
                node = node->next;
            }
        }

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

    paxos_set_broadcast_callback(g_paxos, paxos_broadcast_message, g_network);

    const char *proposal_timeout_env = getenv("PAXOS_PROPOSAL_TIMEOUT_MS");
    uint32_t proposal_timeout_ms = 8000;
    if (proposal_timeout_env) {
        long parsed_timeout = strtol(proposal_timeout_env, nullptr, 10);
        if (parsed_timeout >= 1000 && parsed_timeout <= 60000) {
            proposal_timeout_ms = (uint32_t)parsed_timeout;
        } else {
            printf("[Paxos] Ignoring invalid PAXOS_PROPOSAL_TIMEOUT_MS=%s (using %u ms)\n",
                   proposal_timeout_env, proposal_timeout_ms);
        }
    }
    paxos_set_proposal_timeout(g_paxos, proposal_timeout_ms);
    printf("[Paxos] Proposal timeout set to %u ms\n", proposal_timeout_ms);

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
