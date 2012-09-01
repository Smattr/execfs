/* Use newer version of FUSE API. */
#define FUSE_USE_VERSION 26
#include <fuse.h>

#include <assert.h>
#include <errno.h>
#include <getopt.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "entry.h"
#include "fileops.h"

/* Configuration file to read. */
static char *config_filename = NULL;

/* Debugging enabled. */
static int debug = 0;

/* Entries to present to the user in the mount point. */
entry_t **entries = NULL;
size_t entries_sz = 0;

/* Debugging functions. */
static void debug_dump_entries(void) {
    assert(entries_sz != PARSE_FAIL);
    size_t i;
    fprintf(stderr, "Entries table has %u entries:\n", (unsigned int)entries_sz);
    for (i = 0; i < entries_sz; ++i) {
        fprintf(stderr, " Path: %s; -%c%c%c%c%c%c%c%c%c; Exec: %s\n", entries[i]->path, 
            entries[i]->u_r?'r':'-', entries[i]->u_w?'w':'-', entries[i]->u_x?'x':'-',
            entries[i]->g_r?'r':'-', entries[i]->g_w?'w':'-', entries[i]->g_x?'x':'-',
            entries[i]->o_r?'r':'-', entries[i]->o_w?'w':'-', entries[i]->o_x?'x':'-',
            entries[i]->command);
    }
}
static int debug_printf(char *format, ...) {
    va_list ap;
    int result;

    va_start(ap, format);
    result = vfprintf(stderr, format, ap);
    va_end(ap);
    return result;
}

/* Parse command line arguments. Returns 0 on success, non-zero on failure. */
static int parse_args(int argc, char **argv, int *last) {
    static struct option options[] = {
        {"debug", no_argument, &debug, 1},
        {"config", required_argument, 0, 'c'},
        {"fuse", no_argument, 0, 'f'},
        {0, 0, 0, 0},
    };
    int index;
    int c;

    while ((c = getopt_long(argc, argv, "dc:f", options, &index)) != -1) {
        switch (c) {
            case 0: {
                /* This should have set a flag. */
                assert(options[index].flag != 0);
                break;
            } case 'c': {
                if (config_filename != NULL) {
                    /* Let a later config parameter override an earlier one. */
                    free(config_filename);
                    config_filename = NULL;
                }
                config_filename = strdup(optarg);
                if (config_filename == NULL) {
                    /* Out of memory. */
                    return -1;
                }
                break;
            } case 'd': {
                debug = 1;
                break;
            } case 'f': {
                /* We've hit the FUSE arguments. */
                assert(last != NULL);
                *last = optind;
                return 0;
            } default: {
                fprintf(stderr, "Unrecognised argument: %c\n", optopt);
                errno = EINVAL;
                return -1;
            }
        }
    }

    /* If we reached here, then we never found a -f/--fuse argument. */
    fprintf(stderr, "No -f/--fuse argument provided.\n");
    errno = EINVAL;
    return -1;
}

int main(int argc, char **argv) {
    int last_arg;
    if (parse_args(argc, argv, &last_arg) != 0) {
        perror("Failed to parse arguments");
        return -1;
    }

    if (config_filename == NULL) {
        fprintf(stderr, "No configuration file specified.\n"
                        "Usage: %s [--debug] --config filename --fuse fuse_arguments...\n", argv[0]);
        return -1;
    }

    entries = parse_config(&entries_sz, config_filename, debug ? &debug_printf : NULL);
    if (entries_sz == PARSE_FAIL) {
        perror("Failed to parse configuration file");
        return -1;
    }

    /* We don't need the configuration file any more. */
    free(config_filename);
    config_filename = NULL;

    if (debug) {
        debug_dump_entries();
    }

    /* Adjust arguments to hide any that we handled from FUSE. */
    --last_arg;
    assert(last_arg > 0);
    assert(last_arg < argc);
    argv[last_arg] = argv[0];
    argv += last_arg;
    argc -= last_arg;
    assert(argv[argc] == NULL);
    if (debug) {
        fprintf(stderr, "Altered argument parameters:\n");
        int i;
        for (i = 0; i < argc; ++i) {
            fprintf(stderr, "%d: %s\n", i, argv[i]);
        }
    }

    return fuse_main(argc, argv, &ops, NULL);
}
