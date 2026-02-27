/**
 * @file fused_rpc_server.cpp
 * @brief gRPC server implementation for network filesystem operations
 */

#include <grpcpp/grpcpp.h>
#include <grpcpp/health_check_service_interface.h>
#include <grpcpp/ext/proto_server_reflection_plugin.h>
#include "filesystem.grpc.pb.h"

extern "C"
{
#include "fused_fs.h"
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

/**
 * @brief Normalize path by removing /mnt/fused prefix if present
 */
std::string normalize_path(const std::string &path)
{
    const std::string mount_prefix = "/mnt/fused";
    if (path.find(mount_prefix) == 0)
    {
        std::string normalized = path.substr(mount_prefix.length());
        // If path was just "/mnt/fused", return "/"
        return normalized.empty() ? "/" : normalized;
    }
    return path;
}

class FileSystemServiceImpl final : public FileSystemService::Service
{
public:
    /**
     * Write - Append data to file
     */
    Status Write(ServerContext *context,
                 const WriteRequest *request,
                 WriteResponse *response) override
    {

        std::string path = normalize_path(request->pathname());
        const std::string &data = request->data();
        off_t offset = request->offset();

        log_message("RPC Write: path=%s, size=%zu, offset=%ld",
                    path.c_str(), data.size(), offset);

        // Look up inode to get file handle
        fused_inode_t *inode = path_to_inode(path.c_str());
        if (!inode)
        {
            response->set_status_code(-ENOENT);
            response->set_error_message("File not found");
            response->set_bytes_written(0);
            return Status::OK;
        }

        struct fuse_file_info fi;
        memset(&fi, 0, sizeof(fi));
        fi.fh = inode->ino;

        int result = fused_write(path.c_str(), data.c_str(),
                                 data.size(), offset, &fi);

        if (result < 0)
        {
            response->set_status_code(result);
            response->set_error_message(strerror(-result));
            response->set_bytes_written(0);
        }
        else
        {
            response->set_status_code(0);
            response->set_bytes_written(result);
            log_message("RPC Write success: %d bytes", result);
        }

        return Status::OK;
    }

    /**
     * Get - Read file contents
     */
    Status Get(ServerContext *context,
               const GetRequest *request,
               GetResponse *response) override
    {

        std::string path = normalize_path(request->pathname());
        off_t offset = request->offset();
        size_t size = request->size();

        log_message("RPC Get: path=%s, offset=%ld, size=%zu",
                    path.c_str(), offset, size);

        // Look up inode
        fused_inode_t *inode = path_to_inode(path.c_str());
        if (!inode)
        {
            response->set_status_code(-ENOENT);
            response->set_error_message("File not found");
            response->set_bytes_read(0);
            return Status::OK;
        }

        // If size=0, read entire file
        if (size == 0)
        {
            size = (offset < inode->size) ? (inode->size - offset) : 0;
        }

        // Allocate buffer
        std::vector<char> buffer(size);

        struct fuse_file_info fi;
        memset(&fi, 0, sizeof(fi));
        fi.fh = inode->ino;

        int result = fused_read(path.c_str(), buffer.data(),
                                size, offset, &fi);

        if (result < 0)
        {
            response->set_status_code(result);
            response->set_error_message(strerror(-result));
            response->set_bytes_read(0);
        }
        else
        {
            response->set_data(buffer.data(), result);
            response->set_bytes_read(result);
            response->set_status_code(0);
            log_message("RPC Get success: %d bytes", result);
        }
        return Status::OK;
    }

    /**
     * ReadDirectory - List directory contents
     */
    Status ReadDirectory(ServerContext *context,
                         const ReadDirectoryRequest *request,
                         ReadDirectoryResponse *response) override
    {

        std::string path = normalize_path(request->pathname());

        log_message("RPC ReadDirectory: path=%s", path.c_str());

        // Look up the directory
        fused_inode_t *dir = path_to_inode(path.c_str());
        if (!dir)
        {
            response->set_status_code(-ENOENT);
            response->set_error_message("Directory not found");
            return Status::OK;
        }

        // Ensure it's a directory
        if (!S_ISDIR(dir->mode))
        {
            response->set_status_code(-ENOTDIR);
            response->set_error_message("Not a directory");
            return Status::OK;
        }

        // Add all children to response
        for (int i = 0; i < dir->n_children; i++)
        {
            fused_inode_t *child = lookup_inode(dir->child_inodes[i]);
            if (!child)
                continue;

            FileEntry *entry = response->add_entries();
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
    Status Create(ServerContext *context,
                  const CreateRequest *request,
                  CreateResponse *response) override
    {
        (void)context;

        std::string parent_path = normalize_path(request->pathname());
        std::string filename = request->filename();
        mode_t mode = static_cast<mode_t>(request->mode());

        // Build full path
        std::string full_path = parent_path;
        if (full_path.back() != '/')
            full_path += "/";
        full_path += filename;

        log_message("RPC Create: %s (mode=0%o)", full_path.c_str(), mode);

        // Create file info struct
        struct fuse_file_info fi;
        memset(&fi, 0, sizeof(fi));
        fi.flags = O_CREAT | O_RDWR;

        int res = fused_create(full_path.c_str(), mode, &fi);
        response->set_status_code(res);
        if (res < 0)
        {
            response->set_error_message(strerror(-res));
        }

        return Status::OK;
    }

    /**
     * Mkdir - Create a directory
     */
    Status Mkdir(ServerContext *context,
                 const MkdirRequest *request,
                 MkdirResponse *response) override
    {
        (void)context;

        std::string parent_path = normalize_path(request->pathname());
        std::string dirname = request->dirname();
        mode_t mode = static_cast<mode_t>(request->mode());

        // Build full path
        std::string full_path = parent_path;
        if (full_path.back() != '/')
            full_path += "/";
        full_path += dirname;

        log_message("RPC Mkdir: %s (mode=0%o)", full_path.c_str(), mode);

        int res = fused_mkdir(full_path.c_str(), mode);
        response->set_status_code(res);
        if (res < 0)
        {
            response->set_error_message(strerror(-res));
        }

        return Status::OK;
    }
};

// ============================================================================
// Main Server
// ============================================================================
void RunServer(const std::string &server_address)
{
    // Initialize filesystem state (same as fused_init)
    g_state = (fused_state_t *)calloc(1, sizeof(fused_state_t));
    snprintf(g_state->backing_dir, MAX_PATH, "/tmp/fused_backing");
    mkdir(g_state->backing_dir, 0755);

    // Create root directory (same as init_root_inode)
    fused_inode_t *root = &g_state->inodes[0];
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

int main(int argc, char **argv)
{
    const char *port_env = getenv("RPC_PORT");
    std::string port = port_env ? port_env : "50051";
    std::string server_address = "0.0.0.0:" + port;

    RunServer(server_address);
    return 0;
}
