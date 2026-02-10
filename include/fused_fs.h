/**
 * @file fused_fs.h
 * @brief ShortsFS - YouTube Shorts Optimized File System
 * 
 * FUSE filesystem optimized for efficient YouTube Shorts storage
 * Features: Forced append-only, video metadata, streaming optimizations
 */

#ifndef FUSED_FS_H
#define FUSED_FS_H

#define FUSE_USE_VERSION 26
#define MAX_PATH 256
#define MAX_CHILDREN 1024
#define MAX_INODES 4096
#define FUSE_ROOT_ID 1
#define MAX_NAME 256

#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <assert.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdarg.h>
#include <stdbool.h>

/**
 * @brief Minimal inode structure
 */
typedef struct {
    uint64_t ino;           // Unique inode number
    mode_t mode;            // File type (S_IFREG/S_IFDIR) + permissions
    uid_t uid;              // Owner user ID
    gid_t gid;              // Owner group ID
    off_t size;             // File size in bytes
    time_t atime;           // Last access time
    time_t mtime;           // Last modification time
    time_t ctime;           // Last status change time
    
    int n_children;
    char child_names[MAX_CHILDREN][MAX_PATH];
    uint64_t child_inodes[MAX_CHILDREN];
    
    char backing_path[MAX_PATH];
} fused_inode_t;

/**
 * @brief Global filesystem state
 */
typedef struct {
    fused_inode_t inodes[MAX_INODES];  // Fixed-size inode table
    int n_inodes;                       // Number of allocated inodes
    char backing_dir[MAX_PATH];         // Where backing files live
} fused_state_t;

/* Function prototypes */

/* Initialization and cleanup */
void *fused_init(struct fuse_conn_info *conn);
void fused_destroy(void *private_data);

/* File operations */
int fused_getattr(const char *path, struct stat *stbuf);
int fused_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info *fi);
int fused_open(const char *path, struct fuse_file_info *fi);
int fused_read(const char *path, char *buf, size_t size, off_t offset,
               struct fuse_file_info *fi);
int fused_write(const char *path, const char *buf, size_t size, off_t offset,
                struct fuse_file_info *fi);
int fused_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int fused_mkdir(const char *path, mode_t mode);
int fused_rmdir(const char *path);
int fused_rename(const char *from, const char *to);

/* Global state */
extern fused_state_t *g_state;

/* Helper function declarations */
static fused_inode_t* lookup_inode(uint64_t ino);
static fused_inode_t* path_to_inode(const char *path);
static void init_root_inode(void);
static void generate_backing_path(fused_inode_t *inode, uint64_t ino);

/* Helper function for logging */
void log_message(const char *fmt, ...);

#endif /* FUSED_FS_H */
