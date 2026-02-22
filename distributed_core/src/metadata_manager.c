#include "metadata_manager.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>

// Simple hash map implementation using linked list
typedef struct metadata_entry_node {
    char key[64];                       // file_id
    metadata_entry_t *entry;
    struct metadata_entry_node *next;
} metadata_entry_node_t;

#define HASH_MAP_SIZE 1024

typedef struct {
    metadata_entry_node_t *buckets[HASH_MAP_SIZE];
} metadata_hash_map_t;

/* Simple hash function */
static uint32_t hash_string(const char *str) {
    uint32_t hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % HASH_MAP_SIZE;
}

/* CRC32 checksum calculation */
uint32_t metadata_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

/* Initialize metadata manager */
metadata_manager_t *metadata_manager_init(const char *wal_path) {
    if (!wal_path) {
        return NULL;
    }
    
    metadata_manager_t *mgr = (metadata_manager_t *)calloc(1, sizeof(metadata_manager_t));
    if (!mgr) {
        return NULL;
    }
    
    // Initialize hash map
    mgr->hash_map = calloc(1, sizeof(metadata_hash_map_t));
    if (!mgr->hash_map) {
        free(mgr);
        return NULL;
    }
    
    // Open WAL file
    strncpy(mgr->wal_path, wal_path, sizeof(mgr->wal_path) - 1);
    mgr->wal_fd = open(wal_path, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (mgr->wal_fd < 0) {
        perror("Failed to open WAL file");
        free(mgr->hash_map);
        free(mgr);
        return NULL;
    }
    
    // Initialize locks
    pthread_rwlock_init(&mgr->map_lock, NULL);
    pthread_mutex_init(&mgr->wal_lock, NULL);
    
    mgr->current_lsn = 0;
    mgr->total_entries = 0;
    mgr->total_size = 0;
    
    // Replay WAL for recovery
    if (wal_replay(mgr) != 0) {
        fprintf(stderr, "Warning: WAL replay failed\n");
    }
    
    return mgr;
}

/* Destroy metadata manager */
void metadata_manager_destroy(metadata_manager_t *mgr) {
    if (!mgr) return;
    
    // Close WAL
    if (mgr->wal_fd >= 0) {
        fsync(mgr->wal_fd);
        close(mgr->wal_fd);
    }
    
    // Free hash map
    if (mgr->hash_map) {
        metadata_hash_map_t *map = (metadata_hash_map_t *)mgr->hash_map;
        for (int i = 0; i < HASH_MAP_SIZE; i++) {
            metadata_entry_node_t *node = map->buckets[i];
            while (node) {
                metadata_entry_node_t *next = node->next;
                pthread_rwlock_destroy(&node->entry->lock);
                free(node->entry);
                free(node);
                node = next;
            }
        }
        free(map);
    }
    
    pthread_rwlock_destroy(&mgr->map_lock);
    pthread_mutex_destroy(&mgr->wal_lock);
    
    free(mgr);
}

/* Create new metadata entry */
metadata_entry_t *metadata_create_entry(metadata_manager_t *mgr, const char *path,
                                        mode_t mode, uid_t uid, gid_t gid) {
    if (!mgr || !path) {
        return NULL;
    }
    
    metadata_entry_t *entry = (metadata_entry_t *)calloc(1, sizeof(metadata_entry_t));
    if (!entry) {
        return NULL;
    }
    
    // Generate unique file_id (simple hash for now, should use UUID in production)
    snprintf(entry->file_id, sizeof(entry->file_id), "%lx_%ld", 
             (unsigned long)hash_string(path), time(NULL));
    
    strncpy(entry->path, path, MAX_PATH_LENGTH - 1);
    entry->state = FILE_STATE_CREATING;
    entry->size = 0;
    entry->mode = mode;
    entry->uid = uid;
    entry->gid = gid;
    entry->created_time = time(NULL);
    entry->modified_time = entry->created_time;
    entry->accessed_time = entry->created_time;
    entry->num_storage_nodes = 0;
    entry->version = 1;
    entry->stripe_size = 4194304; // 4MB default
    entry->num_replicas = 0;
    
    pthread_rwlock_init(&entry->lock, NULL);
    
    // Write to WAL
    uint64_t lsn = wal_append(mgr, WAL_OP_CREATE, entry);
    if (lsn == UINT64_MAX) {
        free(entry);
        return NULL;
    }
    
    // Insert into hash map
    pthread_rwlock_wrlock(&mgr->map_lock);
    
    metadata_hash_map_t *map = (metadata_hash_map_t *)mgr->hash_map;
    uint32_t idx = hash_string(entry->file_id);
    
    metadata_entry_node_t *node = (metadata_entry_node_t *)malloc(sizeof(metadata_entry_node_t));
    strncpy(node->key, entry->file_id, sizeof(node->key) - 1);
    node->entry = entry;
    node->next = map->buckets[idx];
    map->buckets[idx] = node;
    
    mgr->total_entries++;
    
    pthread_rwlock_unlock(&mgr->map_lock);
    
    return entry;
}

/* Lookup metadata entry by file_id */
metadata_entry_t *metadata_lookup(metadata_manager_t *mgr, const char *file_id) {
    if (!mgr || !file_id) {
        return NULL;
    }
    
    pthread_rwlock_rdlock(&mgr->map_lock);
    
    metadata_hash_map_t *map = (metadata_hash_map_t *)mgr->hash_map;
    uint32_t idx = hash_string(file_id);
    
    metadata_entry_node_t *node = map->buckets[idx];
    while (node) {
        if (strcmp(node->key, file_id) == 0) {
            pthread_rwlock_unlock(&mgr->map_lock);
            return node->entry;
        }
        node = node->next;
    }
    
    pthread_rwlock_unlock(&mgr->map_lock);
    return NULL;
}

/* Lookup metadata entry by path */
metadata_entry_t *metadata_lookup_by_path(metadata_manager_t *mgr, const char *path) {
    if (!mgr || !path) {
        return NULL;
    }
    
    pthread_rwlock_rdlock(&mgr->map_lock);
    
    metadata_hash_map_t *map = (metadata_hash_map_t *)mgr->hash_map;
    
    // Linear search (inefficient, should add path index in production)
    for (int i = 0; i < HASH_MAP_SIZE; i++) {
        metadata_entry_node_t *node = map->buckets[i];
        while (node) {
            if (strcmp(node->entry->path, path) == 0 && 
                node->entry->state != FILE_STATE_DELETED) {
                pthread_rwlock_unlock(&mgr->map_lock);
                return node->entry;
            }
            node = node->next;
        }
    }
    
    pthread_rwlock_unlock(&mgr->map_lock);
    return NULL;
}

/* Update metadata entry */
int metadata_update_entry(metadata_manager_t *mgr, metadata_entry_t *entry) {
    if (!mgr || !entry) {
        return -1;
    }
    
    // Write to WAL first
    uint64_t lsn = wal_append(mgr, WAL_OP_UPDATE, entry);
    if (lsn == UINT64_MAX) {
        return -1;
    }
    
    pthread_rwlock_wrlock(&entry->lock);
    entry->version++;
    entry->modified_time = time(NULL);
    pthread_rwlock_unlock(&entry->lock);
    
    return 0;
}

/* Delete metadata entry */
int metadata_delete_entry(metadata_manager_t *mgr, const char *file_id) {
    if (!mgr || !file_id) {
        return -1;
    }
    
    metadata_entry_t *entry = metadata_lookup(mgr, file_id);
    if (!entry) {
        return -1;
    }
    
    pthread_rwlock_wrlock(&entry->lock);
    entry->state = FILE_STATE_DELETED;
    pthread_rwlock_unlock(&entry->lock);
    
    // Write to WAL
    uint64_t lsn = wal_append(mgr, WAL_OP_DELETE, entry);
    if (lsn == UINT64_MAX) {
        return -1;
    }
    
    pthread_rwlock_wrlock(&mgr->map_lock);
    mgr->total_entries--;
    mgr->total_size -= entry->size;
    pthread_rwlock_unlock(&mgr->map_lock);
    
    return 0;
}

