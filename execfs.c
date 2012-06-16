/* Use newer version of FUSE API. */
#define FUSE_USE_VERSION 26
#include <fuse.h>

#include <errno.h>

#define UNIMPLEMENTED_ERROR (-ENOSYS)

/* Unimplemented functions. */
static int ef_readlink(const char *path, char *buf, size_t bufsize) {
    return UNIMPLEMENTED_ERROR;
}
static int ef_mknod(const char *path, mode_t mode, dev_t dev) {
    return UNIMPLEMENTED_ERROR;
}
static int ef_unlink(const char *path) {
    return UNIMPLEMENTED_ERROR;
}
static int ef_rmdir(const char *path) {
    return UNIMPLEMENTED_ERROR;
}
static int ef_symlink(const char *path1, const char *path2) {
    return UNIMPLEMENTED_ERROR;
}
static int ef_rename(const char *old, const char *new) {
    return UNIMPLEMENTED_ERROR;
}
static int ef_link(const char *path1, const char *path2) {
    return UNIMPLEMENTED_ERROR;
}

static struct fuse_operations ef_ops = {
    // getattr
    .readlink = &ef_readlink,
    // getdir
    .mknod = &ef_mknod,
    // mkdir
    .unlink = &ef_unlink,
    .rmdir = &ef_rmdir,
    .symlink = &ef_symlink,
    .rename = &ef_rename,
    .link = &ef_link,
    // chmod
    // chown
    // truncate
    // utime
    // open
    // read
    // write
    // statfs
    // flush
    // release
    // fsync
    // setxattr
    // getxattr
    // listxattr
    // removexattr
    // opendir
    // readdir
    // releasedir
    // fsyncdir
    // init
    // destroy
    // access
    // create
    // ftruncate
    // fgetattr
    // lock
    // utimens
    // bmap
    // flag_nullpath_ok
    // ioctl
    // poll
};

int main(int argc, char **argv) {
    return fuse_main(argc, argv, &ef_ops, NULL);
}
