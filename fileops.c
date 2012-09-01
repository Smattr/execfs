/* Use newer version of FUSE API. */
#define FUSE_USE_VERSION 26
#include <fuse.h>

#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "entry.h"
#include "fileops.h"

/* Note: doing a linear search on the entries array is not an efficient way of
 * implementing a file system that will be under heavy load, but we assume that
 * there will be few entries in the file system and these will not be accessed
 * frequently.
 */

static int exec_getattr(const char *path, struct stat *stbuf) {
    assert(stbuf != NULL);

    /* stbuf->st_dev is ignored. */
    /* stbuf->st_ino is ignored. */
    stbuf->st_uid = uid;
    stbuf->st_gid = gid;
    /* stbuf-st_rdev is irrelevant. */
    stbuf->st_size = 0;
    /* stbuf->st_blksize is ignored. */
    /* stbuf->st_blocks is ignored. */
    stbuf->st_atime = stbuf->st_mtime = stbuf->st_ctime = time(NULL);

    if (!strcmp("/", path)) {
        stbuf->st_mode = S_IFDIR|S_IRUSR|S_IXUSR|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH;
        stbuf->st_nlink = 1;
    } else {
        int i;
        for (i = 0; i < entries_sz; ++i) {
            if (!strcmp(entries[i]->path, path)) {
                stbuf->st_mode = S_IFIFO
                    | entries[i]->u_r ? S_IRUSR : 0
                    | entries[i]->u_w ? S_IWUSR : 0
                    | entries[i]->u_x ? S_IXUSR : 0
                    | entries[i]->g_r ? S_IRGRP : 0
                    | entries[i]->g_w ? S_IWGRP : 0
                    | entries[i]->g_x ? S_IXGRP : 0
                    | entries[i]->o_r ? S_IROTH : 0
                    | entries[i]->o_w ? S_IWOTH : 0
                    | entries[i]->o_x ? S_IXOTH : 0;
                stbuf->st_nlink = 1;
                break;
            }
        }
        if (i == entries_sz) {
            /* Matching entry not found. */
            errno = ENOENT;
            return -1;
        }
    }

    return 0;
}

struct fuse_operations ops = {
    .getattr = &exec_getattr,
};
