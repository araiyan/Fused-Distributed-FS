#include "storage_interface.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#define STORAGE_TIMEOUT_SEC 30

/* Initialize storage interface */
storage_interface_t *storage_interface_init(uint32_t max_nodes) {
    storage_interface_t *iface = (storage_interface_t *)calloc(1, sizeof(storage_interface_t));
    if (!iface) {
        return NULL;
    }
    
    iface->max_nodes = max_nodes;
    iface->nodes = (storage_node_info_t *)calloc(max_nodes, sizeof(storage_node_info_t));
    if (!iface->nodes) {
        free(iface);
        return NULL;
    }
    
    pthread_rwlock_init(&iface->nodes_lock, NULL);
    pthread_mutex_init(&iface->pool_lock, NULL);
    
    iface->num_nodes = 0;
    iface->next_node_idx = 0;
    iface->total_reads = 0;
    iface->total_writes = 0;
    iface->total_deletes = 0;
    iface->bytes_written = 0;
    iface->bytes_read = 0;
    
    return iface;
}

/* Destroy storage interface */
void storage_interface_destroy(storage_interface_t *iface) {
    if (!iface) return;
    
    pthread_rwlock_destroy(&iface->nodes_lock);
    pthread_mutex_destroy(&iface->pool_lock);
    
    if (iface->nodes) {
        free(iface->nodes);
    }
    
    free(iface);
}

/* Register storage node */
int storage_interface_register_node(storage_interface_t *iface, uint32_t node_id,
                                    const char *ip_address, uint16_t port,
                                    uint64_t capacity) {
    if (!iface || !ip_address) {
        return -1;
    }
    
    pthread_rwlock_wrlock(&iface->nodes_lock);
    
    if (iface->num_nodes >= iface->max_nodes) {
        pthread_rwlock_unlock(&iface->nodes_lock);
        return -1;
    }
    
    storage_node_info_t *node = &iface->nodes[iface->num_nodes];
    node->node_id = node_id;
    strncpy(node->ip_address, ip_address, sizeof(node->ip_address) - 1);
    node->port = port;
    node->capacity = capacity;
    node->used = 0;
    node->available = capacity;
    node->online = true;
    node->last_heartbeat = time(NULL);
    
    iface->num_nodes++;
    
    pthread_rwlock_unlock(&iface->nodes_lock);
    
    printf("Registered storage node %u at %s:%d (capacity: %" PRIu64 " bytes)\n",
           node_id, ip_address, port, capacity);
    
    return 0;
}

/* Unregister storage node */
int storage_interface_unregister_node(storage_interface_t *iface, uint32_t node_id) {
    if (!iface) {
        return -1;
    }
    
    pthread_rwlock_wrlock(&iface->nodes_lock);
    
    for (uint32_t i = 0; i < iface->num_nodes; i++) {
        if (iface->nodes[i].node_id == node_id) {
            // Shift remaining nodes
            for (uint32_t j = i; j < iface->num_nodes - 1; j++) {
                iface->nodes[j] = iface->nodes[j + 1];
            }
            iface->num_nodes--;
            
            pthread_rwlock_unlock(&iface->nodes_lock);
            return 0;
        }
    }
    
    pthread_rwlock_unlock(&iface->nodes_lock);
    return -1;
}

/* Update storage node status */
int storage_interface_update_node_status(storage_interface_t *iface, uint32_t node_id,
                                         uint64_t used, uint64_t available, bool online) {
    if (!iface) {
        return -1;
    }
    
    pthread_rwlock_wrlock(&iface->nodes_lock);
    
    for (uint32_t i = 0; i < iface->num_nodes; i++) {
        if (iface->nodes[i].node_id == node_id) {
            iface->nodes[i].used = used;
            iface->nodes[i].available = available;
            iface->nodes[i].online = online;
            iface->nodes[i].last_heartbeat = time(NULL);
            
            pthread_rwlock_unlock(&iface->nodes_lock);
            return 0;
        }
    }
    
    pthread_rwlock_unlock(&iface->nodes_lock);
    return -1;
}

/* Select storage nodes for new file (load balancing) */
int storage_interface_select_nodes(storage_interface_t *iface, uint64_t file_size,
                                   uint32_t num_replicas, uint32_t *selected_nodes) {
    if (!iface || !selected_nodes || num_replicas == 0) {
        return -1;
    }
    
    pthread_rwlock_rdlock(&iface->nodes_lock);
    
    uint32_t selected = 0;
    uint32_t attempts = 0;
    uint32_t max_attempts = iface->num_nodes * 2;
    
    while (selected < num_replicas && attempts < max_attempts) {
        uint32_t idx = iface->next_node_idx % iface->num_nodes;
        storage_node_info_t *node = &iface->nodes[idx];
        
        // Check if node is suitable
        if (node->online && node->available >= file_size) {
            // Check if not already selected
            bool already_selected = false;
            for (uint32_t i = 0; i < selected; i++) {
                if (selected_nodes[i] == node->node_id) {
                    already_selected = true;
                    break;
                }
            }
            
            if (!already_selected) {
                selected_nodes[selected] = node->node_id;
                selected++;
            }
        }
        
        iface->next_node_idx = (iface->next_node_idx + 1) % iface->num_nodes;
        attempts++;
    }
    
    pthread_rwlock_unlock(&iface->nodes_lock);
    
    return selected;
}

/* Helper: Connect to storage node */
static int connect_to_storage_node(const char *ip_address, uint16_t port) {
    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        perror("socket failed");
        return -1;
    }
    
    // Set socket options
    int optval = 1;
    setsockopt(sock_fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval));
    
    // Set timeout
    struct timeval timeout;
    timeout.tv_sec = STORAGE_TIMEOUT_SEC;
    timeout.tv_usec = 0;
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    
    // Connect
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip_address, &server_addr.sin_addr);
    
    if (connect(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect failed");
        close(sock_fd);
        return -1;
    }
    
    return sock_fd;
}

/* Write data to storage node */
int storage_interface_write(storage_interface_t *iface, uint32_t node_id,
                            const char *file_id, uint64_t offset,
                            const uint8_t *data, uint64_t length,
                            storage_response_t *response) {
    if (!iface || !file_id || !data || !response) {
        return -1;
    }
    
    memset(response, 0, sizeof(storage_response_t));
    
    // Find storage node
    storage_node_info_t *node = storage_interface_get_node(iface, node_id);
    if (!node) {
        response->status = -1;
        snprintf(response->error_msg, sizeof(response->error_msg), 
                "Storage node %u not found", node_id);
        return -1;
    }
    
    // Connect to storage node
    int sock_fd = connect_to_storage_node(node->ip_address, node->port);
    if (sock_fd < 0) {
        response->status = -1;
        snprintf(response->error_msg, sizeof(response->error_msg),
                "Failed to connect to storage node %u", node_id);
        return -1;
    }
    
    // Send write request (simple protocol: command | file_id | offset | length | data)
    // In production, use a proper protocol like gRPC or custom binary protocol
    char request_header[256];
    int header_len = snprintf(request_header, sizeof(request_header),
                             "WRITE|%s|%lu|%lu\n", file_id, offset, length);
    
    if (send(sock_fd, request_header, header_len, 0) < 0) {
        response->status = -1;
        snprintf(response->error_msg, sizeof(response->error_msg), "Send header failed");
        close(sock_fd);
        return -1;
    }
    
    // Send data
    uint64_t total_sent = 0;
    while (total_sent < length) {
        ssize_t sent = send(sock_fd, data + total_sent, length - total_sent, 0);
        if (sent < 0) {
            response->status = -1;
            snprintf(response->error_msg, sizeof(response->error_msg), "Send data failed");
            close(sock_fd);
            return -1;
        }
        total_sent += sent;
    }
    
    // Receive response
    char response_buffer[256];
    ssize_t recv_len = recv(sock_fd, response_buffer, sizeof(response_buffer) - 1, 0);
    if (recv_len < 0) {
        response->status = -1;
        snprintf(response->error_msg, sizeof(response->error_msg), "Receive response failed");
        close(sock_fd);
        return -1;
    }
    
    response_buffer[recv_len] = '\0';
    
    // Parse response (simple: "OK|bytes_written" or "ERROR|message")
    if (strncmp(response_buffer, "OK|", 3) == 0) {
        response->status = 0;
        response->bytes_transferred = total_sent;
        
        // Update statistics
        __sync_fetch_and_add(&iface->total_writes, 1);
        __sync_fetch_and_add(&iface->bytes_written, total_sent);
    } else {
        response->status = -1;
        strncpy(response->error_msg, response_buffer + 6, sizeof(response->error_msg) - 1);
    }
    
    close(sock_fd);
    return response->status;
}

/* Read data from storage node */
int storage_interface_read(storage_interface_t *iface, uint32_t node_id,
                           const char *file_id, uint64_t offset, uint64_t length,
                           storage_response_t *response) {
    if (!iface || !file_id || !response) {
        return -1;
    }
    
    memset(response, 0, sizeof(storage_response_t));
    
    // Find storage node
    storage_node_info_t *node = storage_interface_get_node(iface, node_id);
    if (!node) {
        response->status = -1;
        snprintf(response->error_msg, sizeof(response->error_msg),
                "Storage node %u not found", node_id);
        return -1;
    }
    
    // Connect to storage node
    int sock_fd = connect_to_storage_node(node->ip_address, node->port);
    if (sock_fd < 0) {
        response->status = -1;
        snprintf(response->error_msg, sizeof(response->error_msg),
                "Failed to connect to storage node %u", node_id);
        return -1;
    }
    
    // Send read request
    char request_header[256];
    int header_len = snprintf(request_header, sizeof(request_header),
                             "READ|%s|%lu|%lu\n", file_id, offset, length);
    
    if (send(sock_fd, request_header, header_len, 0) < 0) {
        response->status = -1;
        snprintf(response->error_msg, sizeof(response->error_msg), "Send request failed");
        close(sock_fd);
        return -1;
    }
    
    // Receive data
    response->data = (uint8_t *)malloc(length);
    if (!response->data) {
        response->status = -1;
        snprintf(response->error_msg, sizeof(response->error_msg), "Memory allocation failed");
        close(sock_fd);
        return -1;
    }
    
    uint64_t total_received = 0;
    while (total_received < length) {
        ssize_t received = recv(sock_fd, response->data + total_received,
                               length - total_received, 0);
        if (received <= 0) {
            if (received == 0) {
                break; // Connection closed
            }
            response->status = -1;
            snprintf(response->error_msg, sizeof(response->error_msg), "Receive data failed");
            free(response->data);
            response->data = NULL;
            close(sock_fd);
            return -1;
        }
        total_received += received;
    }
    
    response->status = 0;
    response->data_len = total_received;
    response->bytes_transferred = total_received;
    
    // Update statistics
    __sync_fetch_and_add(&iface->total_reads, 1);
    __sync_fetch_and_add(&iface->bytes_read, total_received);
    
    close(sock_fd);
    return 0;
}

/* Delete data from storage node */
int storage_interface_delete(storage_interface_t *iface, uint32_t node_id,
                             const char *file_id, storage_response_t *response) {
    if (!iface || !file_id || !response) {
        return -1;
    }
    
    memset(response, 0, sizeof(storage_response_t));
    
    // Find storage node
    storage_node_info_t *node = storage_interface_get_node(iface, node_id);
    if (!node) {
        response->status = -1;
        snprintf(response->error_msg, sizeof(response->error_msg),
                "Storage node %u not found", node_id);
        return -1;
    }
    
    // Connect and send delete request
    int sock_fd = connect_to_storage_node(node->ip_address, node->port);
    if (sock_fd < 0) {
        response->status = -1;
        snprintf(response->error_msg, sizeof(response->error_msg),
                "Failed to connect to storage node %u", node_id);
        return -1;
    }
    
    char request[256];
    int len = snprintf(request, sizeof(request), "DELETE|%s\n", file_id);
    
    if (send(sock_fd, request, len, 0) < 0) {
        response->status = -1;
        snprintf(response->error_msg, sizeof(response->error_msg), "Send request failed");
        close(sock_fd);
        return -1;
    }
    
    // Receive response
    char response_buffer[256];
    ssize_t recv_len = recv(sock_fd, response_buffer, sizeof(response_buffer) - 1, 0);
    if (recv_len > 0) {
        response_buffer[recv_len] = '\0';
        if (strncmp(response_buffer, "OK", 2) == 0) {
            response->status = 0;
            __sync_fetch_and_add(&iface->total_deletes, 1);
        } else {
            response->status = -1;
            strncpy(response->error_msg, response_buffer, sizeof(response->error_msg) - 1);
        }
    }
    
    close(sock_fd);
    return response->status;
}

/* Replicate data between storage nodes */
int storage_interface_replicate(storage_interface_t *iface, uint32_t source_node_id,
                                uint32_t target_node_id, const char *file_id,
                                storage_response_t *response) {
    if (!iface || !file_id || !response) {
        return -1;
    }
    
    // Read from source
    storage_response_t read_resp;
    if (storage_interface_read(iface, source_node_id, file_id, 0, UINT64_MAX, &read_resp) != 0) {
        *response = read_resp;
        return -1;
    }
    
    // Write to target
    int result = storage_interface_write(iface, target_node_id, file_id, 0,
                                        read_resp.data, read_resp.data_len, response);
    
    storage_response_free(&read_resp);
    return result;
}

/* Health check */
bool storage_interface_health_check(storage_interface_t *iface, uint32_t node_id) {
    if (!iface) {
        return false;
    }
    
    storage_node_info_t *node = storage_interface_get_node(iface, node_id);
    if (!node) {
        return false;
    }
    
    // Simple TCP connection test
    int sock_fd = connect_to_storage_node(node->ip_address, node->port);
    if (sock_fd < 0) {
        return false;
    }
    
    // Send ping
    const char *ping = "PING\n";
    send(sock_fd, ping, strlen(ping), 0);
    
    // Receive pong
    char buffer[64];
    ssize_t received = recv(sock_fd, buffer, sizeof(buffer) - 1, 0);
    
    close(sock_fd);
    
    return (received > 0 && strncmp(buffer, "PONG", 4) == 0);
}

/* Get storage node by ID */
storage_node_info_t *storage_interface_get_node(storage_interface_t *iface, uint32_t node_id) {
    if (!iface) {
        return NULL;
    }
    
    pthread_rwlock_rdlock(&iface->nodes_lock);
    
    for (uint32_t i = 0; i < iface->num_nodes; i++) {
        if (iface->nodes[i].node_id == node_id) {
            storage_node_info_t *node = &iface->nodes[i];
            pthread_rwlock_unlock(&iface->nodes_lock);
            return node;
        }
    }
    
    pthread_rwlock_unlock(&iface->nodes_lock);
    return NULL;
}

/* Get healthy storage nodes */
int storage_interface_get_healthy_nodes(storage_interface_t *iface,
                                        storage_node_info_t *nodes, uint32_t max_nodes) {
    if (!iface || !nodes) {
        return -1;
    }
    
    pthread_rwlock_rdlock(&iface->nodes_lock);
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < iface->num_nodes && count < max_nodes; i++) {
        if (iface->nodes[i].online) {
            nodes[count] = iface->nodes[i];
            count++;
        }
    }
    
    pthread_rwlock_unlock(&iface->nodes_lock);
    
    return count;
}

/* Free storage response */
void storage_response_free(storage_response_t *response) {
    if (response && response->data) {
        free(response->data);
        response->data = NULL;
        response->data_len = 0;
    }
}
