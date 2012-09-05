/* This program opens a file and reads and writes from it. It is designed to be
 * used on an execfs-mounted file and does some things that don't make sense on
 * a normal file.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s file\n", argv[0]);
        return -1;
    }

    FILE *f = fopen(argv[1], "w+");
    if (f == NULL) {
        fprintf(stderr, "Failed to open %s\n", argv[1]);
        return -1;
    }

    int c;
    while ((c = getchar()) != EOF) {
        fputc(c, f);
        /* Assume when we hit a newline that we will have a line to read. */
        if (c == '\n') {
            /* We need to seek the file to make sure we don't skip over
             * output.
             */
            if (fseek(f, 0, SEEK_SET) != 0) {
                fprintf(stderr, "fseek failed\n");
                return -1;
            }
            while ((c = fgetc(f)) != EOF && c != '\n') {
                putchar(c);
            }
            /* Ignore EOF on the stdout of the command because it may just mean
             * there is no output currently ready, not that the command has
             * finished emitting.
             */
            if (c != EOF) {
                putchar(c);
            }
        }
    }
    return 0;
}
