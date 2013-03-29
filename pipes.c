/* Functionality that extends popen. */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include "pipes.h"

/* Shell to use when opening a file read/write. */
#define SHELL "/bin/sh"

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
            exit(1);
        }

        /* Overwrite our image with the command to execute. Note that exec will
         * only return if it fails.
         */
        (void)execl(SHELL, "sh", "-c", path, NULL);
        exit(1);
    } else {
        /* We are the parent. */

        /* Close the ends of the pipe we don't need. */
        close(input[0]); close(output[1]);

        /* Pack the file descriptors we do need into the handle. */
        *handle = pack_fds(output[0], input[1]);
        return 0;
    }
    /* Unreachable. */
}

int pipe_open(char *command, char *mode, uint64_t* out_handle) {

    if (!strcmp(mode, "r")) {
        FILE *f = popen(command, "r");
        if (f == NULL) {
            return -1;
        }
        *out_handle = pack_fds(fileno(f), 0);
        return 0;

    } else if (!strcmp(mode, "w")) {
        FILE *f = popen(command, "w");
        if (f == NULL) {
            return -1;
        }
        *out_handle = pack_fds(0, fileno(f));
        return 0;

    }
    
    /* "rw" */
    return popen_rw(command, out_handle);
}
