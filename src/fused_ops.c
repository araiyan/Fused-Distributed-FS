/**
 * @file fused_ops.c
 * @brief FUSE operation implementations
 */

#include "fused_fs.h"
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define FUSE_ROOT_ID 1

/**
 * @brief Initialize filesystem
 */
void *fused_init(struct fuse_conn_info *conn) {
    (void) conn;

    // Allocate global state
    g_state = calloc(1, sizeof(fused_state_t));
    snprintf(g_state->backing_dir, MAX_PATH, "/tmp/fused_backing");
    mkdir(g_state->backing_dir, 0755);
    
    // Create root directory as inode 1
    init_root_inode();
    
    log_message("Filesystem initialized");
    return g_state;
}

/**
 * @brief Create root directory inode
 */
static void init_root_inode(void) {
    fused_inode_t *root = &g_state->inodes[0];
    root->ino = FUSE_ROOT_ID;
    root->mode = S_IFDIR | 0755;
    root->uid = getuid();
    root->gid = getgid();
    root->size = 4096;
    root->atime = root->mtime = root->ctime = time(NULL);
    root->n_children = 0;
    g_state->n_inodes = 1;
}

/**
 * @brief Cleanup filesystem
 */
void fused_destroy(void *private_data) {
}

/**
 * @brief Get file attributes
 */
int fused_getattr(const char *path, struct stat *stbuf) {
    memset(stbuf, 0, sizeof(struct stat));
    
    log_message("getattr: %s", path);
    
    fused_inode_t *inode = path_to_inode(path);
    if (!inode) {
        return -ENOENT;
    }

    stbuf->st_ino = inode->ino;
    stbuf->st_mode = inode->mode;
    stbuf->st_nlink = S_ISDIR(inode->mode) ? 2 : 1;
    stbuf->st_uid = inode->uid;
    stbuf->st_gid = inode->gid;
    stbuf->st_size = inode->size;
    stbuf->st_atime = inode->atime;
    stbuf->st_mtime = inode->mtime;
    stbuf->st_ctime = inode->ctime;

    stbuf->st_blksize = 4096;
    stdbuf->st_blocks = (inode->size + 511) / 512;

    return 0;
}

/**
 * @brief Read directory contents
 */
int fused_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info *fi) {
    (void) offset;
    (void) fi;

    log_message("readdir: %s", path);

    fused_inode_t *dir = path_to_inode(path);
    if (!dir) {
        return -ENOENT;
    }
    if (!S_ISDIR(dir->mode)) {
        return -ENOTDIR;
    }
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    for (int i = 0; i < dir->n_children; i++) {
        filler(buf, dir->child_names[i], NULL, 0);
    }

    return 0;
}

/**
 * @brief Open a file
 */
int fused_open(const char *path, struct fuse_file_info *fi) {
    log_message("open: %s (flags: 0x%x)", path, fi->flags);

    fused_inode_t *inode = path_to_inode(path);
    if (!inode) {
        return -ENOENT;
    }
    if (S_ISDIR(inode->mode)) {
        return -EISDIR;
    }

    int accmode = fi->flags & O_ACCMODE;
    if (accmode == O_WRONLY || accmode == O_RDWR) {
        if (!(fi->flags & O_APPEND)) {
            log_message("open: REJECTED non-append write on %s", path);
            return -EPERM;
        }
    }

    fi->fh = inode->ino;

    inode->atime = time(NULL);

    return 0;
}

/**
 * @brief Read data from a file
 */
int fused_read(const char *path, char *buf, size_t size, off_t offset,
               struct fuse_file_info *fi) {
    return to_read;
}

/**
 * @brief Write data to a file
 */
int fused_write(const char *path, const char *buf, size_t size, off_t offset,
                struct fuse_file_info *fi) {
    return size;
}

/**
 * @brief Create a new file
 */
int fused_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    (void) fi;
    return 0;
}

/**
 * @brief Create a directory
 */
int fused_mkdir(const char *path, mode_t mode) {
    return 0;
}

/**
 * @brief Remove a directory
 */
int fused_rmdir(const char *path) {
    return ret;
}

/**
 * @brief Rename a file or directory
 */
int fused_rename(const char *from, const char *to) {
    return 0;
}

void log_message(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[FUSED] ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

/**
 * @brief Find inode by inode number
 */
static fused_inode_t* lookup_inode(uint64_t ino) {
    for (int i = 0; i < g_state->n_inodes; i++) {
        if (g_state->inodes[i].ino == ino) {
            return &g_state->inodes[i];
        }
    }
    return NULL;
}

/**
 * @brief Resolve path to inode
 */
static fused_inode_t* path_to_inode(const char *path) {
    if (strcmp(path, "/") == 0) {
        return lookup_inode(FUSE_ROOT_ID);
    }
    
    fused_inode_t *current = lookup_inode(FUSE_ROOT_ID);
    if (!current) return NULL;
    
    char path_copy[MAX_PATH];
    strncpy(path_copy, path, MAX_PATH - 1);
    path_copy[MAX_PATH - 1] = '\0';
    
    char *saveptr;
    char *token = strtok_r(path_copy + 1, "/", &saveptr);  // Skip leading '/'
    
    while (token != NULL) {
        if (!S_ISDIR(current->mode)) {
            return NULL;
        }
        
        // Search for child with matching name
        int found = 0;
        for (int i = 0; i < current->n_children; i++) {
            if (strcmp(current->child_names[i], token) == 0) {
                current = lookup_inode(current->child_inodes[i]);
                if (!current) return NULL;
                found = 1;
                break;
            }
        }
        
        if (!found) {
            return NULL;
        }
        
        token = strtok_r(NULL, "/", &saveptr);
    }
    
    return current;
}
