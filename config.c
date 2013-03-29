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
#include "macros.h"

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
 * Permissions should be given in chmod numerical form. The first parameter is
 * taken as a location to write the parsed entry to and this function returns
 * non-zero on failure.
 */
static int parse_entry(entry_t *e, dictionary *d, char *name, printf_arg) {
    assert(d != NULL);
    assert(name != NULL);
    assert(e != NULL);

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

    return 0;

parse_entry_fail:
    if (e->path != NULL) free(e->path);
    if (e->command != NULL) free(e->command);
    return -1;
}

entry_t *parse_config(size_t *len, char *filename, printf_arg) {
    assert(len != NULL);
    assert(filename != NULL);
    *len = 0;
    entry_t *entries = NULL;

    dictionary *d = NULL;

    if ((d = iniparser_load(filename)) == NULL) {
        goto parse_config_fail;
    }

    *len = iniparser_getnsec(d);
    DPRINTF("%d sections found in configuration file.\n", *len);
    if (*len == -1) {
        goto parse_config_fail;
    }
    entries = (entry_t*)malloc(sizeof(entry_t) * *len);
    if (entries == NULL) {
        *len = 0;
        goto parse_config_fail;
    }
    memset(entries, 0, sizeof(entry_t) * *len);

    int i;
    for (i = 0; i < *len; ++i) {
        char *secname = iniparser_getsecname(d, i);
        if (secname == NULL) {
            goto parse_config_fail;
        }
        DPRINTF("Parsing section %s\n", secname);

        if (parse_entry(&entries[i], d, secname, debug_printf) != 0) {
            goto parse_config_fail;
        }
    }

    return entries;

parse_config_fail:
    assert(len != NULL);
    if (entries != NULL) {
        int i;
        for (i = 0; i < *len; ++i) {
            free(entries[i].path);
            free(entries[i].command);
        }
        free(entries);
    }
    *len = PARSE_FAIL;
    return NULL;
}
