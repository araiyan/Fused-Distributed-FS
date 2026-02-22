#ifndef METADATA_MANAGER_H
#define METADATA_MANAGER_H

#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include <sys/types.h>

#define MAX_STORAGE_NODES 16
#define MAX_PATH_LENGTH 4096
#define MAX_REPLICAS 3

/* File Metadata State */
typedef enum {
    FILE_STATE_CREATING = 0,
    FILE_STATE_ACTIVE = 1,
    FILE_STATE_DELETING = 2,
    FILE_STATE_DELETED = 3,
    FILE_STATE_REPLICATING = 4
} file_state_t;

/* Metadata Entry for Distributed Filesystem */
typedef struct {
    char file_id[64];                           // Unique file identifier (UUID or hash)
    char path[MAX_PATH_LENGTH];                 // Full file path
    file_state_t state;                         // Current file state
    
    // File attributes
    uint64_t size;                              // File size in bytes
    mode_t mode;                                // File permissions
    uid_t uid;                                  // Owner user ID
    gid_t gid;                                  // Owner group ID
    time_t created_time;                        // Creation timestamp
    time_t modified_time;                       // Last modification timestamp
    time_t accessed_time;                       // Last access timestamp
    
    // Storage location
    char storage_node_ips[MAX_STORAGE_NODES][64]; // IP addresses of storage nodes
    uint32_t storage_node_ports[MAX_STORAGE_NODES]; // Ports of storage nodes
    uint32_t num_storage_nodes;                  // Number of storage nodes holding this file
    uint32_t primary_node_idx;                   // Index of primary storage node
    
    // Distributed filesystem specifics
    uint64_t version;                            // Version number for consistency
    uint64_t stripe_size;                        // Stripe/chunk size for distributed storage
    uint32_t num_replicas;                       // Number of replicas
    
    // Locking for concurrent access
    pthread_rwlock_t lock;                       // Read-write lock for this entry
} metadata_entry_t;

/* WAL (Write-Ahead Log) Entry */
typedef enum {
    WAL_OP_CREATE = 1,
    WAL_OP_UPDATE = 2,
    WAL_OP_DELETE = 3,
    WAL_OP_TRUNCATE = 4
} wal_op_type_t;

typedef struct {
    uint64_t log_sequence_number;                // LSN (globally ordered)
    wal_op_type_t op_type;                       // Type of operation
    time_t timestamp;                            // When operation was logged
    
    // Serialized metadata entry
    uint8_t *data;                               // Serialized metadata_entry_t
    size_t data_len;                             // Length of serialized data
    
    // Checksum for integrity
    uint32_t checksum;                           // CRC32 checksum of data
} wal_entry_t;

/* Metadata Manager (In-Memory Hash Map + WAL) */
typedef struct {
    // Hash map for metadata (key: file_id, value: metadata_entry_t*)
    void *hash_map;                              // Using uthash or custom implementation
    
    // WAL management
    int wal_fd;                                  // File descriptor for WAL file
    char wal_path[256];                          // Path to WAL file
    uint64_t current_lsn;                        // Current log sequence number
    pthread_mutex_t wal_lock;                    // Lock for WAL writes
    
    // Global lock for hash map operations
    pthread_rwlock_t map_lock;                   // Read-write lock for the hash map
    
    // Statistics
    uint64_t total_entries;                      // Total number of metadata entries
    uint64_t total_size;                         // Total size of all files
} metadata_manager_t;

