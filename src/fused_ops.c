/**
 * @file fused_ops.c
 * @brief FUSE operation implementations
 */

#include "fused_fs.h"
#include <stdint.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#define FUSE_ROOT_ID 1

/* Forward declarations of static helper functions */
static void init_root_inode(void);
static void split_path(const char *path, char *parent_path, char *child_name);
static fused_inode_t *alloc_inode(void);
static void free_inode(fused_inode_t *inode);
static int dir_add_entry(fused_inode_t *dir, const char *name, fused_inode_t *child);
static int dir_rm_entry(fused_inode_t *dir, const char *name, fused_inode_t *child);
static fused_inode_t *lookup_inode(uint64_t ino);
static void generate_backing_path(fused_inode_t *inode, uint64_t ino);
static fused_inode_t *path_to_inode(const char *path);

/* Global state pointer */
fused_state_t *g_state = NULL;

/**
 * @brief Initialize filesystem
 */
void *fused_init(struct fuse_conn_info *conn)
{
    (void)conn;

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
static void init_root_inode(void)
{
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
void fused_destroy(void *private_data)
{
    (void)private_data;

    if (!g_state)
        return;

    log_message("Filesystem destroyed");

    // Cleanup backing files
    for (int i = 0; i < g_state->n_inodes; i++)
    {
        if (g_state->inodes[i].backing_path[0] != '\0')
        {
            unlink(g_state->inodes[i].backing_path);
        }
    }

    free(g_state);
    g_state = NULL;
}


/**
 * @brief Get file attributes
 */
int fused_getattr(const char *path, struct stat *stbuf)
{
    memset(stbuf, 0, sizeof(struct stat));

    log_message("getattr: %s", path);

    fused_inode_t *inode = path_to_inode(path);
    if (!inode)
    {
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
    stbuf->st_blocks = (inode->size + 511) / 512;

    return 0;
}

/**
 * @brief Read directory contents
 */
int fused_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                  off_t offset, struct fuse_file_info *fi)
{
    (void)offset;
    (void)fi;

    log_message("readdir: %s", path);

    fused_inode_t *dir = path_to_inode(path);
    if (!dir)
    {
        return -ENOENT;
    }
    if (!S_ISDIR(dir->mode))
    {
        return -ENOTDIR;
    }
    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);

    for (int i = 0; i < dir->n_children; i++)
    {
        filler(buf, dir->child_names[i], NULL, 0);
    }

    return 0;
}

/**
 * @brief Open a file
 */
int fused_open(const char *path, struct fuse_file_info *fi)
{
    log_message("open: %s (flags: 0x%x)", path, fi->flags);

    fused_inode_t *inode = path_to_inode(path);
    if (!inode)
    {
        return -ENOENT;
    }
    if (S_ISDIR(inode->mode))
    {
        return -EISDIR;
    }

    int accmode = fi->flags & O_ACCMODE;
    if (accmode == O_WRONLY || accmode == O_RDWR)
    {
        if (!(fi->flags & O_APPEND))
        {
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
               struct fuse_file_info *fi)
{
    (void)path; // Use inode from file handle instead

    log_message("read: inode=%lu, size=%zu, offset=%ld", fi->fh, size, offset);

    // Get inode directly from file handle (set in fused_open)
    fused_inode_t *inode = lookup_inode(fi->fh);
    if (!inode)
    {
        log_message("read: inode %lu not found", fi->fh);
        return -ENOENT;
    }

    // Check if offset is beyond file size
    if (offset >= inode->size)
    {
        return 0;
    }

    // Calculate how much to read
    size_t to_read = size;
    if (offset + to_read > (size_t)inode->size)
    {
        to_read = inode->size - offset;
    }

    // Open the backing file for reading
    FILE *fp = fopen(inode->backing_path, "rb"); // Note: FILE will depend on how create is made
    if (!fp)
    {
        log_message("read: failed to open backing file %s", inode->backing_path);
        return -EIO;
    }

    // Seek to offset and read
    if (fseek(fp, offset, SEEK_SET) != 0)
    {
        fclose(fp);
        return -EIO;
    }

    size_t bytes_read = fread(buf, 1, to_read, fp);
    fclose(fp);

    // Update access time
    inode->atime = time(NULL);

    log_message("read: successfully read %zu bytes from inode %lu", bytes_read, fi->fh);

    return bytes_read;
}

/**
 * @brief Write data to a file
 */
int fused_write(const char *path, const char *buf, size_t size, off_t offset,
                struct fuse_file_info *fi)
{
    (void)path; // Use inode from file handle instead

    log_message("write: inode=%lu, size=%zu, offset=%ld", fi->fh, size, offset);

    // Get inode directly from file handle (set in fused_open)
    fused_inode_t *inode = lookup_inode(fi->fh);
    if (!inode)
    {
        log_message("write: inode %lu not found", fi->fh);
        return -ENOENT;
    }

    // Enforce append-only: offset must be at end of file
    if (offset < inode->size)
    {
        log_message("write: REJECTED - append-only mode, offset=%ld < size=%ld",
                    offset, inode->size);
        return -EPERM;
    }

    // Open the backing file for writing (append mode)
    FILE *fp = fopen(inode->backing_path, "ab"); // Note: FILE will depend on how create is made
    if (!fp)
    {
        log_message("write: failed to open backing file %s", inode->backing_path);
        return -EIO;
    }

    // If there's a gap between current size and offset, fill with zeros
    if (offset > inode->size)
    {
        size_t gap = offset - inode->size;
        char zero_buf[4096];
        memset(zero_buf, 0, sizeof(zero_buf));

        while (gap > 0)
        {
            size_t write_size = (gap > sizeof(zero_buf)) ? sizeof(zero_buf) : gap;
            fwrite(zero_buf, 1, write_size, fp);
            gap -= write_size;
        }
    }

    // Write the data
    size_t bytes_written = fwrite(buf, 1, size, fp);
    fclose(fp);

    if (bytes_written != size)
    {
        log_message("write: partial write - wrote %zu of %zu bytes", bytes_written, size);
        return -EIO;
    }

    // Update inode metadata
    inode->size = offset + bytes_written;
    inode->mtime = time(NULL);
    inode->ctime = time(NULL);

    log_message("write: successfully wrote %zu bytes to inode %lu (new size: %ld)",
                bytes_written, fi->fh, inode->size);

    return bytes_written;
}

/**
 * @brief Create a new file
 */
int fused_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    fused_inode_t *existing = path_to_inode(path);
    if (existing)
    {
        return -EEXIST;
    }
    char parent_path[MAX_PATH];
    char child_name[MAX_NAME];
    split_path(path, parent_path, child_name);

    fused_inode_t *parent = path_to_inode(parent_path);

    if (!parent || !S_ISDIR(parent->mode))
    {
        return -ENOENT;
    }
    fused_inode_t *inode = alloc_inode();
    if (!inode)
    {
        return -ENOMEM;
    }
    // overwrite file type as 'regular'
    inode->mode = S_IFREG | (mode & 0777);
    inode->uid = fuse_get_context()->uid;
    inode->gid = fuse_get_context()->gid;
    inode->size = 0;

    // accessed, modified, and created now
    inode->atime = time(NULL);
    inode->mtime = inode->atime;
    inode->ctime = inode->atime;

    // create backing file
    FILE *fp = fopen(inode->backing_path, "wb");
    if (!fp)
    {
        free_inode(inode);
        return -EIO;
    }
    fclose(fp);

    int rc = dir_add_entry(parent, child_name, inode);
    if (rc != 0)
    {
        free_inode(inode);
        return rc;
    }

    fi->fh = inode->ino;

    return 0;
}

/**
 * @brief Update file timestamps (utimens)
 */
int fused_utimens(const char *path, const struct timespec tv[2])
{
    log_message("utimens: %s", path);
    
    fused_inode_t *inode = path_to_inode(path);
    if (!inode)
    {
        return -ENOENT;
    }
    
    // Update access time (tv[0])
    if (tv[0].tv_nsec == UTIME_NOW)
    {
        inode->atime = time(NULL);
    }
    else if (tv[0].tv_nsec != UTIME_OMIT)
    {
        inode->atime = tv[0].tv_sec;
    }
    
    // Update modification time (tv[1])
    if (tv[1].tv_nsec == UTIME_NOW)
    {
        inode->mtime = time(NULL);
    }
    else if (tv[1].tv_nsec != UTIME_OMIT)
    {
        inode->mtime = tv[1].tv_sec;
    }
    
    // Always update ctime when any metadata changes
    inode->ctime = time(NULL);
    
    log_message("utimens: updated timestamps for %s (inode %lu)", path, inode->ino);
    return 0;
}


/**
 * @brief Create a directory
 */
int fused_mkdir(const char *path, mode_t mode)
{
    log_message("mkdir: %s", path);

    // error if directory already exists
    if (path_to_inode(path) != NULL)
        return -EEXIST;

    // separate path into parent and directory
    char path_copy[MAX_PATH];
    strncpy(path_copy, path, MAX_PATH - 1);
    path_copy[MAX_PATH - 1] = '\0';
    char *last_slash = strrchr(path_copy, '/');

    char *parent_path;
    char *dir_name;

    if (last_slash == path_copy)
    {
        parent_path = "/";
        dir_name = path_copy + 1;
    }
    else
    {
        *last_slash = '\0';
        parent_path = path_copy;
        dir_name = last_slash + 1;
    }

    fused_inode_t *parent = path_to_inode(parent_path);
    if (!parent)
        return -ENOENT;
    if (!S_ISDIR(parent->mode))
        return -ENOTDIR;

    // create the new inode
    if (g_state->n_inodes >= MAX_INODES)
        return -ENOSPC;

    fused_inode_t *new_inode = &g_state->inodes[g_state->n_inodes];
    memset(new_inode, 0, sizeof(fused_inode_t));

    new_inode->ino = g_state->n_inodes + 1;
    generate_backing_path(new_inode, new_inode->ino);

    new_inode->mode = S_IFDIR | (mode & 0777);
    new_inode->uid = getuid();
    new_inode->gid = getgid();
    new_inode->size = 4096;
    new_inode->atime = new_inode->mtime = new_inode->ctime = time(NULL);
    new_inode->n_children = 0;

    // register the name in the parent's children list
    if (parent->n_children >= MAX_CHILDREN)
        return -ENOSPC;

    strncpy(parent->child_names[parent->n_children], dir_name, MAX_NAME - 1);
    parent->child_names[parent->n_children][MAX_NAME - 1] = '\0';
    parent->child_inodes[parent->n_children] = new_inode->ino;
    parent->n_children++;
    parent->mtime = time(NULL);
    parent->ctime = parent->mtime;

    g_state->n_inodes++;

    log_message("mkdir: created %s (inode %lu)", path, new_inode->ino);
    return 0;
}

/**
 * @brief Remove a directory
 * @pre directory is empty
 */
int fused_rmdir(const char *path)
{
    log_message("rmdir: %s", path);

    if (strcmp(path, "/") == 0)
        return -EBUSY;

    // find the target inode
    fused_inode_t *inode = path_to_inode(path);
    if (!inode)
        return -ENOENT;
    if (!S_ISDIR(inode->mode))
        return -ENOTDIR;
    if (inode->n_children > 0)
        return -ENOTEMPTY;

    // find the parent to remove the reference
    char path_copy[MAX_PATH];
    strncpy(path_copy, path, MAX_PATH - 1);
    path_copy[MAX_PATH - 1] = '\0';
    char *last_slash = strrchr(path_copy, '/');

    char *parent_path = (last_slash == path_copy) ? "/" : path_copy;
    if (last_slash != path_copy)
        *last_slash = '\0';

    char *dir_name = last_slash + 1;

    fused_inode_t *parent = path_to_inode(parent_path);
    if (!parent)
        return -ENOENT;

    // remove the name from parent's child_names array
    bool found = false;
    for (int i = 0; i < parent->n_children; i++)
    {
        if (strcmp(parent->child_names[i], dir_name) == 0)
        {
            for (int j = i; j < parent->n_children - 1; j++)
            {
                strncpy(parent->child_names[j],
                        parent->child_names[j + 1],
                        MAX_NAME);

                parent->child_inodes[j] =
                    parent->child_inodes[j + 1];
            }

            parent->n_children--;
            found = true;
            break;
        }
    }

    if (!found)
        return -ENOENT;

    parent->mtime = time(NULL);
    parent->ctime = parent->mtime;

    // delete inode
    memset(inode, 0, sizeof(fused_inode_t));

    log_message("rmdir: successfully removed %s", path);
    return 0;
}

/**
 * @brief Rename a file or directory
 */
int fused_rename(const char *from, const char *to)
{
    fused_inode_t *inode = path_to_inode(from);
    if (!inode)
    {
        return -ENOENT;
    }
    fused_inode_t *existing = path_to_inode(to);
    if (existing)
    {
        return -EEXIST;
    }
    char parent_path[MAX_PATH];
    char child_name[MAX_NAME];
    split_path(from, parent_path, child_name);

    fused_inode_t *parent = path_to_inode(parent_path);
    int rc = dir_rm_entry(parent, child_name, inode);
    if (rc != 0)
    {
        return rc;
    }

    // accessed, and modified now
    inode->atime = time(NULL);
    inode->mtime = inode->atime;

    split_path(to, parent_path, child_name);
    parent = path_to_inode(parent_path);
    rc = dir_add_entry(parent, child_name, inode);
    if (rc != 0)
    {
        return rc;
    }
    return 0;
}

void log_message(const char *fmt, ...)
{
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
static fused_inode_t *lookup_inode(uint64_t ino)
{
    for (int i = 0; i < g_state->n_inodes; i++)
    {
        if (g_state->inodes[i].ino == ino)
        {
            return &g_state->inodes[i];
        }
    }
    return NULL;
}

/**
 * @brief Generate backing file path for an inode
 */
static void generate_backing_path(fused_inode_t *inode, uint64_t ino)
{
    snprintf(inode->backing_path, MAX_PATH, "%s/inode_%lu",
             g_state->backing_dir, ino);
}

/**
 * @brief Resolve path to inode
 */
static fused_inode_t *path_to_inode(const char *path)
{
    if (strcmp(path, "/") == 0)
    {
        return lookup_inode(FUSE_ROOT_ID);
    }

    fused_inode_t *current = lookup_inode(FUSE_ROOT_ID);
    if (!current)
        return NULL;

    char path_copy[MAX_PATH];
    strncpy(path_copy, path, MAX_PATH - 1);
    path_copy[MAX_PATH - 1] = '\0';

    char *saveptr;
    char *token = strtok_r(path_copy + 1, "/", &saveptr); // Skip leading '/'

    while (token != NULL)
    {
        if (!S_ISDIR(current->mode))
        {
            return NULL;
        }

        // Search for child with matching name
        int found = 0;
        for (int i = 0; i < current->n_children; i++)
        {
            if (strcmp(current->child_names[i], token) == 0)
            {
                current = lookup_inode(current->child_inodes[i]);
                if (!current)
                    return NULL;
                found = 1;
                break;
            }
        }

        if (!found)
        {
            return NULL;
        }

        token = strtok_r(NULL, "/", &saveptr);
    }

    return current;
}

/**
 * @brief Allocate a new inode
 * @return pointer to new inode, or NULL if no space
 */
static fused_inode_t *alloc_inode(void)
{
    if (g_state->n_inodes >= MAX_INODES)
    {
        return NULL;
    }

    fused_inode_t *inode = &g_state->inodes[g_state->n_inodes];

    // Clear entire inode slot
    memset(inode, 0, sizeof(fused_inode_t));

    inode->ino = g_state->n_inodes + 1;
    generate_backing_path(inode, inode->ino);

    g_state->n_inodes++;
    return inode;
}

/**
 * @brief Free an inode (mark as unused)
 * Note: This simple implementation does not reuse freed inodes, but it does clean up backing files.
 */
static void free_inode(fused_inode_t *inode)
{
    if (!inode)
        return;

    if (inode->backing_path[0] != '\0')
    {
        unlink(inode->backing_path);
    }

    memset(inode, 0, sizeof(fused_inode_t));
}

/**
 * @brief Split full path into parent path and child name
 */
static void split_path(const char *path, char *parent_path, char *child_name)
{
    if (strcmp(path, "/") == 0)
    {
        strcpy(parent_path, "/");
        strcpy(child_name, "");
        return;
    }

    char temp[MAX_PATH];
    strncpy(temp, path, MAX_PATH - 1);
    temp[MAX_PATH - 1] = '\0';

    char *last_slash = strrchr(temp, '/');

    if (last_slash == temp)
    {
        // Parent is root
        strcpy(parent_path, "/");
        strcpy(child_name, last_slash + 1);
    }
    else
    {
        *last_slash = '\0';
        strcpy(parent_path, temp);
        strcpy(child_name, last_slash + 1);
    }
}

/**
 * @brief Add a child entry to a directory
 */
static int dir_add_entry(fused_inode_t *dir, const char *name, fused_inode_t *child)
{
    if (!dir || !S_ISDIR(dir->mode))
    {
        return -ENOTDIR;
    }

    if (dir->n_children >= MAX_CHILDREN)
    {
        return -ENOSPC;
    }

    // Check duplicate name
    for (int i = 0; i < dir->n_children; i++)
    {
        if (strcmp(dir->child_names[i], name) == 0)
        {
            return -EEXIST;
        }
    }

    strncpy(dir->child_names[dir->n_children], name, MAX_NAME - 1);
    dir->child_names[dir->n_children][MAX_NAME - 1] = '\0';

    dir->child_inodes[dir->n_children] = child->ino;

    dir->n_children++;

    dir->mtime = time(NULL);
    dir->ctime = dir->mtime;

    return 0;
}

/**
 * @brief Remove a child entry from a directory
 */
static int dir_rm_entry(fused_inode_t *dir, const char *name, fused_inode_t *child)
{
    if (!dir || !S_ISDIR(dir->mode))
    {
        return -ENOTDIR;
    }

    for (int i = 0; i < dir->n_children; i++)
    {
        if (strcmp(dir->child_names[i], name) == 0 &&
            dir->child_inodes[i] == child->ino)
        {

            // Shift left
            for (int j = i; j < dir->n_children - 1; j++)
            {
                strncpy(dir->child_names[j],
                        dir->child_names[j + 1],
                        MAX_NAME);

                dir->child_inodes[j] =
                    dir->child_inodes[j + 1];
            }

            dir->n_children--;

            dir->mtime = time(NULL);
            dir->ctime = dir->mtime;

            return 0;
        }
    }

    return -ENOENT;
}
