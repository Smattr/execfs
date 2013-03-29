#ifndef _EXECFS_PIPES_H_
#define _EXECFS_PIPES_H_

#include <stdint.h>

#define BIT(n) (1ULL << (n))
#define MASK(n) (BIT(n) - 1)

/* The following functions pack two file descriptors (ints) into a uint64_t, so
 * let's check (at compile time) that they'll actually fit.
 */
typedef char _two_ints_fit_in_a_uint64_t
    [sizeof(uint64_t) >= 2 * sizeof(int) ? 1 : -1];

static inline int read_fd(uint64_t handle) {
    return handle & MASK(32);
}

static inline int write_fd(uint64_t handle) {
    return handle >> 32;
}

static inline uint64_t pack_fds(int rd, int wr) {
    return (uint64_t)wr << 32 | (uint64_t)rd;
}

/* Thin wrapper around popen with some extra functionality. Returns a packed
 * set of file descriptors in 'out_handle'. Returns non-zero on failure.
 */
int pipe_open(char *command, char *mode, uint64_t *out_handle);

#endif
