/**
 * @file fused_main.c
 * @brief Main entry point for ShortsFS - YouTube Shorts optimized filesystem
 */

#include "fused_fs.h"
#include <syslog.h>
#include <unistd.h>

/**
 * @brief FUSE operations structure (append-only, optimized for shorts)
 */
static struct fuse_operations fused_oper = {
    .init       = fused_init,
    .destroy    = fused_destroy,
    .getattr    = fused_getattr,
    .readdir    = fused_readdir,
    .open       = fused_open,
    .read       = fused_read,
    .write      = fused_write,
    .create     = fused_create,
    .mkdir      = fused_mkdir,
    .rmdir      = fused_rmdir,
    .rename     = fused_rename,
    .utimens    = fused_utimens,
};
/**
 * @brief Main entry point
 */
int main(int argc, char *argv[]) {
    int ret;
    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    /* Run FUSE */
    ret = fuse_main(args.argc, args.argv, &fused_oper, NULL);
    
    /* Cleanup */
    fuse_opt_free_args(&args);
    return ret;
}
