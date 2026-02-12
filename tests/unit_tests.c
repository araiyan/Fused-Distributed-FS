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
// Main Test Runner
// ============================================================================

int main()
{
    CU_pSuite suite_getattr = NULL;
    CU_pSuite suite_readdir = NULL;
    CU_pSuite suite_open = NULL;
    
    // Initialize CUnit
    if (CUE_SUCCESS != CU_initialize_registry())
    {
        return CU_get_error();
    }
    
    // Create test suites
    suite_getattr = CU_add_suite("fused_getattr Tests", init_suite, clean_suite);
    suite_readdir = CU_add_suite("fused_readdir Tests", init_suite, clean_suite);
    suite_open = CU_add_suite("fused_open Tests", init_suite, clean_suite);
    
    if (!suite_getattr || !suite_readdir || !suite_open)
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
    
    // Run tests
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    
    int failures = CU_get_number_of_failures();
    
    CU_cleanup_registry();
    
    return failures;
}
