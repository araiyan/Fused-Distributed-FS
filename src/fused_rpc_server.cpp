/**
 * @file fused_rpc_server.cpp
 * @brief gRPC server implementation for network filesystem operations
 */

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include "filesystem.grpc.pb.h"

extern "C" {
#include "fused_fs.h"
}

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using fused::FileSystemService;
using fused::CreateRequest;
using fused::CreateResponse;
using fused::MkdirRequest;
using fused::MkdirResponse;
using fused::WriteRequest;
using fused::WriteResponse;
using fused::GetRequest;
using fused::GetResponse;
using fused::ReadDirectoryRequest;
using fused::ReadDirectoryResponse;
using fused::FileEntry;

class FileSystemServiceImpl final : public FileSystemService::Service {
public:

    /**
     * Write - Append data to file
     */
    Status Write(ServerContext* context,
                 const WriteRequest* request,
                 WriteResponse* response) override {
        
        const std::string& path = request->pathname();
        const std::string& data = request->data();
        off_t offset = request->offset();
        
        log_message("RPC Write: path=%s, size=%zu, offset=%ld",
                    path.c_str(), data.size(), offset);
        
        // Look up the file
        fused_inode_t* inode = path_to_inode(path.c_str());
        if (!inode) {
            response->set_status_code(-ENOENT);
            response->set_error_message("File not found");
            response->set_bytes_written(0);
            return Status::OK;
        }
        
        // Ensure it's a regular file
        if (S_ISDIR(inode->mode)) {
            response->set_status_code(-EISDIR);
            response->set_error_message("Is a directory");
            response->set_bytes_written(0);
            return Status::OK;
        }
        
        // Enforce append-only: offset must be at EOF
        if (offset < inode->size) {
            response->set_status_code(-EPERM);
            response->set_error_message("Append-only: cannot write before EOF");
            response->set_bytes_written(0);
            return Status::OK;
        }
        
        // Open backing file for append
        FILE* fp = fopen(inode->backing_path, "ab");
        if (!fp) {
            response->set_status_code(-EIO);
            response->set_error_message("Failed to open backing file");
            response->set_bytes_written(0);
            return Status::OK;
        }
        
        // Handle gap with zeros if offset > current size
        if (offset > inode->size) {
            size_t gap = offset - inode->size;
            char zero_buf[4096] = {0};
            while (gap > 0) {
                size_t chunk = (gap > 4096) ? 4096 : gap;
                fwrite(zero_buf, 1, chunk, fp);
                gap -= chunk;
            }
        }
        
        // Write the data
        size_t bytes_written = fwrite(data.data(), 1, data.size(), fp);
        fclose(fp);
        
        if (bytes_written != data.size()) {
            response->set_status_code(-EIO);
            response->set_error_message("Partial write");
            response->set_bytes_written(bytes_written);
            return Status::OK;
        }
        
        // Update inode metadata
        inode->size = offset + bytes_written;
        inode->mtime = time(NULL);
        inode->ctime = time(NULL);
        
        response->set_status_code(0);
        response->set_bytes_written(bytes_written);
        
        log_message("RPC Write success: %zu bytes", bytes_written);
        return Status::OK;
    }
    
    /**
     * Get - Read file contents
     */
    Status Get(ServerContext* context,
               const GetRequest* request,
               GetResponse* response) override {
        
        const std::string& path = request->pathname();
        off_t offset = request->offset();
        size_t size = request->size();
        
        log_message("RPC Get: path=%s, offset=%ld, size=%zu",
                    path.c_str(), offset, size);
        
        // Look up the file
        fused_inode_t* inode = path_to_inode(path.c_str());
        if (!inode) {
            response->set_status_code(-ENOENT);
            response->set_error_message("File not found");
            response->set_bytes_read(0);
            return Status::OK;
        }
        
        // Ensure it's a regular file
        if (S_ISDIR(inode->mode)) {
            response->set_status_code(-EISDIR);
            response->set_error_message("Is a directory");
            response->set_bytes_read(0);
            return Status::OK;
        }
        
        // If size=0, read entire file
        if (size == 0) {
            if (offset >= inode->size) {
                size = 0;
            } else {
                size = inode->size - offset;
            }
        }
        
        // Check bounds
        if (offset >= inode->size) {
            response->set_status_code(0);
            response->set_bytes_read(0);
            return Status::OK;
        }
        
        // Adjust size if reading past EOF
        if (offset + size > (size_t)inode->size) {
            size = inode->size - offset;
        }
        
        // Read from backing file
        FILE* fp = fopen(inode->backing_path, "rb");
        if (!fp) {
            response->set_status_code(-EIO);
            response->set_error_message("Failed to open file");
            response->set_bytes_read(0);
            return Status::OK;
        }
        
        fseek(fp, offset, SEEK_SET);
        
        std::vector<char> buffer(size);
        size_t bytes_read = fread(buffer.data(), 1, size, fp);
        fclose(fp);
        
        // Update access time
        inode->atime = time(NULL);
        
        response->set_data(buffer.data(), bytes_read);
        response->set_bytes_read(bytes_read);
        response->set_status_code(0);
        
        log_message("RPC Get success: %zu bytes", bytes_read);
        return Status::OK;
    }
    
    /**
     * ReadDirectory - List directory contents
     */
    Status ReadDirectory(ServerContext* context,
                         const ReadDirectoryRequest* request,
                         ReadDirectoryResponse* response) override {
        
        const std::string& path = request->pathname();
        
        log_message("RPC ReadDirectory: path=%s", path.c_str());
        
        // Look up the directory
        fused_inode_t* dir = path_to_inode(path.c_str());
        if (!dir) {
            response->set_status_code(-ENOENT);
            response->set_error_message("Directory not found");
            return Status::OK;
        }
        
        // Ensure it's a directory
        if (!S_ISDIR(dir->mode)) {
            response->set_status_code(-ENOTDIR);
            response->set_error_message("Not a directory");
            return Status::OK;
        }
        
        // Add all children to response
        for (int i = 0; i < dir->n_children; i++) {
            fused_inode_t* child = lookup_inode(dir->child_inodes[i]);
            if (!child) continue;
            
            FileEntry* entry = response->add_entries();
            entry->set_name(dir->child_names[i]);
            entry->set_is_directory(S_ISDIR(child->mode));
            entry->set_size(child->size);
            entry->set_mtime(child->mtime);
        }
        
        response->set_status_code(0);
        
        log_message("RPC ReadDirectory success: %d entries", dir->n_children);
        return Status::OK;
    }
    
    /**
     * Create - Create a new file
     */
    Status Create(ServerContext* context,
                  const CreateRequest* request,
                  CreateResponse* response) override {
        (void)context;
		(void)request;
		
		response->set_status_code(-ENOSYS);
		response->set_error_message("Create not yet implemented");
		
		return Status::OK;
    }
    
    /**
     * Mkdir - Create a directory
     */
    Status Mkdir(ServerContext* context,
                 const MkdirRequest* request,
                 MkdirResponse* response) override {
		(void)context;
		(void)request;
		
		response->set_status_code(-ENOSYS);
		response->set_error_message("Mkdir not yet implemented");
		
		return Status::OK;
    }
};

// ============================================================================
// Main Server
// ============================================================================
void RunServer(const std::string& server_address) {
    // Initialize filesystem state (same as fused_init)
    g_state = (fused_state_t*)calloc(1, sizeof(fused_state_t));
    snprintf(g_state->backing_dir, MAX_PATH, "/tmp/fused_backing");
    mkdir(g_state->backing_dir, 0755);
    
    // Create root directory (same as init_root_inode)
    fused_inode_t* root = &g_state->inodes[0];
    root->ino = FUSE_ROOT_ID;
    root->mode = S_IFDIR | 0755;
    root->uid = getuid();
    root->gid = getgid();
    root->size = 4096;
    root->atime = root->mtime = root->ctime = time(NULL);
    root->n_children = 0;
    g_state->n_inodes = 1;
    
    log_message("Filesystem initialized");
    
    // Start gRPC server
    FileSystemServiceImpl service;
    
    grpc::EnableDefaultHealthCheckService(true);
    grpc::reflection::InitProtoReflectionServerBuilderPlugin();
    
    ServerBuilder builder;
    builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
    builder.RegisterService(&service);
    
    std::unique_ptr<Server> server(builder.BuildAndStart());
    std::cout << "Server listening on " << server_address << std::endl;
    
    server->Wait();
}

int main(int argc, char** argv) {
    const char* port_env = getenv("RPC_PORT");
    std::string port = port_env ? port_env : "50051";
    std::string server_address = "0.0.0.0:" + port;
    
    RunServer(server_address);
    return 0;
}
