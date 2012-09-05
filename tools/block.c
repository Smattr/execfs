/* Read stdin into a buffer until EOF, then dump everything. This filter can be
 * used to ensure that each call to read ALWAYS fills the output buffer of the
 * caller if it doesn't hit EOF. That is, no early returns from read in the
 * case when there is more data, but it is not ready yet.
 */

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

#define CHUNK_SIZE 1024ULL

int main(int argc, char **argv) {
    unsigned long long len = 0, index = 0;
    char *buffer = NULL;
    int c;

    while ((c = getchar()) != EOF) {
        if (index + 1 >= len) {
            assert(LLONG_MAX - CHUNK_SIZE >= len);
            buffer = (char*)realloc(buffer, len + CHUNK_SIZE);
            if (buffer == NULL) {
                fprintf(stderr, "Out of memory\n");
                return -1;
            }
            len += CHUNK_SIZE;
        }
        buffer[index++] = c;
    }
    if (buffer != NULL) {
        buffer[index] = '\0';
        printf("%s", buffer);
    }
    return 0;
}
