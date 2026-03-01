/**
 * @file storage_tcp_adapter.cpp
 * @brief TCP adapter with proper gRPC client
 */

#include <grpcpp/grpcpp.h>
#include "filesystem.grpc.pb.h"

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
}

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;

#define STORAGE_PORT 9000
#define GRPC_SERVER "localhost:50051"
#define MAX_BUFFER 8192
#define MAX_FILE_ID 64

// Global gRPC client
class StorageClient {
private:
    std::unique_ptr<fused::FileSystemService::Stub> stub_;
    
public:
    StorageClient(std::shared_ptr<Channel> channel)
        : stub_(fused::FileSystemService::NewStub(channel)) {}
    
    int Write(const char* file_id, uint64_t offset, 
              const uint8_t* data, uint64_t length) {
        fused::WriteRequest req;
        req.set_pathname(file_id);
        req.set_data(data, length);
        req.set_offset(offset);
        
        fused::WriteResponse resp;
        ClientContext ctx;
        
        Status status = stub_->Write(&ctx, req, &resp);
        
        if (!status.ok() || resp.status_code() != 0) {
            fprintf(stderr, "[gRPC] Write failed: %s\n", 
                    resp.error_message().c_str());
            return -1;
        }
        
        return resp.bytes_written();
    }
    
    int Read(const char* file_id, uint64_t offset, uint64_t length,
             uint8_t** data_out) {
        fused::GetRequest req;
        req.set_pathname(file_id);
        req.set_offset(offset);
        req.set_size(length);
        
        fused::GetResponse resp;
        ClientContext ctx;
        
        Status status = stub_->Get(&ctx, req, &resp);
        
        if (!status.ok() || resp.status_code() != 0) {
            fprintf(stderr, "[gRPC] Read failed: %s\n",
                    resp.error_message().c_str());
            return -1;
        }
        
        *data_out = (uint8_t*)malloc(resp.bytes_read());
        memcpy(*data_out, resp.data().data(), resp.bytes_read());
        
        return resp.bytes_read();
    }
};

StorageClient* g_client = nullptr;

/**
 * Handle TCP client
 */
extern "C" void* handle_client(void* arg) {
    int client_fd = *(int*)arg;
    free(arg);
    
    fprintf(stderr, "[TCP Handler] Handling client fd=%d\n", client_fd);
    fflush(stderr);
    
    char buffer[MAX_BUFFER];
    ssize_t n = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
    if (n <= 0) {
        fprintf(stderr, "[TCP Handler] recv returned %zd\n", n);
        fflush(stderr);
        close(client_fd);
        return NULL;
    }
    buffer[n] = '\0';
    
    printf("[TCP] Received: %.*s", (int)strcspn(buffer, "\n"), buffer);
    fprintf(stderr, "[TCP Handler] Received %zd bytes: %.*s\n", n, (int)strcspn(buffer, "\n"), buffer);
    fflush(stderr);
    
    // Parse command
    if (strncmp(buffer, "WRITE|", 6) == 0) {
        char file_id[MAX_FILE_ID];
        uint64_t offset, length;
        
        // Find the end of the header line
        char* newline = strchr(buffer, '\n');
        if (!newline) {
            fprintf(stderr, "[TCP] No newline found in header\n");
            fflush(stderr);
            send(client_fd, "ERROR|Invalid header\n", 21, 0);
            close(client_fd);
            return NULL;
        }
        
        if (sscanf(buffer, "WRITE|%63[^|]|%lu|%lu\n", 
                   file_id, &offset, &length) == 3) {
            
            fprintf(stderr, "[TCP] Parsed: file_id=%s, offset=%lu, length=%lu\n", file_id, offset, length);
            fflush(stderr);
            
            // Calculate how much data was already received after the header
            size_t header_len = (newline - buffer) + 1;  // Include the newline
            size_t data_in_buffer = n - header_len;
            
            fprintf(stderr, "[TCP] header_len=%zu, data_in_buffer=%zu, total received=%zd\n", 
                    header_len, data_in_buffer, n);
            fflush(stderr);
            
            // Allocate buffer for full payload
            uint8_t* data = (uint8_t*)malloc(length);
            uint64_t total = 0;
            
            // Copy any data that was already received
            if (data_in_buffer > 0) {
                size_t to_copy = data_in_buffer < length ? data_in_buffer : length;
                memcpy(data, buffer + header_len, to_copy);
                total = to_copy;
                fprintf(stderr, "[TCP] Copied %zu bytes from initial buffer\n", to_copy);
                fflush(stderr);
            }
            
            // Receive remaining data
            while (total < length) {
                ssize_t r = recv(client_fd, data + total, length - total, 0);
                if (r <= 0) {
                    fprintf(stderr, "[TCP] recv failed while reading data: %zd\n", r);
                    fflush(stderr);
                    break;
                }
                total += r;
                fprintf(stderr, "[TCP] Received %zd more bytes, total=%lu/%lu\n", r, total, length);
                fflush(stderr);
            }
            
            printf(" → Write: %s, %lu bytes at offset %lu\n", 
                   file_id, total, offset);
            fprintf(stderr, "[TCP] Calling gRPC Write...\n");
            fflush(stderr);
            
            // Call gRPC
            int bytes_written = g_client->Write(file_id, offset, data, total);
            free(data);
            
            fprintf(stderr, "[TCP] gRPC Write returned %d\n", bytes_written);
            fflush(stderr);
            
            // Send response
            if (bytes_written > 0) {
                char resp[128];
                snprintf(resp, sizeof(resp), "OK|%d\n", bytes_written);
                send(client_fd, resp, strlen(resp), 0);
                printf("[TCP] Write OK: %d bytes\n", bytes_written);
                fprintf(stderr, "[TCP] Sent response: %s", resp);
                fflush(stderr);
            } else {
                send(client_fd, "ERROR|Write failed\n", 19, 0);
                printf("[TCP] Write FAILED\n");
                fprintf(stderr, "[TCP] Write failed, sent error response\n");
                fflush(stderr);
            }
        } else {
            fprintf(stderr, "[TCP] Failed to parse WRITE command\n");
            fflush(stderr);
            send(client_fd, "ERROR|Invalid WRITE syntax\n", 27, 0);
        }
    }
    else if (strncmp(buffer, "READ|", 5) == 0) {
        char file_id[MAX_FILE_ID];
        uint64_t offset, length;
        
        if (sscanf(buffer, "READ|%63[^|]|%lu|%lu\n", 
                   file_id, &offset, &length) == 3) {
            
            printf(" → Read: %s, %lu bytes at offset %lu\n",
                   file_id, offset, length);
            
            // Call gRPC
            uint8_t* data = nullptr;
            int bytes_read = g_client->Read(file_id, offset, length, &data);
            
            if (bytes_read > 0) {
                send(client_fd, data, bytes_read, 0);
                free(data);
                printf("[TCP] Read OK: %d bytes\n", bytes_read);
            } else if (bytes_read == 0) {
                // Empty read is OK
                printf("[TCP] Read OK: 0 bytes (EOF)\n");
            } else {
                printf("[TCP] Read FAILED\n");
            }
        }
    }
    else if (strncmp(buffer, "DELETE|", 7) == 0) {
        printf(" → Delete (not implemented)\n");
        send(client_fd, "ERROR|Delete not implemented\n", 30, 0);
    }
    else if (strncmp(buffer, "PING\n", 5) == 0) {
        printf(" → Ping\n");
        send(client_fd, "PONG\n", 5, 0);
    }
    else {
        printf(" → Unknown command\n");
        send(client_fd, "ERROR|Unknown command\n", 22, 0);
    }
    
    close(client_fd);
    return NULL;
}

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    
    const char* port_env = getenv("STORAGE_PORT");
    int port = port_env ? atoi(port_env) : STORAGE_PORT;
    
    const char* grpc_env = getenv("GRPC_SERVER");
    const char* grpc_addr = grpc_env ? grpc_env : GRPC_SERVER;
    
    printf("=========================================\n");
    printf(" TCP Storage Adapter\n");
    printf("=========================================\n");
    printf(" TCP Port:    %d\n", port);
    printf(" gRPC Server: %s\n", grpc_addr);
    printf("=========================================\n\n");
    
    // Initialize gRPC client
    g_client = new StorageClient(
        grpc::CreateChannel(grpc_addr, grpc::InsecureChannelCredentials())
    );
    
    // Create TCP socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket failed");
        return 1;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");
        close(server_fd);
        return 1;
    }
    
    if (listen(server_fd, 10) < 0) {
        perror("listen failed");
        close(server_fd);
        return 1;
    }
    
    printf("[TCP] Listening on 0.0.0.0:%d...\n\n", port);
    fprintf(stderr, "[TCP] TCP Adapter listening on 0.0.0.0:%d...\n", port);
    fflush(stdout);
    fflush(stderr);
    
    // Accept loop
    while (1) {
        int* client_fd = (int*)malloc(sizeof(int));
        *client_fd = accept(server_fd, NULL, NULL);
        
        fprintf(stderr, "[TCP] Accepted connection fd=%d\n", *client_fd);
        fflush(stderr);
        
        if (*client_fd < 0) {
            free(client_fd);
            continue;
        }
        
        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, client_fd);
        pthread_detach(tid);
    }
    
    close(server_fd);
    delete g_client;
    return 0;
}