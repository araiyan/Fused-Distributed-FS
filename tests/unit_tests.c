/**
 * @file unit_tests.c
 * @brief CUnit tests for FUSED filesystem callbacks
 * Tests for: fused_getattr, fused_readdir, fused_open
 */

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include "../include/fused_fs.h"
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

// Test fixture: initialize filesystem before each test
int init_suite(void)
{
    // Initialize filesystem state
    g_state = calloc(1, sizeof(fused_state_t));
    snprintf(g_state->backing_dir, MAX_PATH, "/tmp/fused_test_backing");
    mkdir(g_state->backing_dir, 0755);
    
    // Create root inode
    fused_inode_t *root = &g_state->inodes[0];
    root->ino = FUSE_ROOT_ID;
    root->mode = S_IFDIR | 0755;
    root->uid = getuid();
    root->gid = getgid();
    root->size = 4096;
    root->atime = root->mtime = root->ctime = time(NULL);
    root->n_children = 0;
    g_state->n_inodes = 1;
    
    return 0;
}

// Test fixture: cleanup after each test
int clean_suite(void)
{
    if (g_state)
    {
        // Remove backing files
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
    rmdir("/tmp/fused_test_backing");
    return 0;
}

// Helper: create a test file inode
static fused_inode_t* create_test_file(const char *name, const char *parent_path)
{
    if (g_state->n_inodes >= MAX_INODES)
        return NULL;
    
    fused_inode_t *file = &g_state->inodes[g_state->n_inodes];
    memset(file, 0, sizeof(fused_inode_t));
    
    file->ino = g_state->n_inodes + 1;
    snprintf(file->backing_path, MAX_PATH, "%s/inode_%lu",
             g_state->backing_dir, file->ino);
    file->mode = S_IFREG | 0644;
    file->uid = getuid();
    file->gid = getgid();
    file->size = 100;  // Arbitrary size
    file->atime = file->mtime = file->ctime = time(NULL);
    
    // Add to parent directory
    fused_inode_t *parent = &g_state->inodes[0];  // Root
    strncpy(parent->child_names[parent->n_children], name, MAX_NAME - 1);
    parent->child_inodes[parent->n_children] = file->ino;
    parent->n_children++;
    
    g_state->n_inodes++;
    
    // Create actual backing file
    FILE *fp = fopen(file->backing_path, "wb");
    if (fp) fclose(fp);
    
    return file;
}

// ============================================================================
// fused_getattr Tests
// ============================================================================

void test_getattr_root_directory(void)
{
    struct stat stbuf;
    int result = fused_getattr("/", &stbuf);
    
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_EQUAL(stbuf.st_ino, FUSE_ROOT_ID);
    CU_ASSERT_TRUE(S_ISDIR(stbuf.st_mode));
    CU_ASSERT_EQUAL(stbuf.st_nlink, 2);
    CU_ASSERT_EQUAL(stbuf.st_size, 4096);
}

void test_getattr_nonexistent_file(void)
{
    struct stat stbuf;
    int result = fused_getattr("/nonexistent.txt", &stbuf);
    
    CU_ASSERT_EQUAL(result, -ENOENT);
}

void test_getattr_regular_file(void)
{
    // Create a test file
    fused_inode_t *file = create_test_file("test.txt", "/");
    CU_ASSERT_PTR_NOT_NULL(file);
    
    struct stat stbuf;
    int result = fused_getattr("/test.txt", &stbuf);
    
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_EQUAL(stbuf.st_ino, file->ino);
    CU_ASSERT_TRUE(S_ISREG(stbuf.st_mode));
    CU_ASSERT_EQUAL(stbuf.st_nlink, 1);
    CU_ASSERT_EQUAL(stbuf.st_size, 100);
    CU_ASSERT_EQUAL(stbuf.st_blksize, 4096);
}

void test_getattr_file_attributes(void)
{
    fused_inode_t *file = create_test_file("attrs.txt", "/");
    CU_ASSERT_PTR_NOT_NULL(file);
    
    file->uid = 1000;
    file->gid = 1000;
    file->mode = S_IFREG | 0640;
    
    struct stat stbuf;
    int result = fused_getattr("/attrs.txt", &stbuf);
    
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_EQUAL(stbuf.st_uid, 1000);
    CU_ASSERT_EQUAL(stbuf.st_gid, 1000);
    CU_ASSERT_EQUAL(stbuf.st_mode & 0777, 0640);
}

void test_getattr_block_calculation(void)
{
    fused_inode_t *file = create_test_file("blocks.txt", "/");
    CU_ASSERT_PTR_NOT_NULL(file);
    
    file->size = 1000;
    
    struct stat stbuf;
    int result = fused_getattr("/blocks.txt", &stbuf);
    
    CU_ASSERT_EQUAL(result, 0);
    // 1000 bytes = (1000 + 511) / 512 = 2 blocks
    CU_ASSERT_EQUAL(stbuf.st_blocks, 2);
}

// ============================================================================
// fused_readdir Tests
// ============================================================================

// Helper to capture filler calls
typedef struct {
    char names[MAX_CHILDREN][MAX_NAME];
    int count;
} readdir_capture_t;

static int test_filler(void *buf, const char *name, const struct stat *stbuf, off_t off)
{
    (void)stbuf;
    (void)off;
    
    readdir_capture_t *capture = (readdir_capture_t *)buf;
    strncpy(capture->names[capture->count], name, MAX_NAME - 1);
    capture->names[capture->count][MAX_NAME - 1] = '\0';
    capture->count++;
    return 0;
}

void test_readdir_empty_root(void)
{
    readdir_capture_t capture = {0};
    
    int result = fused_readdir("/", &capture, test_filler, 0, NULL);
    
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_EQUAL(capture.count, 2);  // Only . and ..
    CU_ASSERT_STRING_EQUAL(capture.names[0], ".");
    CU_ASSERT_STRING_EQUAL(capture.names[1], "..");
}

void test_readdir_with_files(void)
{
    // Create test files
    create_test_file("file1.txt", "/");
    create_test_file("file2.txt", "/");
    create_test_file("file3.txt", "/");
    
    readdir_capture_t capture = {0};
    int result = fused_readdir("/", &capture, test_filler, 0, NULL);
    
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_EQUAL(capture.count, 5);  // ., .., file1, file2, file3
    
    // Check that all files are present (order may vary)
    int found_file1 = 0, found_file2 = 0, found_file3 = 0;
    for (int i = 0; i < capture.count; i++)
    {
        if (strcmp(capture.names[i], "file1.txt") == 0) found_file1 = 1;
        if (strcmp(capture.names[i], "file2.txt") == 0) found_file2 = 1;
        if (strcmp(capture.names[i], "file3.txt") == 0) found_file3 = 1;
    }
    CU_ASSERT_TRUE(found_file1);
    CU_ASSERT_TRUE(found_file2);
    CU_ASSERT_TRUE(found_file3);
}

void test_readdir_nonexistent_directory(void)
{
    readdir_capture_t capture = {0};
    int result = fused_readdir("/nonexistent", &capture, test_filler, 0, NULL);
    
    CU_ASSERT_EQUAL(result, -ENOENT);
}

void test_readdir_file_not_directory(void)
{
    create_test_file("notadir.txt", "/");
    
    readdir_capture_t capture = {0};
    int result = fused_readdir("/notadir.txt", &capture, test_filler, 0, NULL);
    
    CU_ASSERT_EQUAL(result, -ENOTDIR);
}

// ============================================================================
// fused_open Tests
// ============================================================================

void test_open_file_for_reading(void)
{
    create_test_file("readable.txt", "/");
    
    struct fuse_file_info fi = {0};
    fi.flags = O_RDONLY;
    
    int result = fused_open("/readable.txt", &fi);
    
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_NOT_EQUAL(fi.fh, 0);  // File handle should be set
}

void test_open_file_for_append(void)
{
    fused_inode_t *file = create_test_file("appendable.txt", "/");
    
    struct fuse_file_info fi = {0};
    fi.flags = O_WRONLY | O_APPEND;
    
    int result = fused_open("/appendable.txt", &fi);
    
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_EQUAL(fi.fh, file->ino);
}

void test_open_reject_non_append_write(void)
{
    create_test_file("protected.txt", "/");
    
    struct fuse_file_info fi = {0};
    fi.flags = O_WRONLY;  // Write without O_APPEND
    
    int result = fused_open("/protected.txt", &fi);
    
    CU_ASSERT_EQUAL(result, -EPERM);
}

void test_open_reject_rdwr_without_append(void)
{
    create_test_file("protected2.txt", "/");
    
    struct fuse_file_info fi = {0};
    fi.flags = O_RDWR;  // Read-write without O_APPEND
    
    int result = fused_open("/protected2.txt", &fi);
    
    CU_ASSERT_EQUAL(result, -EPERM);
}

void test_open_nonexistent_file(void)
{
    struct fuse_file_info fi = {0};
    fi.flags = O_RDONLY;
    
    int result = fused_open("/doesnotexist.txt", &fi);
    
    CU_ASSERT_EQUAL(result, -ENOENT);
}

void test_open_directory_as_file(void)
{
    struct fuse_file_info fi = {0};
    fi.flags = O_RDONLY;
    
    int result = fused_open("/", &fi);  // Try to open root directory
    
    CU_ASSERT_EQUAL(result, -EISDIR);
}

void test_open_updates_atime(void)
{
    fused_inode_t *file = create_test_file("timefile.txt", "/");
    time_t old_atime = file->atime;
    
    sleep(1);  // Ensure time difference
    
    struct fuse_file_info fi = {0};
    fi.flags = O_RDONLY;
    
    int result = fused_open("/timefile.txt", &fi);
    
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_TRUE(file->atime > old_atime);
}

// ============================================================================
// fused_read Tests
// ============================================================================

void test_read_basic_file(void)
{
    // Create a file with known content
    fused_inode_t *file = create_test_file("readtest.txt", "/");
    CU_ASSERT_PTR_NOT_NULL(file);
    
    const char *test_data = "Hello, FUSED filesystem!";
    FILE *fp = fopen(file->backing_path, "wb");
    CU_ASSERT_PTR_NOT_NULL(fp);
    fwrite(test_data, 1, strlen(test_data), fp);
    fclose(fp);
    file->size = strlen(test_data);
    
    // Open the file
    struct fuse_file_info fi = {0};
    fi.flags = O_RDONLY;
    int result = fused_open("/readtest.txt", &fi);
    CU_ASSERT_EQUAL(result, 0);
    
    // Read the file
    char buf[256] = {0};
    int bytes_read = fused_read("/readtest.txt", buf, sizeof(buf), 0, &fi);
    
    CU_ASSERT_EQUAL(bytes_read, strlen(test_data));
    CU_ASSERT_STRING_EQUAL(buf, test_data);
}

void test_read_with_offset(void)
{
    fused_inode_t *file = create_test_file("offsettest.txt", "/");
    CU_ASSERT_PTR_NOT_NULL(file);
    
    const char *test_data = "0123456789ABCDEFGHIJ";
    FILE *fp = fopen(file->backing_path, "wb");
    fwrite(test_data, 1, strlen(test_data), fp);
    fclose(fp);
    file->size = strlen(test_data);
    
    struct fuse_file_info fi = {0};
    fi.flags = O_RDONLY;
    fused_open("/offsettest.txt", &fi);
    
    // Read from offset 10
    char buf[256] = {0};
    int bytes_read = fused_read("/offsettest.txt", buf, 10, 10, &fi);
    
    CU_ASSERT_EQUAL(bytes_read, 10);
    CU_ASSERT_STRING_EQUAL(buf, "ABCDEFGHIJ");
}

void test_read_beyond_file_size(void)
{
    fused_inode_t *file = create_test_file("smallfile.txt", "/");
    CU_ASSERT_PTR_NOT_NULL(file);
    
    const char *test_data = "small";
    FILE *fp = fopen(file->backing_path, "wb");
    fwrite(test_data, 1, strlen(test_data), fp);
    fclose(fp);
    file->size = strlen(test_data);
    
    struct fuse_file_info fi = {0};
    fi.flags = O_RDONLY;
    fused_open("/smallfile.txt", &fi);
    
    // Try to read beyond file size
    char buf[256] = {0};
    int bytes_read = fused_read("/smallfile.txt", buf, 100, file->size, &fi);
    
    CU_ASSERT_EQUAL(bytes_read, 0);  // Should return 0 for EOF
}

void test_read_partial_data(void)
{
    fused_inode_t *file = create_test_file("partial.txt", "/");
    CU_ASSERT_PTR_NOT_NULL(file);
    
    const char *test_data = "This is a longer file for partial reading";
    FILE *fp = fopen(file->backing_path, "wb");
    fwrite(test_data, 1, strlen(test_data), fp);
    fclose(fp);
    file->size = strlen(test_data);
    
    struct fuse_file_info fi = {0};
    fi.flags = O_RDONLY;
    fused_open("/partial.txt", &fi);
    
    // Read only 10 bytes
    char buf[256] = {0};
    int bytes_read = fused_read("/partial.txt", buf, 10, 0, &fi);
    
    CU_ASSERT_EQUAL(bytes_read, 10);
    CU_ASSERT_NSTRING_EQUAL(buf, "This is a ", 10);
}

void test_read_empty_file(void)
{
    fused_inode_t *file = create_test_file("empty.txt", "/");
    CU_ASSERT_PTR_NOT_NULL(file);
    file->size = 0;
    
    struct fuse_file_info fi = {0};
    fi.flags = O_RDONLY;
    fused_open("/empty.txt", &fi);
    
    char buf[256] = {0};
    int bytes_read = fused_read("/empty.txt", buf, sizeof(buf), 0, &fi);
    
    CU_ASSERT_EQUAL(bytes_read, 0);
}

// ============================================================================
// fused_write Tests
// ============================================================================

void test_write_basic_append(void)
{
    fused_inode_t *file = create_test_file("writetest.txt", "/");
    CU_ASSERT_PTR_NOT_NULL(file);
    file->size = 0;
    
    struct fuse_file_info fi = {0};
    fi.flags = O_WRONLY | O_APPEND;
    int result = fused_open("/writetest.txt", &fi);
    CU_ASSERT_EQUAL(result, 0);
    
    const char *test_data = "Hello, World!";
    int bytes_written = fused_write("/writetest.txt", test_data, 
                                     strlen(test_data), 0, &fi);
    
    CU_ASSERT_EQUAL(bytes_written, strlen(test_data));
    CU_ASSERT_EQUAL(file->size, strlen(test_data));
}

void test_write_multiple_appends(void)
{
    fused_inode_t *file = create_test_file("multiwrite.txt", "/");
    CU_ASSERT_PTR_NOT_NULL(file);
    file->size = 0;
    
    struct fuse_file_info fi = {0};
    fi.flags = O_WRONLY | O_APPEND;
    fused_open("/multiwrite.txt", &fi);
    
    // First write
    const char *data1 = "First line\n";
    int bytes1 = fused_write("/multiwrite.txt", data1, strlen(data1), 0, &fi);
    CU_ASSERT_EQUAL(bytes1, strlen(data1));
    
    // Second write
    const char *data2 = "Second line\n";
    int bytes2 = fused_write("/multiwrite.txt", data2, strlen(data2), 
                             file->size, &fi);
    CU_ASSERT_EQUAL(bytes2, strlen(data2));
    
    // Check total size
    CU_ASSERT_EQUAL(file->size, strlen(data1) + strlen(data2));
}

void test_write_reject_non_append(void)
{
    fused_inode_t *file = create_test_file("protected_write.txt", "/");
    CU_ASSERT_PTR_NOT_NULL(file);
    
    // Write some initial data
    const char *initial = "Initial content";
    FILE *fp = fopen(file->backing_path, "wb");
    fwrite(initial, 1, strlen(initial), fp);
    fclose(fp);
    file->size = strlen(initial);
    
    struct fuse_file_info fi = {0};
    fi.flags = O_WRONLY | O_APPEND;
    fused_open("/protected_write.txt", &fi);
    
    // Try to write at offset 0 (should be rejected)
    const char *overwrite = "OVERWRITE";
    int result = fused_write("/protected_write.txt", overwrite, 
                             strlen(overwrite), 0, &fi);
    
    CU_ASSERT_EQUAL(result, -EPERM);
    CU_ASSERT_EQUAL(file->size, strlen(initial));  // Size unchanged
}

void test_write_updates_metadata(void)
{
    fused_inode_t *file = create_test_file("metadata.txt", "/");
    CU_ASSERT_PTR_NOT_NULL(file);
    file->size = 0;
    
    time_t old_mtime = file->mtime;
    time_t old_ctime = file->ctime;
    
    sleep(1);  // Ensure time difference
    
    struct fuse_file_info fi = {0};
    fi.flags = O_WRONLY | O_APPEND;
    fused_open("/metadata.txt", &fi);
    
    const char *data = "Test data";
    fused_write("/metadata.txt", data, strlen(data), 0, &fi);
    
    CU_ASSERT_TRUE(file->mtime > old_mtime);
    CU_ASSERT_TRUE(file->ctime > old_ctime);
}

void test_write_and_read_consistency(void)
{
    fused_inode_t *file = create_test_file("readwrite.txt", "/");
    CU_ASSERT_PTR_NOT_NULL(file);
    file->size = 0;
    
    // Write data
    struct fuse_file_info fi_write = {0};
    fi_write.flags = O_WRONLY | O_APPEND;
    fused_open("/readwrite.txt", &fi_write);
    
    const char *test_data = "Data consistency test!";
    int bytes_written = fused_write("/readwrite.txt", test_data, 
                                     strlen(test_data), 0, &fi_write);
    CU_ASSERT_EQUAL(bytes_written, strlen(test_data));
    
    // Read back the data
    struct fuse_file_info fi_read = {0};
    fi_read.flags = O_RDONLY;
    fused_open("/readwrite.txt", &fi_read);
    
    char buf[256] = {0};
    int bytes_read = fused_read("/readwrite.txt", buf, sizeof(buf), 0, &fi_read);
    
    CU_ASSERT_EQUAL(bytes_read, strlen(test_data));
    CU_ASSERT_STRING_EQUAL(buf, test_data);
}

void test_write_large_data(void)
{
    fused_inode_t *file = create_test_file("largefile.txt", "/");
    CU_ASSERT_PTR_NOT_NULL(file);
    file->size = 0;
    
    struct fuse_file_info fi = {0};
    fi.flags = O_WRONLY | O_APPEND;
    fused_open("/largefile.txt", &fi);
    
    // Write 10KB of data
    const size_t data_size = 10240;
    char *large_data = malloc(data_size);
    memset(large_data, 'A', data_size);
    
    int bytes_written = fused_write("/largefile.txt", large_data, 
                                     data_size, 0, &fi);
    
    CU_ASSERT_EQUAL(bytes_written, data_size);
    CU_ASSERT_EQUAL(file->size, data_size);
    
    free(large_data);
}

void test_read_after_multiple_writes(void)
{
    fused_inode_t *file = create_test_file("sequential.txt", "/");
    CU_ASSERT_PTR_NOT_NULL(file);
    file->size = 0;
    
    struct fuse_file_info fi_write = {0};
    fi_write.flags = O_WRONLY | O_APPEND;
    fused_open("/sequential.txt", &fi_write);
    
    // Write multiple chunks
    const char *chunk1 = "Line1\n";
    const char *chunk2 = "Line2\n";
    const char *chunk3 = "Line3\n";
    
    fused_write("/sequential.txt", chunk1, strlen(chunk1), 0, &fi_write);
    fused_write("/sequential.txt", chunk2, strlen(chunk2), file->size, &fi_write);
    fused_write("/sequential.txt", chunk3, strlen(chunk3), file->size, &fi_write);
    
    // Read all data
    struct fuse_file_info fi_read = {0};
    fi_read.flags = O_RDONLY;
    fused_open("/sequential.txt", &fi_read);
    
    char buf[256] = {0};
    int bytes_read = fused_read("/sequential.txt", buf, sizeof(buf), 0, &fi_read);
    
    CU_ASSERT_EQUAL(bytes_read, strlen(chunk1) + strlen(chunk2) + strlen(chunk3));
    CU_ASSERT_STRING_EQUAL(buf, "Line1\nLine2\nLine3\n");
}

// ============================================================================
// fused_mkdir Tests
// ============================================================================

void test_mkdir_success(void)
{
    int result = fused_mkdir("/newdir", 0755);
    CU_ASSERT_EQUAL(result, 0);

    struct stat stbuf;
    result = fused_getattr("/newdir", &stbuf);
    CU_ASSERT_EQUAL(result, 0);
    CU_ASSERT_TRUE(S_ISDIR(stbuf.st_mode));

    readdir_capture_t capture = {0};
    int rc = fused_readdir("/", &capture, test_filler, 0, NULL);
    CU_ASSERT_EQUAL(rc, 0);

    int found = 0;
    for (int i = 0; i < capture.count; i++)
    {
        if (strcmp(capture.names[i], "newdir") == 0)
        {
            found = 1;
            break;
        }
    }
    CU_ASSERT_TRUE(found);
}

void test_mkdir_existing(void)
{
    int result = fused_mkdir("/existsdir", 0755);
    CU_ASSERT_EQUAL(result, 0);

    result = fused_mkdir("/existsdir", 0755);
    CU_ASSERT_EQUAL(result, -EEXIST);
}

void test_mkdir_parent_nonexistent(void)
{
    int result = fused_mkdir("/no_parent/child", 0755);
    CU_ASSERT_EQUAL(result, -ENOENT);
}

void test_mkdir_parent_not_directory(void)
{
    fused_inode_t *file = create_test_file("notdir", "/");
    CU_ASSERT_PTR_NOT_NULL(file);

    int result = fused_mkdir("/notdir/child", 0755);
    CU_ASSERT_EQUAL(result, -ENOENT);
}

// ============================================================================
// fused_rmdir Tests
// ============================================================================

void test_rmdir_success(void)
{
    int result = fused_mkdir("/toremove", 0755);
    CU_ASSERT_EQUAL(result, 0);

    result = fused_rmdir("/toremove");
    CU_ASSERT_EQUAL(result, 0);

    struct stat stbuf;
    result = fused_getattr("/toremove", &stbuf);
    CU_ASSERT_EQUAL(result, -ENOENT);

    readdir_capture_t capture = {0};
    int rc = fused_readdir("/", &capture, test_filler, 0, NULL);
    CU_ASSERT_EQUAL(rc, 0);

    int found = 0;
    for (int i = 0; i < capture.count; i++)
    {
        if (strcmp(capture.names[i], "toremove") == 0)
        {
            found = 1;
            break;
        }
    }
    CU_ASSERT_FALSE(found);
}

void test_rmdir_nonempty(void)
{
    int result = fused_mkdir("/parent", 0755);
    CU_ASSERT_EQUAL(result, 0);

    fused_inode_t *parent = path_to_inode("/parent");
    CU_ASSERT_PTR_NOT_NULL(parent);

    // Create a child file and move it under /parent
    fused_inode_t *child = create_test_file("child.txt", "/");
    CU_ASSERT_PTR_NOT_NULL(child);

    // Remove child from root's entries
    fused_inode_t *root = &g_state->inodes[0];
    int idx = -1;
    for (int i = 0; i < root->n_children; i++)
    {
        if (root->child_inodes[i] == child->ino)
        {
            idx = i;
            break;
        }
    }
    if (idx != -1)
    {
        for (int j = idx; j < root->n_children - 1; j++)
        {
            strncpy(root->child_names[j], root->child_names[j + 1], MAX_NAME);
            root->child_inodes[j] = root->child_inodes[j + 1];
        }
        root->n_children--;
    }

    // Add child to parent
    strncpy(parent->child_names[parent->n_children], "child.txt", MAX_NAME - 1);
    parent->child_names[parent->n_children][MAX_NAME - 1] = '\0';
    parent->child_inodes[parent->n_children] = child->ino;
    parent->n_children++;

    // Now attempt to remove non-empty directory
    result = fused_rmdir("/parent");
    CU_ASSERT_EQUAL(result, -ENOTEMPTY);
}

void test_rmdir_nonexistent(void)
{
    int result = fused_rmdir("/doesnotexist");
    CU_ASSERT_EQUAL(result, -ENOENT);
}

void test_rmdir_root_busy(void)
{
    int result = fused_rmdir("/");
    CU_ASSERT_EQUAL(result, -EBUSY);
}

void test_rmdir_not_directory(void)
{
    fused_inode_t *file = create_test_file("notdir2", "/");
    CU_ASSERT_PTR_NOT_NULL(file);

    int result = fused_rmdir("/notdir2");
    CU_ASSERT_EQUAL(result, -ENOTDIR);
}


// touch
// dependent on fused_read
void test_create_successful(void)
{
    struct fuse_file_info fi = {0};
    fi.flags = O_WRONLY;
    const char* path = "/file1834.txt";
    int result = fused_create(path, 0755, &fi);
    CU_ASSERT_EQUAL(result, 0);

    char buf[10];
    result = fused_read(path, buf, 3, 0, &fi);
    
    // should not be able to read anything from the stream
    CU_ASSERT_EQUAL(result, 0);
}

void test_create_file_exists(void)
{
    struct fuse_file_info fi = {0};
    fi.flags = O_WRONLY;
  const char* path = "/file_create_2.txt";
    int result = fused_create(path, 0755, &fi);
    CU_ASSERT_EQUAL(result, 0);
    result = fused_create(path, 0755, &fi);
    CU_ASSERT_NOT_EQUAL(result, 0);

}
// create should error when the parent directory doesn't exist
void test_create_parent_dne(void)
{
    struct fuse_file_info fi = {0};
    fi.flags = O_WRONLY;
  const char* path = "create_test/file.txt";
    int result = fused_create(path, 0755, &fi);
    CU_ASSERT_NOT_EQUAL(result, 0);
}


// rename
// dependent on fused_create and fused_write and fused_read
void test_rename_successful(void)
{
    struct fuse_file_info fi = {0};
    fi.flags = O_WRONLY;
  const char* path = "/file3.txt";
    int result = fused_create(path, 0755, &fi);
  const char* write_buf = "this will be written to a file";
  int write_buf_len = strlen(write_buf);
    
  fused_write(path, write_buf, write_buf_len, 0, &fi);
  const char* newpath= "/renametestfile.txt" ;
    result = fused_rename(path, newpath);
    CU_ASSERT_EQUAL(result, 0);
    
    fused_inode_t* inode = path_to_inode(path);
    CU_ASSERT_PTR_NULL(inode);

    inode = path_to_inode(newpath);
    CU_ASSERT_PTR_NOT_NULL(inode);
    char buf[write_buf_len+1];
    result = fused_read(newpath, buf, write_buf_len, 0, &fi);
    buf[write_buf_len] = '\0';
    CU_ASSERT_STRING_EQUAL(write_buf, buf);
}
// if dest does not exist, should throw an error
void test_rename_invalid_source(void)
{
    struct fuse_file_info fi = {0};
    fi.flags = O_WRONLY;
  const char* path = "/file4.txt";

  const char* write_buf = "this will be written to a file";
  int write_buf_len = strlen(write_buf);
  fused_write(path, write_buf, write_buf_len, 0, &fi);

  const char* newpath= "/renametestfile2.txt";

    int result = fused_rename(path, newpath);
    CU_ASSERT_NOT_EQUAL(result, 0);

    fused_inode_t* inode = path_to_inode(path);
    CU_ASSERT_PTR_NULL(inode);

    inode = path_to_inode(newpath);
    CU_ASSERT_PTR_NULL(inode);
}

// if parent directory of destination path does not exist, should throw an error
void test_rename_invalid_dest(void)
{
    struct fuse_file_info fi = {0};
    fi.flags = O_WRONLY;
  const char* path = "/file5.txt";

    int result = fused_create(path, 0755, &fi);
  const char* write_buf = "this will be written to a file";
  int write_buf_len = strlen(write_buf);
  fused_write(path, write_buf, write_buf_len, 0, &fi);
  const char* newpath= "/nonexistent_dir/renametestfile.txt" ;
    result = fused_rename(path, newpath);
    CU_ASSERT_NOT_EQUAL(result, 0);

  // make sure original file wasn't deleted
    fused_inode_t* inode = path_to_inode(newpath);
    CU_ASSERT_PTR_NULL(inode);
    inode = path_to_inode("/nonexistent_dir");
    CU_ASSERT_PTR_NULL(inode);
    inode = path_to_inode(path);
    CU_ASSERT_PTR_NOT_NULL(inode);

    char buf[write_buf_len+1];
    result = fused_read(path, buf, write_buf_len, 0, &fi);
    buf[write_buf_len] = '\0';
    CU_ASSERT_STRING_EQUAL(write_buf, buf);
}


// rename a file to itself
void test_rename_same_source_as_dest(void)
{
    struct fuse_file_info fi = {0};
    fi.flags = O_WRONLY;
  const char* path = "/file6.txt";
    int result = fused_create(path, 0755, &fi);

  const char* write_buf = "this will be written to a file";
  int write_buf_len = strlen(write_buf);
  fused_write(path, write_buf, write_buf_len, 0, &fi);
    result = fused_rename(path, path);
    CU_ASSERT_EQUAL(result, 0);

  // make sure file wasn't deleted
    fused_inode_t* inode = path_to_inode(path);
    CU_ASSERT_PTR_NOT_NULL(inode);

    char buf[write_buf_len+1];
    result = fused_read(path, buf, write_buf_len, 0, &fi);
    buf[write_buf_len] = '\0';
    CU_ASSERT_STRING_EQUAL(write_buf, buf);
}

// remove a file
// relies on create:
void test_remove_successful(void)
{
    struct fuse_file_info fi = {0};
    fi.flags = O_WRONLY;

  const char* path = "/remove.txt";
    int result = fused_create(path, 0755, &fi);

    result = fused_unlink(path);
    CU_ASSERT_EQUAL(result, 0);
    // make sure file wasn't deleted
    fused_inode_t* inode = path_to_inode(path);
    CU_ASSERT_PTR_NULL(inode);

    result = fused_unlink(path);
    CU_ASSERT_NOT_EQUAL(result, 0);
}
// Main Test Runner
// ============================================================================

int main()
{
    CU_pSuite suite_getattr = NULL;
    CU_pSuite suite_readdir = NULL;
    CU_pSuite suite_open = NULL;
    CU_pSuite suite_read = NULL;
    CU_pSuite suite_write = NULL;
    CU_pSuite suite_mkdir = NULL;
    CU_pSuite suite_rmdir = NULL;
    CU_pSuite suite_create = NULL;
    CU_pSuite suite_rename = NULL;
    CU_pSuite suite_unlink = NULL;
    
    // Initialize CUnit
    if (CUE_SUCCESS != CU_initialize_registry())
    {
        return CU_get_error();
    }
    
    // Create test suites
    suite_getattr = CU_add_suite("fused_getattr Tests", init_suite, clean_suite);
    suite_readdir = CU_add_suite("fused_readdir Tests", init_suite, clean_suite);
    suite_open = CU_add_suite("fused_open Tests", init_suite, clean_suite);
    suite_read = CU_add_suite("fused_read Tests", init_suite, clean_suite);
    suite_write = CU_add_suite("fused_write Tests", init_suite, clean_suite);
    suite_mkdir = CU_add_suite("fused_mkdir Tests", init_suite, clean_suite);
    suite_rmdir = CU_add_suite("fused_rmdir Tests", init_suite, clean_suite);
    suite_create = CU_add_suite("fused_create Tests", init_suite, clean_suite);
    suite_rename = CU_add_suite("fused_rename Tests", init_suite, clean_suite);
    suite_unlink = CU_add_suite("fused_unlink Tests", init_suite, clean_suite);

    
    if (!suite_getattr || !suite_readdir || !suite_open || !suite_read || !suite_write || !suite_mkdir || !suite_rmdir)
    {
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    // Add getattr tests
    CU_add_test(suite_getattr, "Root directory", test_getattr_root_directory);
    CU_add_test(suite_getattr, "Nonexistent file", test_getattr_nonexistent_file);
    CU_add_test(suite_getattr, "Regular file", test_getattr_regular_file);
    CU_add_test(suite_getattr, "File attributes", test_getattr_file_attributes);
    CU_add_test(suite_getattr, "Block calculation", test_getattr_block_calculation);
    
    // Add readdir tests
    CU_add_test(suite_readdir, "Empty root directory", test_readdir_empty_root);
    CU_add_test(suite_readdir, "Directory with files", test_readdir_with_files);
    CU_add_test(suite_readdir, "Nonexistent directory", test_readdir_nonexistent_directory);
    CU_add_test(suite_readdir, "File not directory", test_readdir_file_not_directory);
    
    // Add open tests
    CU_add_test(suite_open, "Open for reading", test_open_file_for_reading);
    CU_add_test(suite_open, "Open for append", test_open_file_for_append);
    CU_add_test(suite_open, "Reject non-append write", test_open_reject_non_append_write);
    CU_add_test(suite_open, "Reject RDWR without append", test_open_reject_rdwr_without_append);
    CU_add_test(suite_open, "Nonexistent file", test_open_nonexistent_file);
    CU_add_test(suite_open, "Directory as file", test_open_directory_as_file);
    CU_add_test(suite_open, "Updates atime", test_open_updates_atime);
    
    // Add read tests
    CU_add_test(suite_read, "Basic file read", test_read_basic_file);
    CU_add_test(suite_read, "Read with offset", test_read_with_offset);
    CU_add_test(suite_read, "Read beyond file size", test_read_beyond_file_size);
    CU_add_test(suite_read, "Read partial data", test_read_partial_data);
    CU_add_test(suite_read, "Read empty file", test_read_empty_file);
    
    // Add write tests
    CU_add_test(suite_write, "Basic append write", test_write_basic_append);
    CU_add_test(suite_write, "Multiple appends", test_write_multiple_appends);
    CU_add_test(suite_write, "Reject non-append", test_write_reject_non_append);
    CU_add_test(suite_write, "Updates metadata", test_write_updates_metadata);
    CU_add_test(suite_write, "Write and read consistency", test_write_and_read_consistency);
    CU_add_test(suite_write, "Write large data", test_write_large_data);
    CU_add_test(suite_write, "Read after multiple writes", test_read_after_multiple_writes);

    // Add mkdir tests
    CU_add_test(suite_mkdir, "Create directory (success)", test_mkdir_success);
    CU_add_test(suite_mkdir, "Create directory (existing)", test_mkdir_existing);
    CU_add_test(suite_mkdir, "Create directory (parent nonexistent)", test_mkdir_parent_nonexistent);
    CU_add_test(suite_mkdir, "Create directory (parent not dir)", test_mkdir_parent_not_directory);
    
    // Add rmdir tests
    CU_add_test(suite_rmdir, "Remove empty directory (success)", test_rmdir_success);
    CU_add_test(suite_rmdir, "Remove non-empty directory", test_rmdir_nonempty);
    CU_add_test(suite_rmdir, "Remove nonexistent directory", test_rmdir_nonexistent);
    CU_add_test(suite_rmdir, "Remove root (busy)", test_rmdir_root_busy);
    CU_add_test(suite_rmdir, "Remove not a directory", test_rmdir_not_directory);

    // add create tests
    CU_add_test(suite_create, "Successful create", test_create_successful);
    CU_add_test(suite_create, "Create to invalid path", test_create_parent_dne);
    CU_add_test(suite_create, "Create existing path", test_create_file_exists);

    CU_add_test(suite_rename, "Working rename", test_rename_successful);
    CU_add_test(suite_rename, "Rename to an invalid path", test_rename_invalid_dest);
    CU_add_test(suite_rename, "Rename a file that does not exist", test_rename_invalid_source);
    CU_add_test(suite_rename, "Rename a file to itself", test_rename_same_source_as_dest);

    CU_add_test(suite_unlink, "Remove a file, and a nonexistant file", test_remove_successful);
    
    // Run tests
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    
    int failures = CU_get_number_of_failures();
    
    CU_cleanup_registry();
    
    return failures;
}
