/* Underlying implementations of the interesting parts of this file system. */

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "entry.h"
#include "fuse.h"
#include "pipes.h"

int file_open(entry_t *e, unsigned int rights, info_t *fi) {
    char *mode = rights == O_RDONLY ? "r" : rights == O_WRONLY ? "w" : "rw";

    if (pipe_open(e->command, mode, &fi->fh) != 0) {
        return -EBADF;
    }
    return 0;
}

int file_read(char *buf, size_t size, off_t offset, info_t *fi) {
    int fd = read_fd(fi->fh);
    (void)offset; /* TODO */
    ssize_t sz = read(fd, buf, size);
    return sz;
}

int file_write(const char *buf, size_t size, off_t offset, info_t *fi) {
    int fd = write_fd(fi->fh);
    (void)offset;
    ssize_t sz = write(fd, buf, size);
    return sz;
}

int file_close(info_t *fi) {
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
