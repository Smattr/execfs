/* Implementations of all the FUSE operations for this file system. */

/* Use newer version of FUSE API. */
#define FUSE_USE_VERSION 26
#include <fuse.h>

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "entry.h"
#include "fileops.h"
#include "globals.h"
#include "log.h"

/* All the functions in this file are invoked as FUSE callbacks, which results
 * in assertion failures being invisible to the user. To provide meaningful
 * assertion functionality we provide our own assert() that prints failures to
 * the log.
 */
#undef assert
#ifdef NDEBUG
    #define assert(expr) ((void)0)
#else
    #define assert(expr) do { \
        if (!(expr)) { \
            LOG("%s:%d: %s: Assertion `%s' failed", __FILE__, __LINE__, __func__, #expr); \
            return -1; \
        } \
    } while(0)
#endif

#define BIT(n) (1ULL << (n))
#define MASK(n) (BIT(n) - 1)
#define R BIT(2)
#define W BIT(1)
#define X BIT(0)

#define RIGHTS_MASK 0x3

/* These functions pack two file descriptors (ints) into a uint64_t, so let's
 * check (at compile time) that they'll actually fit.
 */
typedef char _two_ints_fit_in_a_uint64_t
    [sizeof(uint64_t) >= 2 * sizeof(int) ? 1 : -1];
static int read_fd(uint64_t fh) { return fh & MASK(32); }
static int write_fd(uint64_t fh) { return fh >> 32; }
static uint64_t pack_fds(int rd, int wr) { return (uint64_t)wr << 32 | (uint64_t)rd; }

/* Shell to use when opening a file read/write. */
#define SHELL "/bin/sh"

/* Whether this path is the root of the mount point. */
static int is_root(const char *path) {
    return !strcmp("/", path);
}

/* Note: doing a linear search on the entries array is not an efficient way of
 * implementing a file system that will be under heavy load, but we assume that
 * there will be few entries in the file system and these will not be accessed
 * frequently.
 */
static entry_t *find_entry(const char *path) {
    if (path[0] != '/') {
        /* We were passed a path outside this mount point (?) */
        return NULL;
    }

    int i;
    for (i = 0; i < entries_sz; ++i) {
        if (!strcmp(path + 1, entries[i]->path)) {
            return entries[i];
        }
    }
    return NULL;
}

/* Determine the permissions of a given file in the context of the user
 * currently operating on it.
 */
static unsigned int access_rights(entry_t *entry) {
    struct fuse_context *context = fuse_get_context();
    unsigned int rights;

    if (context->uid == uid) {
        rights = (entry->u_r ? R : 0)
            | (entry->u_w ? W : 0)
            | (entry->u_x ? X : 0);
    } else if (context->gid == gid) {
        rights = (entry->g_r ? R : 0)
            | (entry->g_w ? W : 0)
            | (entry->g_x ? X : 0);
    } else {
        rights = (entry->o_r ? R : 0)
            | (entry->o_w ? W : 0)
            | (entry->o_x ? X : 0);
    }
    return rights;
}

/* Called when the file system is unmounted. */
static void exec_destroy(void *private_data) {
    LOG("destroy called (unmounting file system)");
    log_close();
}

static int exec_flush(const char *path, struct fuse_file_info *fi) {
    LOG("flush called on %s with handle %llu", path, fi->fh);
    if (is_root(path)) {
        return 0;
    }

    entry_t *e = find_entry(path);
    if (e == NULL) {
        return -ENOENT;
    }

    /* We don't need to flush at all because reading/writing is not done via
     * streams.
     */
    return 0;
}

static int exec_fsync(const char *path, int datasync, struct fuse_file_info *fi) {
    LOG("fsync called on %s (%s)", path, datasync ? "datasync" : "metadata only");
    if (is_root(path)) {
        return 0;
    }

    entry_t *e = find_entry(path);
    if (e == NULL) {
        return -ENOENT;
    }

    /* Like flush, no need to do anything for fsync because we are doing
     * unbuffered I/O.
     */
    return 0;
}

/* Start of "interesting" code. The guts of the implementation are below in
 * exec_getattr(), exec_open(), exec_read() and exec_write().
 */

static int exec_getattr(const char *path, struct stat *stbuf) {
    LOG("getattr called on %s", path);
    assert(stbuf != NULL);

    /* stbuf->st_dev is ignored. */
    /* stbuf->st_ino is ignored. */

    /* Mark every entry as owned by the mounter. */
    stbuf->st_uid = uid;
    stbuf->st_gid = gid;

    /* stbuf-st_rdev is irrelevant. */
    /* stbuf->st_blksize is ignored. */
    /* stbuf->st_blocks is ignored. */

    /* The current time is as good as any considering any process reading this
     * file may encounter different data to last time.
     */
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);

    if (is_root(path)) {
        stbuf->st_mode = S_IFDIR|S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
        stbuf->st_size = 0; /* FIXME: This should be set more appropriately. */
        stbuf->st_nlink = 1;
    } else {
        entry_t *e = find_entry(path);
        if (e == NULL) {
            return -ENOENT;
        }

        /* It would be nice to mark entries as FIFOs (S_IFIFO), but
         * irritatingly the kernel doesn't call FUSE handlers for FIFOs so we
         * never get read/write calls.
         */
        stbuf->st_mode = S_IFREG
            | (e->u_r ? S_IRUSR : 0)
            | (e->u_w ? S_IWUSR : 0)
            | (e->u_x ? S_IXUSR : 0)
            | (e->g_r ? S_IRGRP : 0)
            | (e->g_w ? S_IWGRP : 0)
            | (e->g_x ? S_IXGRP : 0)
            | (e->o_r ? S_IROTH : 0)
            | (e->o_w ? S_IWOTH : 0)
            | (e->o_x ? S_IXOTH : 0);
        stbuf->st_size = e->size == UNSPECIFIED_SIZE ? size : e->size;
        stbuf->st_nlink = 1;
    }

    return 0;
}

/* Basically popen(path, "rw"), but popen doesn't let you do this. */
static int popen_rw(const char *path, uint64_t *handle) {
    /* What we're going to do is create two pipes that we'll use as the read
     * and write file descriptors. Stdout and stdin, repsectively, in the
     * opened process need to connect to these pipes.
     */
    int input[2], output[2];
    if (pipe(input) != 0 || pipe(output) != 0) {
        return -1;
    }

    /* Flush standard streams to avoid aberrations after forking. This
     * shouldn't really be required as we aren't using any of these anyway.
     */
    fflush(stdout); fflush(stderr); fflush(stdin);

    pid_t pid = fork();
    if (pid == -1) {
        LOG("Failed to fork");
        close(input[0]);
        close(input[1]);
        close(output[0]);
        close(output[1]);
        return -1;
    } else if (pid == 0) {
        /* We are the child. */

        /* Close the ends of the pipe we don't need. */
        close(input[1]); close(output[0]);

        /* Overwrite our stdin and stdout such that they connect to the pipes.
         */
        if (dup2(input[0], STDIN_FILENO) < 0 || dup2(output[1], STDOUT_FILENO) < 0) {
            LOG("Failed to overwrite stdin/stdout after forking");
            exit(1);
        }

        /* Overwrite our image with the command to execute. Note that exec will
         * only return if it fails.
         */
        (void)execl(SHELL, "sh", "-c", path, NULL);
        LOG("Exec failed");
        exit(1);
    } else {
        /* We are the parent. */
        LOG("Forked off child %d to run %s", pid, path);

        /* Close the ends of the pipe we don't need. */
        close(input[0]); close(output[1]);

        /* Pack the file descriptors we do need into the handle. */
        assert(handle != NULL);
        *handle = pack_fds(output[0], input[1]);
        return 0;
    }
    assert(!"Unreachable");
}

static int exec_open(const char *path, struct fuse_file_info *fi) {
    assert(fi != NULL);
    LOG("open called on %s with flags %d", path, fi->flags);
    entry_t *e = find_entry(path);
    if (e == NULL) {
        return -ENOENT;
    }

    unsigned int entry_rights = access_rights(e);
    unsigned int rights = fi->flags & RIGHTS_MASK;

    if (((rights == O_RDONLY || rights == O_RDWR) && !(entry_rights & R)) ||
        ((rights == O_WRONLY || rights == O_RDWR) && !(entry_rights & W))) {
        return -EACCES;
    }

    LOG("Opening %s (%s) for %s", path, e->command,
        rights == O_RDONLY ? "read" :
        rights == O_WRONLY ? "write" : "read/write");

    /* Open the pipe and put the file descriptor in the low 32 bits of fi->fh
     * if it's open for reading and the high 32 bits if it's open for writing.
     * The reason for this is because we'll need two separate file descriptors
     * to do a read/write pipe, in which case we can pack them both into
     * fi->fh. Kind of handy that FUSE gives us 64 bits for a handle.
     */
    FILE *f;
    if (rights == O_RDONLY) {
        f = popen(e->command, "r");
        if (f == NULL) {
            LOG("Failed to popen %s for reading", e->command);
            return -EBADF;
        }
        fi->fh = pack_fds(fileno(f), 0);
    } else if (rights == O_WRONLY) {
        f = popen(e->command, "w");
        if (f == NULL) {
            LOG("Failed to popen %s for writing", e->command);
            return -EBADF;
        }
        fi->fh = pack_fds(0, fileno(f));
    } else {
        /* Opening a file for read/write is a bit more complicated because
         * popen doesn't let us do this directly.
         */
        if (popen_rw(e->command, &(fi->fh)) != 0) {
            LOG("Failed to open %s for read/write", e->command);
            return -EBADF;
        }
    }
    LOG("Handle %llu returned from popen", fi->fh);

    return 0;
}

static int exec_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    assert(fi != NULL);
    LOG("read of %d bytes from %s with handle %llu", size, path, fi->fh);

    /* 0 is actually a valid file descriptor, but we know it's stdin so there's
     * no way it can be the one we're about to read from.
     */
    assert(fi->fh != 0);
    assert(size <= SSIZE_MAX); /* read() is undefined when passed >SSIZE_MAX */
    int fd = read_fd(fi->fh);
    ssize_t sz = read(fd, buf, size);
    if (sz == -1) {
        LOG("read from %s failed with error %d", path, errno);
    } else {
        LOG("read from %s returned %d bytes", path, sz);
    }
    return sz;
}

static int exec_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    LOG("readdir called on %s", path);
    if (!is_root(path)) {
        /* Don't support subdirectories. */
        return -EBADF;
    }

    int i;
    for (i = offset; i < entries_sz; ++i) {
        if (filler(buf, entries[i]->path, NULL, i + 1) != 0) {
            return 0;
        }
    }
    return 0;
}

static int exec_release(const char *path, struct fuse_file_info *fi) {
    assert(fi != NULL);
    LOG("Releasing %s with handle %llu", path, fi->fh);
    assert(is_root(path) || find_entry(path) != NULL);
    assert(fi->fh != 0);
    int readfd = read_fd(fi->fh);
    int writefd = write_fd(fi->fh);
    if (readfd != 0) { /* File was opened for reading. */
        (void)close(readfd);
    }
    if (writefd != 0) { /* File was opened for writing. */
        (void)close(writefd);
    }
    return 0;
}

static int exec_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    assert(fi != NULL);
    LOG("write of %d bytes to %s with handle %llu", size, path, fi->fh);

    assert(fi->fh != 0);
    assert(size <= SSIZE_MAX); /* write() is undefined when passed >SSIZE_MAX */
    int fd = write_fd(fi->fh);
    ssize_t sz = write(fd, buf, size);
    if (sz == -1) {
        LOG("write to %s failed with error %d", path, errno);
    } else {
        LOG("write to %s of %d bytes", path, sz);
    }
    return sz;
}

/* Stub out all the irrelevant functions. */
#define FAIL_STUB(func, args...) \
    static int exec_ ## func(const char *path , ## args) { \
        LOG("Fail stubbed function %s called on %s", __func__, path); \
        return -EACCES; \
    }
#define NOP_STUB(func, args...) \
    static int exec_ ## func(const char *path , ## args) { \
        LOG("No-op stubbed function %s called on %s", __func__, path); \
        if (!is_root(path) && find_entry(path) == NULL) { \
            return -ENOENT; \
        } \
        return 0; \
    }
FAIL_STUB(bmap, size_t blocksize, uint64_t *idx);
FAIL_STUB(chmod, mode_t mode); /* Edit the config file to change permissions. */
FAIL_STUB(chown, uid_t uid, gid_t gid);
NOP_STUB(fsyncdir, int datasync, struct fuse_file_info *fi);
FAIL_STUB(link, const char *target);
FAIL_STUB(mkdir, mode_t mode); /* Subdirectories not supported. */
FAIL_STUB(mknod, mode_t mode, dev_t dev);
FAIL_STUB(readlink, char *buf, size_t size); /* Symlinks not supported. */
NOP_STUB(releasedir, struct fuse_file_info *fi);
FAIL_STUB(removexattr, const char *name);
FAIL_STUB(rename, const char *new_name);
FAIL_STUB(rmdir);
FAIL_STUB(setxattr, const char *name, const char *value, size_t size, int flags);
FAIL_STUB(symlink, const char *target);
NOP_STUB(truncate, off_t size);
FAIL_STUB(unlink); /* Edit the config file to remove entries. */
NOP_STUB(utime, struct utimbuf *buf);
NOP_STUB(utimens, const struct timespec tv[2]);
#undef FAIL_STUB
#undef NOP_STUB

#define OP(func) .func = &exec_ ## func
struct fuse_operations ops = {
    .flag_nullpath_ok = 0, /* Don't accept NULL paths. */
    // TODO access
    OP(bmap),
    OP(chmod),
    OP(chown),
    /* No need to implement create as open gets be called instead. */
    OP(destroy),
    // TODO fgetattr
    OP(flush),
    /* No need to implement ftruncate as truncate gets called instead. */
    OP(fsync),
    OP(fsyncdir),
    OP(getattr),
    // TODO getxattr
    // TODO init
    // TODO ioctl
    OP(link),
    // TODO listxattr
    /* No need to implement lock. Let the kernel handle flocking. */
    OP(mkdir),
    OP(mknod),
    OP(open),
    // TODO opendir
    // TODO poll
    OP(read),
    OP(readdir),
    OP(readlink),
    OP(release),
    OP(releasedir),
    OP(removexattr),
    OP(rename),
    OP(rmdir),
    OP(setxattr),
    // TODO statfs
    OP(symlink),
    OP(truncate),
    OP(unlink),
    OP(utime),
    OP(utimens),
    OP(write),
};
#undef OP
