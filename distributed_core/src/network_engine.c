#include "network_engine.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <poll.h>

#define MAGIC_NUMBER 0xFEEDFACE
#define MAX_EVENTS 64
#define MAX_POLL_FDS 128

static void network_remove_inbound_fd(network_engine_t *engine, int fd) {
    if (!engine || fd < 0) {
        return;
    }

    pthread_mutex_lock(&engine->inbound_lock);
    for (uint32_t i = 0; i < engine->num_inbound_fds; i++) {
        if (engine->inbound_fds[i] == fd) {
            for (uint32_t j = i; j + 1 < engine->num_inbound_fds; j++) {
                engine->inbound_fds[j] = engine->inbound_fds[j + 1];
            }
            engine->num_inbound_fds--;
            break;
        }
    }
    pthread_mutex_unlock(&engine->inbound_lock);
}

static int network_connect_peer(peer_connection_t *peer) {
    if (!peer) {
        return -1;
    }

    int sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0) {
        return -1;
    }

    struct sockaddr_in peer_addr;
    memset(&peer_addr, 0, sizeof(peer_addr));
    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(peer->port);

    int resolved = 0;
    struct addrinfo hints;
    struct addrinfo *result = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int gai_result = getaddrinfo(peer->ip_address, NULL, &hints, &result);
    if (gai_result == 0 && result) {
        struct sockaddr_in *resolved_addr = (struct sockaddr_in *)result->ai_addr;
        peer_addr.sin_addr = resolved_addr->sin_addr;
        resolved = 1;
        freeaddrinfo(result);
    } else if (inet_pton(AF_INET, peer->ip_address, &peer_addr.sin_addr) == 1) {
        resolved = 1;
    }

    if (!resolved) {
        close(sock_fd);
        return -1;
    }

    if (connect(sock_fd, (struct sockaddr *)&peer_addr, sizeof(peer_addr)) != 0) {
        close(sock_fd);
        return -1;
    }

    network_set_nonblocking(sock_fd);
    network_set_socket_options(sock_fd);

    peer->socket_fd = sock_fd;
    peer->connected = true;
    peer->last_heartbeat = time(NULL);

    return 0;
}

/* Set socket to non-blocking mode */
int network_set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        return -1;
    }
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

/* Set TCP socket options */
int network_set_socket_options(int fd) {
    int optval = 1;
    
    // Enable TCP keepalive
    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &optval, sizeof(optval)) < 0) {
        perror("setsockopt SO_KEEPALIVE failed");
        return -1;
    }
    
    // Disable Nagle's algorithm for low latency
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &optval, sizeof(optval)) < 0) {
        perror("setsockopt TCP_NODELAY failed");
        return -1;
    }
    
    // Allow address reuse
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        return -1;
    }
    
    return 0;
}

/* Event loop thread */
static void *network_event_loop(void *arg) {
    network_engine_t *engine = (network_engine_t *)arg;
    uint8_t recv_buffer[RECV_BUFFER_SIZE];
    struct pollfd poll_fds[MAX_POLL_FDS];
    
    while (engine->running) {
        int nfds = 0;
        
        // Add listen socket
        poll_fds[nfds].fd = engine->listen_fd;
        poll_fds[nfds].events = POLLIN;
        nfds++;
        
        // Add peer sockets
        pthread_rwlock_rdlock(&engine->peers_lock);
        for (uint32_t i = 0; i < engine->num_peers && nfds < MAX_POLL_FDS; i++) {
            if (engine->peers[i].connected && engine->peers[i].socket_fd >= 0) {
                poll_fds[nfds].fd = engine->peers[i].socket_fd;
                poll_fds[nfds].events = POLLIN;
                nfds++;
            }
        }
        pthread_rwlock_unlock(&engine->peers_lock);

        // Add inbound accepted sockets
        pthread_mutex_lock(&engine->inbound_lock);
        for (uint32_t i = 0; i < engine->num_inbound_fds && nfds < MAX_POLL_FDS; i++) {
            if (engine->inbound_fds[i] >= 0) {
                poll_fds[nfds].fd = engine->inbound_fds[i];
                poll_fds[nfds].events = POLLIN;
                nfds++;
            }
        }
        pthread_mutex_unlock(&engine->inbound_lock);
        
        // Poll with 1 second timeout
        int ret = poll(poll_fds, nfds, 1000);
        
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("poll failed");
            break;
        }
        
        if (ret == 0) continue; // Timeout
        
        // Check listen socket
        if (poll_fds[0].revents & POLLIN) {
            // Accept new connection
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            int client_fd = accept(engine->listen_fd, 
                                  (struct sockaddr *)&client_addr, &client_len);
            
            if (client_fd < 0) {
                perror("accept failed");
            } else {
                network_set_nonblocking(client_fd);
                network_set_socket_options(client_fd);

                pthread_mutex_lock(&engine->inbound_lock);
                if (engine->num_inbound_fds < MAX_PENDING_CONNECTIONS) {
                    engine->inbound_fds[engine->num_inbound_fds++] = client_fd;
                } else {
                    close(client_fd);
                    client_fd = -1;
                }
                pthread_mutex_unlock(&engine->inbound_lock);
                
                if (client_fd >= 0) {
                    printf("Accepted connection from %s:%d\n",
                           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
                }
            }
        }
        
        // Check peer sockets
        for (int i = 1; i < nfds; i++) {
            if (poll_fds[i].revents & (POLLIN | POLLERR | POLLHUP)) {
                int client_fd = poll_fds[i].fd;
                ssize_t bytes_read = recv(client_fd, recv_buffer, RECV_BUFFER_SIZE, 0);
                
                if (bytes_read <= 0) {
                    if (bytes_read == 0 || (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                        // Connection closed or error
                        close(client_fd);

                        network_remove_inbound_fd(engine, client_fd);

                        // Mark peer as disconnected
                        pthread_rwlock_wrlock(&engine->peers_lock);
                        for (uint32_t j = 0; j < engine->num_peers; j++) {
                            if (engine->peers[j].socket_fd == client_fd) {
                                engine->peers[j].connected = false;
                                engine->peers[j].socket_fd = -1;
                                break;
                            }
                        }
                        pthread_rwlock_unlock(&engine->peers_lock);
                    }
                    continue;
                }
                
                // Deserialize message
                network_message_t msg;
                if (network_deserialize_message(recv_buffer, bytes_read, &msg) == 0) {
                    // Call message handler
                    if (engine->message_handler) {
                        engine->message_handler(msg.header.sender_id, 
                                              msg.header.type,
                                              msg.payload,
                                              msg.header.payload_len,
                                              engine->handler_context);
                    }
                    
                    engine->messages_received++;
                    engine->bytes_received += bytes_read;
                    
                    network_free_message(&msg);
                }
            }
        }
    }
    
    return NULL;
}

/* Initialize network engine */
network_engine_t *network_engine_init(uint32_t node_id, uint16_t listen_port,
                                      void (*handler)(uint32_t, message_type_t,
                                                     const uint8_t*, size_t, void*),
                                      void *ctx) {
    network_engine_t *engine = (network_engine_t *)calloc(1, sizeof(network_engine_t));
    if (!engine) {
        return NULL;
    }
    
    engine->node_id = node_id;
    engine->listen_port = listen_port;
    engine->listen_fd = -1;
    engine->num_peers = 0;
    engine->running = false;
    engine->message_handler = handler;
    engine->handler_context = ctx;
    engine->num_inbound_fds = 0;
    
    pthread_rwlock_init(&engine->peers_lock, NULL);
    pthread_mutex_init(&engine->inbound_lock, NULL);
    
    for (int i = 0; i < MAX_PEERS; i++) {
        engine->peers[i].connected = false;
        engine->peers[i].socket_fd = -1;
        pthread_mutex_init(&engine->peers[i].send_lock, NULL);
    }
    
    return engine;
}

/* Destroy network engine */
void network_engine_destroy(network_engine_t *engine) {
    if (!engine) return;
    
    if (engine->running) {
        network_engine_stop(engine);
    }
    
    if (engine->listen_fd >= 0) {
        close(engine->listen_fd);
    }
    
    for (int i = 0; i < MAX_PEERS; i++) {
        if (engine->peers[i].socket_fd >= 0) {
            close(engine->peers[i].socket_fd);
        }
        pthread_mutex_destroy(&engine->peers[i].send_lock);
    }

    pthread_mutex_lock(&engine->inbound_lock);
    for (uint32_t i = 0; i < engine->num_inbound_fds; i++) {
        if (engine->inbound_fds[i] >= 0) {
            close(engine->inbound_fds[i]);
        }
    }
    pthread_mutex_unlock(&engine->inbound_lock);
    
    pthread_rwlock_destroy(&engine->peers_lock);
    pthread_mutex_destroy(&engine->inbound_lock);
    
    free(engine);
}

/* Start network engine */
int network_engine_start(network_engine_t *engine) {
    if (!engine || engine->running) {
        return -1;
    }
    
    // Create listening socket
    engine->listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (engine->listen_fd < 0) {
        perror("socket failed");
        return -1;
    }
    
    network_set_nonblocking(engine->listen_fd);
    network_set_socket_options(engine->listen_fd);
    
    // Bind to port
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(engine->listen_port);
    
    if (bind(engine->listen_fd, (struct sockaddr *)&server_addr, 
            sizeof(server_addr)) < 0) {
        perror("bind failed");
        close(engine->listen_fd);
        return -1;
    }
    
    // Listen for connections
    if (listen(engine->listen_fd, MAX_PENDING_CONNECTIONS) < 0) {
        perror("listen failed");
        close(engine->listen_fd);
        return -1;
    }
    
    // Start event loop thread
    engine->running = true;
    if (pthread_create(&engine->event_thread, NULL, network_event_loop, engine) != 0) {
        perror("pthread_create failed");
        engine->running = false;
        close(engine->listen_fd);
        return -1;
    }
    
    printf("Network engine listening on port %d\n", engine->listen_port);
    
    return 0;
}

/* Stop network engine */
void network_engine_stop(network_engine_t *engine) {
    if (!engine || !engine->running) {
        return;
    }
    
    engine->running = false;
    pthread_join(engine->event_thread, NULL);
}

/* Add peer */
int network_engine_add_peer(network_engine_t *engine, uint32_t node_id,
                            const char *ip_address, uint16_t port) {
    if (!engine || !ip_address) {
        return -1;
    }
    
    pthread_rwlock_wrlock(&engine->peers_lock);
    
    if (engine->num_peers >= MAX_PEERS) {
        pthread_rwlock_unlock(&engine->peers_lock);
        return -1;
    }
    
    peer_connection_t *peer = &engine->peers[engine->num_peers];
    peer->node_id = node_id;
    strncpy(peer->ip_address, ip_address, sizeof(peer->ip_address) - 1);
    peer->port = port;
    peer->connected = false;
    peer->socket_fd = -1;
    
    // Attempt to connect
    if (network_connect_peer(peer) == 0) {
        printf("Connected to peer %u at %s:%d\n", node_id, ip_address, port);
    }
    
    engine->num_peers++;
    pthread_rwlock_unlock(&engine->peers_lock);
    
    return 0;
}

/* Send message to peer */
int network_engine_send(network_engine_t *engine, uint32_t peer_id,
                       message_type_t type, const uint8_t *payload, size_t payload_len) {
    if (!engine || !payload) {
        return -1;
    }
    
    pthread_rwlock_wrlock(&engine->peers_lock);
    
    peer_connection_t *peer = NULL;
    for (uint32_t i = 0; i < engine->num_peers; i++) {
        if (engine->peers[i].node_id == peer_id) {
            peer = &engine->peers[i];
            break;
        }
    }
    
    if (!peer) {
        pthread_rwlock_unlock(&engine->peers_lock);
        return -1;
    }

    if (!peer->connected || peer->socket_fd < 0) {
        if (network_connect_peer(peer) != 0) {
            pthread_rwlock_unlock(&engine->peers_lock);
            return -1;
        }
    }

    if (!peer->connected) {
        pthread_rwlock_unlock(&engine->peers_lock);
        return -1;
    }
    
    // Create message
    network_message_t msg;
    msg.header.magic = MAGIC_NUMBER;
    msg.header.type = type;
    msg.header.sender_id = engine->node_id;
    static uint32_t sequence = 0;
    msg.header.sequence = __sync_fetch_and_add(&sequence, 1);
    msg.header.payload_len = payload_len;
    msg.payload = (uint8_t *)payload;
    
    // Calculate checksum (simple sum for now)
    uint32_t checksum = 0;
    for (size_t i = 0; i < payload_len; i++) {
        checksum += payload[i];
    }
    msg.header.checksum = checksum;
    
    // Serialize
    uint8_t *buffer;
    size_t buffer_len;
    if (network_serialize_message(&msg, &buffer, &buffer_len) != 0) {
        pthread_rwlock_unlock(&engine->peers_lock);
        return -1;
    }
    
    // Send
    pthread_mutex_lock(&peer->send_lock);
    ssize_t sent = send(peer->socket_fd, buffer, buffer_len, 0);
    pthread_mutex_unlock(&peer->send_lock);
    
    free(buffer);
    
    pthread_rwlock_unlock(&engine->peers_lock);
    
    if (sent < 0) {
        perror("send failed");
        pthread_rwlock_wrlock(&engine->peers_lock);
        if (peer->socket_fd >= 0) {
            close(peer->socket_fd);
        }
        peer->socket_fd = -1;
        peer->connected = false;
        pthread_rwlock_unlock(&engine->peers_lock);
        return -1;
    }
    
    engine->messages_sent++;
    engine->bytes_sent += sent;
    
    return 0;
}

/* Broadcast to all peers */
int network_engine_broadcast(network_engine_t *engine, message_type_t type,
                            const uint8_t *payload, size_t payload_len) {
    if (!engine) {
        return -1;
    }
    
    int success_count = 0;

    uint32_t peer_ids[MAX_PEERS];
    uint32_t peer_count = 0;

    pthread_rwlock_rdlock(&engine->peers_lock);
    for (uint32_t i = 0; i < engine->num_peers && i < MAX_PEERS; i++) {
        peer_ids[peer_count++] = engine->peers[i].node_id;
    }
    pthread_rwlock_unlock(&engine->peers_lock);

    // Always attempt send to every configured peer. network_engine_send() will
    // reconnect on demand if the peer socket is currently down.
    for (uint32_t i = 0; i < peer_count; i++) {
        if (network_engine_send(engine, peer_ids[i], type, payload, payload_len) == 0) {
            success_count++;
        }
    }
    
    return success_count;
}

/* Serialize message */
int network_serialize_message(const network_message_t *msg, uint8_t **buffer, size_t *len) {
    if (!msg || !buffer || !len) {
        return -1;
    }
    
    *len = sizeof(message_header_t) + msg->header.payload_len;
    *buffer = (uint8_t *)malloc(*len);
    if (!*buffer) {
        return -1;
    }
    
    memcpy(*buffer, &msg->header, sizeof(message_header_t));
    if (msg->header.payload_len > 0) {
        memcpy(*buffer + sizeof(message_header_t), msg->payload, msg->header.payload_len);
    }
    
    return 0;
}

/* Deserialize message */
int network_deserialize_message(const uint8_t *buffer, size_t len, network_message_t *msg) {
    if (!buffer || !msg || len < sizeof(message_header_t)) {
        return -1;
    }
    
    memcpy(&msg->header, buffer, sizeof(message_header_t));
    
    if (msg->header.magic != MAGIC_NUMBER) {
        return -1;
    }
    
    if (msg->header.payload_len > 0) {
        msg->payload = (uint8_t *)malloc(msg->header.payload_len);
        if (!msg->payload) {
            return -1;
        }
        memcpy(msg->payload, buffer + sizeof(message_header_t), msg->header.payload_len);
    } else {
        msg->payload = NULL;
    }
    
    return 0;
}

/* Free message */
void network_free_message(network_message_t *msg) {
    if (msg && msg->payload) {
        free(msg->payload);
        msg->payload = NULL;
    }
}

/* Check if peer is connected */
bool network_engine_is_peer_connected(network_engine_t *engine, uint32_t peer_id) {
    if (!engine) {
        return false;
    }
    
    pthread_rwlock_rdlock(&engine->peers_lock);
    
    for (uint32_t i = 0; i < engine->num_peers; i++) {
        if (engine->peers[i].node_id == peer_id) {
            bool connected = engine->peers[i].connected;
            pthread_rwlock_unlock(&engine->peers_lock);
            return connected;
        }
    }
    
    pthread_rwlock_unlock(&engine->peers_lock);
    return false;
}

/* Remove peer */
int network_engine_remove_peer(network_engine_t *engine, uint32_t node_id) {
    if (!engine) {
        return -1;
    }
    
    pthread_rwlock_wrlock(&engine->peers_lock);
    
    for (uint32_t i = 0; i < engine->num_peers; i++) {
        if (engine->peers[i].node_id == node_id) {
            if (engine->peers[i].socket_fd >= 0) {
                close(engine->peers[i].socket_fd);
            }
            
            // Shift remaining peers
            for (uint32_t j = i; j < engine->num_peers - 1; j++) {
                engine->peers[j] = engine->peers[j + 1];
            }
            engine->num_peers--;
            
            pthread_rwlock_unlock(&engine->peers_lock);
            return 0;
        }
    }
    
    pthread_rwlock_unlock(&engine->peers_lock);
    return -1;
}

/* Get connected peers */
int network_engine_get_connected_peers(network_engine_t *engine,
                                       uint32_t *peer_ids, uint32_t max_peers) {
    if (!engine || !peer_ids) {
        return -1;
    }
    
    pthread_rwlock_rdlock(&engine->peers_lock);
    
    uint32_t count = 0;
    for (uint32_t i = 0; i < engine->num_peers && count < max_peers; i++) {
        if (engine->peers[i].connected) {
            peer_ids[count++] = engine->peers[i].node_id;
        }
    }
    
    pthread_rwlock_unlock(&engine->peers_lock);
    
    return count;
}
