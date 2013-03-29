/* Underlying implementations of the interesting parts of this file system. */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "entry.h"
#include "fuse.h"
#include "pipes.h"

int file_open(entry_t *e, unsigned int rights, info_t *fi) {
    char *mode = rights == O_RDONLY ? "r" : rights == O_WRONLY ? "w" : "rw";

    handle_t *h = (handle_t*)malloc(sizeof(handle_t));
    if (h == NULL) {
        return -ENOMEM;
    }
    h->read_fd = h->write_fd = -1;
    h->buf = NULL;
    h->len = 0;
    h->cache = e->cache;

    if (pipe_open(e->command, mode, &h->read_fd, &h->write_fd) != 0) {
        free(h);
        return -EBADF;
    }

    typedef char _handle_t_fits_in_uint64_t[sizeof(h) <= sizeof(fi->fh) ? 1 : -1];
    fi->fh = (uint64_t)h;

    return 0;
}

int file_read(char *buf, size_t size, off_t offset, info_t *fi) {
    handle_t *h = (handle_t*)fi->fh;
    if (h->cache) {
        if (offset + size > h->len) {
            /* We need to fill up the cache. */
            int extra_bytes = offset + size - h->len;

            char *r_buf = (char*)realloc(h->buf, h->len + extra_bytes);
            if (r_buf == NULL) {
                return -ENOMEM;
            }
            h->buf = r_buf;

            ssize_t sz = read(h->read_fd, h->buf + h->len, extra_bytes);
            if (sz < extra_bytes) {
                /* Argh, we couldn't read enough. Let's realloc less so we
                 * don't leak memory.
                 */
                h->buf = (char*)realloc(h->buf, h->len + sz);
                /* FIXME: The above shouldn't fail because we're reducing
                 * h->buf, but we shouldn't really rely on that.
                 */
            }
            h->len += sz;
        }

        ssize_t len = size;
        if (len + offset > h->len) {
            len -= len + offset - h->len;
        }
        if (len < 0) {
            return 0;
        }
        memcpy(buf, h->buf + offset, len);
        return len;

    } else {
        return read(h->read_fd, buf, size);
    }
}

int file_write(const char *buf, size_t size, off_t offset, info_t *fi) {
    (void)offset;
    handle_t *h = (handle_t*)fi->fh;
    return write(h->write_fd, buf, size);
}

int file_close(info_t *fi) {
    handle_t *h = (handle_t*)fi->fh;
    if (h->read_fd != -1) {
        close(h->read_fd);
    }
    if (h->write_fd != -1) {
        close(h->write_fd);
    }
    if (h->cache && h->buf != NULL) {
        free(h->buf);
    }
    free(h);
    return 0;
}
