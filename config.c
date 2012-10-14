/* Configuration reading and parsing. */

#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Nicolas Devillard's INI parser. */
#include <dictionary.h>
#include <iniparser.h>

#include "config.h"
#include "entry.h"

#define BIT(n) (1UL << (n))
#define R BIT(2)
#define W BIT(1)
#define X BIT(0)

#define printf_arg int(*debug_printf)(char *format, ...)

#define DPRINTF(args...) \
    do { \
        if (debug_printf != NULL) { \
            debug_printf(args); \
        } \
    } while (0)

static char *make_key(char *prefix, char *suffix) {
    char *index = (char*)malloc(sizeof(char) *
        (strlen(prefix) + strlen(":") + strlen(suffix) + 1));
    if (index == NULL) {
        return NULL;
    }
    strcpy(index, prefix);
    strcat(index, ":");
    strcat(index, suffix);
    return index;
}

/* Wrapper around iniparser_getstring(). */
static char *get_string(dictionary *d, char *section, char *key) {
    assert(d != NULL);
    assert(section != NULL);
    assert(key != NULL);

    /* INI parser stores items indexed by "section:key" */
    char *index = make_key(section, key);
    if (index == NULL) {
        return NULL;
    }

    char *value = iniparser_getstring(d, index, NULL);
    free(index);
    return value;
}

/* Wrapper around iniparser_getint(). */
static int get_int(dictionary *d, char *section, char *key, int notfound) {
    assert(d != NULL);
    assert(section != NULL);
    assert(key != NULL);

    char *index = make_key(section, key);
    if (index == NULL) {
        return notfound;
    }

    int i = iniparser_getint(d, index, notfound);
    free(index);
    return i;
}

/* Parse a string into a directory entry. An entry is expected to be in the
 * form:
 *
 *  [path]
 *      access = permissions
 *      command = command to execute
 *
 * Permissions should be given in chmod numerical form. This function returns
 * NULL on failure.
 */
static entry_t *parse_entry(dictionary *d, char *name, printf_arg) {
    assert(d != NULL);
    assert(name != NULL);

    entry_t *e = (entry_t*)malloc(sizeof(entry_t));
    if (e == NULL) {
        errno = ENOMEM;
        goto parse_entry_fail;
    }
    memset(e, 0, sizeof(entry_t));
    e->size = UNSPECIFIED_SIZE;

    /* Copy path. */
    e->path = strdup(name);
    if (e->path == NULL) {
        errno = ENOMEM;
        goto parse_entry_fail;
    }

    /* Parse permissions. */
    char *tmp = get_string(d, name, "access");
    if (tmp == NULL) {
        DPRINTF("Missing access entry\n");
        goto parse_entry_fail;
    }
    unsigned int u, g, o;
    if (sscanf(tmp, "%1u%1u%1u", &u, &g, &o) != 3 ||
            u & ~(R|W|X) || g & ~(R|W|X) || o & ~(R|W|X)) {
        DPRINTF("Invalid permissions entry\n");
        goto parse_entry_fail;
    }
    e->u_r = !!(u & R); e->u_w = !!(u & W); e->u_x = !!(u & X);
    e->g_r = !!(g & R); e->g_w = !!(g & W); e->g_x = !!(g & X);
    e->o_r = !!(o & R); e->o_w = !!(o & W); e->o_x = !!(o & X);

    /* Parse command. */
    tmp = get_string(d, name, "command");
    if (tmp == NULL) {
        DPRINTF("Missing command entry\n");
        goto parse_entry_fail;
    }
    e->command = strdup(tmp);
    if (e->command == NULL) {
        errno = ENOMEM;
        goto parse_entry_fail;
    }

    /* Parse size. */
    e->size = get_int(d, name, "size", UNSPECIFIED_SIZE);

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
    assert(filename != NULL);
    *len = 0;
    entry_t **entries = NULL;
    entry_t *e = NULL;

    dictionary *d = NULL;

    if ((d = iniparser_load(filename)) == NULL) {
        goto parse_config_fail;
    }

    int sections = iniparser_getnsec(d);
    DPRINTF("%d sections found in configuration file.\n", sections);
    if (sections == -1) {
        goto parse_config_fail;
    }

    int i;
    for (i = 0; i < sections; ++i) {
        char *secname = iniparser_getsecname(d, i);
        if (secname == NULL) {
            goto parse_config_fail;
        }
        DPRINTF("Parsing section %s\n", secname);

        e = parse_entry(d, secname, debug_printf);
        if (e == NULL) {
            goto parse_config_fail;
        }

        DPRINTF("Appending entry %s\n", e->path);
        if (append_entry(&entries, e, *len, debug_printf) != 0) {
            DPRINTF("Failed to append entry.\n");
            goto parse_config_fail;
        }
        (*len)++;
    }

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
    if (e != NULL) {
        if (e->path != NULL) free(e->path);
        if (e->command != NULL) free(e->command);
        free(e);
    }
    *len = PARSE_FAIL;
    return NULL;
}
