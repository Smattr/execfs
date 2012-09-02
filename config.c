#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "entry.h"

#define BIT(n) (1UL << (n))
#define R BIT(2)
#define W BIT(1)
#define X BIT(0)

#define DELIMITERS "|"
#define BUFFER_SIZE 512

#define printf_arg int(*debug_printf)(char *format, ...)

#define DPRINTF(args...) \
    do { \
        if (debug_printf != NULL) { \
            debug_printf(args); \
        } \
    } while (0)

/* Read a line from the given file. Returns NULL on failure. */
static char *readline(FILE *fd, printf_arg) {
    size_t current_len = BUFFER_SIZE;
    char *ptr, *line = NULL;

    while (1) {
        ptr = (char*)realloc(line, current_len);
        if (ptr == NULL) {
            DPRINTF("Out of memory in %s\n", __func__);
            free(line);
            return NULL;
        }
        if (line == NULL) {
            /* First iteration of the loop. We need to make sure the contents
             * of line are sane in case fgets fails.
             */
            *ptr = '\0';
        }
        line = ptr;

        ptr = fgets(line + current_len - BUFFER_SIZE, BUFFER_SIZE, fd);
        if (ptr == NULL) {
            if (feof(fd)) {
                /* This line contained data and then EOF (no newline). */
                return line;
            } else {
                DPRINTF("Error while reading file\n");
                free(line);
                return NULL;
            }
            assert(!"Unreachable");
        }
        size_t len = strlen(line);
        assert(len < current_len);
        if (len == current_len - 1 && line[len - 1] != '\n') {
            DPRINTF("Expanding buffer to fit longer line\n");
            current_len += BUFFER_SIZE;
            continue;
        } else if (line[len - 1] == '\n') {
            /* Reached end of line, indicated by \n. */
            line[len - 1] = '\0';
        }

        /* We have either expanded the buffer to fit more characters (and
         * continued) or have read the entire line.
         */
        break;
    }
    return line;
}

/* Parse a string into a directory entry. An entry is expected to be in the
 * form:
 *  path/to/file|permissions|command to execute
 * Permissions should be given in chmod numerical form. This function returns
 * NULL on failure.
 */
static entry_t *parse_entry(char *s, printf_arg) {
    entry_t *e = (entry_t*)malloc(sizeof(entry_t));
    if (e == NULL) {
        DPRINTF("Out of memory in %s\n", __func__);
        goto parse_entry_fail;
    }
    e->path = e->command = NULL;

    /* Read the path field. */
    char *next = strtok(s, DELIMITERS);
    if (next == NULL) {
        DPRINTF("Failed to find path entry\n");
        errno = EINVAL;
        goto parse_entry_fail;
    }
    e->path = strdup(next);
    if (e->path == NULL) {
        DPRINTF("Out of memory in %s\n", __func__);
        goto parse_entry_fail;
    }

    /* Read permissions field. */
    next = strtok(NULL, DELIMITERS);
    if (next == NULL) {
        DPRINTF("Failed to find permissions entry\n");
        errno = EINVAL;
        goto parse_entry_fail;
    }
    unsigned int u, g, o;
    if (sscanf(next, "%1u%1u%1u", &u, &g, &o) != 3 ||
        u & ~(R|W|X) || g & ~(R|W|X) || o & ~(R|W|X)) {
        errno = EINVAL;
        DPRINTF("Invalid permissions entry\n");
        goto parse_entry_fail;
    }
    e->u_r = !!(u & R); e->u_w = !!(u & W); e->u_x = !!(u & X);
    e->g_r = !!(g & R); e->g_w = !!(g & W); e->g_x = !!(g & X);
    e->o_r = !!(o & R); e->o_w = !!(o & W); e->o_x = !!(o & X);

    /* Read command field. */
    next = strtok(NULL, DELIMITERS);
    if (next == NULL) {
        DPRINTF("Failed to find command entry\n");
        errno = EINVAL;
        goto parse_entry_fail;
    }
    e->command = strdup(next);
    if (e->command == NULL) {
        DPRINTF("Out of memory in %s\n", __func__);
        goto parse_entry_fail;
    }

    /* Check for trailing fields. */
    if (strtok(NULL, DELIMITERS) != NULL) {
        DPRINTF("Illegal trailing field\n");
        errno = EINVAL;
        goto parse_entry_fail;
    }

    return e;

parse_entry_fail:
    if (e != NULL) {
        if (e->path != NULL) free(e->path);
        if (e->command != NULL) free(e->command);
        free(e);
    }
    return NULL;
}

/* Append an entry to the existing array of directory entries. arr is the
 * address of the current entry array, item is the entry to append and len is
 * the current length of the array.
 */
static int append_entry(entry_t ***arr, entry_t *item, size_t len, printf_arg) {
    assert(arr != NULL);
    assert(item != NULL);
    entry_t **ptr = (entry_t**)realloc(*arr, sizeof(entry_t) * (len + 1));
    if (ptr == NULL) {
        DPRINTF("Out of memory in %s\n", __func__);
        return -1;
    }
    *arr = ptr;
    (*arr)[len] = item;
    return 0;
}

entry_t **parse_config(size_t *len, char *filename, printf_arg) {
    assert(len != NULL);
    *len = 0;
    entry_t **entries = NULL;
    int line_num = 0;

    assert(filename != NULL);
    FILE *fd = fopen(filename, "r");
    if (fd == NULL) {
        goto parse_config_fail;
    }

    while (!feof(fd)) {
        char *line = readline(fd, debug_printf);
        assert(line_num < INT_MAX);
        line_num++;
        if (line == NULL) {
            DPRINTF("Line %d: Readline returned NULL\n", line_num);
            continue;
        }

        if (!strcmp(line, "")) {
            DPRINTF("Line %d: Skipping blank line\n", line_num);
            free(line);
            continue;
        }

        entry_t *e = parse_entry(line, debug_printf);
        if (e == NULL) {
            DPRINTF("Line %d: Failed to parse entry.\n", line_num);
            free(line);
            goto parse_config_fail;
        }
        if (append_entry(&entries, e, *len, debug_printf) != 0) {
            DPRINTF("Line %d: Failed to append entry.\n", line_num);
            free(e);
            goto parse_config_fail;
        }
        (*len)++;
    }

    fclose(fd);
    return entries;

parse_config_fail:
    if (entries != NULL) {
        assert(len != NULL);
        int i;
        for (i = 0; i < *len; ++i) {
            assert(entries[i] != NULL);
            free(entries[i]->path);
            free(entries[i]->command);
            free(entries[i]);
        }
        free(entries);
    }
    if (fd != NULL) {
        fclose(fd);
    }
    *len = PARSE_FAIL;
    return NULL;
}
