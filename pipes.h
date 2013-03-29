#ifndef _EXECFS_PIPES_H_
#define _EXECFS_PIPES_H_

/* Thin wrapper around popen with some extra functionality. Returns a packed
 * set of file descriptors in read_fd, write_fd. Returns non-zero on failure.
 */
int pipe_open(char *command, char *mode, int *read_fd, int *write_fd);

#endif
