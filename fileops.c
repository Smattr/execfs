/* Use newer version of FUSE API. */
#define FUSE_USE_VERSION 26
#include <fuse.h>

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "entry.h"
#include "fileops.h"
#include "globals.h"
#include "log.h"

#define BIT(n) (1UL << (n))
#define R BIT(2)
#define W BIT(1)
#define X BIT(0)

#define RIGHTS_MASK 0x3

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

static void exec_destroy(void *private_data) {
    log_close();
}

static int exec_getattr(const char *path, struct stat *stbuf) {
    assert(stbuf != NULL);

    /* stbuf->st_dev is ignored. */
    /* stbuf->st_ino is ignored. */
    stbuf->st_uid = uid;
    stbuf->st_gid = gid;
    /* stbuf-st_rdev is irrelevant. */
    /* stbuf->st_blksize is ignored. */
    /* stbuf->st_blocks is ignored. */
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);

    if (is_root(path)) {
        stbuf->st_mode = S_IFDIR|S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
        stbuf->st_size = 0;
        stbuf->st_nlink = 1;
    } else {
        entry_t *e = find_entry(path);
        if (e == NULL) {
            return -ENOENT;
        }

        /* It would be nice to mark entries as FIFOs (S_IFIFO), but
         * irritatingly the kernel doesn't call FUSE handlers for FIFOs so we
         * never get read/write calls. To further complicate matters some
         * programs like cat attempt to be clever and stat the size of a file
         * to see how much they should read. To get around this we need to set
         * a reasonably large file size.
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
        stbuf->st_size = 1024;
        stbuf->st_nlink = 1;
    }

    return 0;
}

static int exec_open(const char *path, struct fuse_file_info *fi) {
    entry_t *e = find_entry(path);
    if (e == NULL) {
        return -ENOENT;
    }

    assert(fi != NULL);
    unsigned int entry_rights = access_rights(e);
    unsigned int rights = fi->flags & RIGHTS_MASK;

    if (((rights == O_RDONLY || rights == O_RDWR) && !(entry_rights & R)) ||
        ((rights == O_WRONLY || rights == O_RDWR) && !(entry_rights & W))) {
        return -EACCES;
    }

    LOG("Opening %s (%s) for %s", path, e->command,
        rights == O_RDONLY ? "read" :
        rights == O_WRONLY ? "write" : "read/write");

    /* TODO: rw pipes. */
    fi->fh = (uint64_t)popen(e->command, rights == O_RDONLY ? "r" : "w");
    if (fi->fh == 0) {
        LOG("Failed to popen %s", e->command);
        return -EBADF;
    }
    LOG("Handle %p returned from popen", (FILE*)fi->fh);

    return 0;
}

static int exec_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    assert(fi != NULL);
    assert(fi->fh != 0);

    LOG("read called on %s (popen handle %p)", path, (FILE*)fi->fh);

    size_t sz = fread(buf, 1, size, (FILE*)fi->fh);
    if (sz == -1) {
        LOG("read from %s failed with error %d", path, errno);
    } else {
        LOG("read from %s returned %d bytes", path, sz);
    }
    return sz;
}

static int exec_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
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
    assert(is_root(path) || find_entry(path) != NULL);
    assert(fi != NULL);
    assert(fi->fh != 0);
    (void)pclose((FILE*)fi->fh);
    return 0;
}

#define FAIL_STUB(func, args...) \
    static int exec_ ## func(const char *path , ## args) { \
        LOG("Fail stubbed function %s called on %s", __func__, path); \
        return -EACCES; \
    }
FAIL_STUB(mkdir, mode_t mode); /* Subdirectories not supported. */
FAIL_STUB(mknod, mode_t mode, dev_t dev);
FAIL_STUB(readlink, char *buf, size_t size); /* Symlinks not supported. */
FAIL_STUB(rename, const char *new_name);
FAIL_STUB(rmdir);
FAIL_STUB(unlink); /* Edit the config file to remove entries. */
#undef FAIL_STUB

#define OP(func) .func = &exec_ ## func
struct fuse_operations ops = {
    OP(destroy),
    OP(getattr),
    OP(mkdir),
    OP(mknod),
    OP(open),
    OP(read),
    OP(readdir),
    OP(readlink),
    OP(release),
    OP(rename),
    OP(rmdir),
    OP(unlink),
};
#undef OP
