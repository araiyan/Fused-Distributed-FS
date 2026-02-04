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

#endif /* FUSED_FS_H */
