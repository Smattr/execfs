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
#include <unistd.h>

#include "config.h"
#include "entry.h"
#include "fileops.h"
#include "globals.h"
#include "log.h"

/* Configuration file to read. */
static char *config_filename = NULL;

/* Debugging enabled. */
static int debug = 0;

/* Entries to present to the user in the mount point. */
entry_t **entries = NULL;
size_t entries_sz = 0;

/* Identity of the mounter. This will become the owner of all entries in the
 * mount point.
 */
uid_t uid;
gid_t gid;

/* Size in bytes to assign to each file. */
#define DEFAULT_SIZE (10 * 1024) /* 10 KB */
size_t size = DEFAULT_SIZE;

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
        {"help", no_argument, 0, '?'},
        {"log", required_argument, 0, 'l'},
        {"size", required_argument, 0, 's'},
        {"version", no_argument, 0, 'v'},
        {0, 0, 0, 0},
    };
    int index;
    int c;

    while ((c = getopt_long(argc, argv, "dc:f?", options, &index)) != -1) {
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
            } case 'l': {
                if (log_open(optarg) != 0) {
                    fprintf(stderr, "Failed to open log file %s\n", optarg);
                    errno = EINVAL;
                    return -1;
                }
                break;
            } case 's': {
                size_t sz = atoi(optarg);
                if (sz == 0) {
                    fprintf(stderr, "Invalid file size %s passed\n", optarg);
                    errno = EINVAL;
                    return -1;
                }
                size = sz;
                break;
            } case 'v': {
                printf("execfs version %s\n", VERSION);
                exit(0);
            } case '?': {
                printf("Usage: %s options -f fuse_options\n"
                       " -c, --config FILE     Read configuration from the given file. This argument\n"
                       "                       is required.\n"
                       " -d, --debug           Enable debugging output on startup.\n"
                       " -f, --fuse            Any arguments following this are interpreted as\n"
                       "                       arguments to be passed through to FUSE. This argument\n"
                       "                       must be used to terminate your execfs argument list.\n"
                       " -?, --help            Print this usage information.\n"
                       " -l, --log FILE        Write logging information to FILE. Without this\n"
                       "                       argument no logging is performed.\n"
                       " -s, --size SIZE       A size in bytes to report each file entry as having\n"
                       "                       (default 10). The argument exists because some programs\n"
                       "                       will stat a file before reading it and only read as\n"
                       "                       many bytes as its reported size. Increase this value if\n"
                       "                       you find the output of your executed commands is being\n"
                       "                       truncated when read.\n",
                       argv[0]);
                exit(0);
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
        fprintf(stderr, "No configuration file specified.\n");
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

    /* Set the owner of the mount point entries. */
    uid = geteuid();
    gid = getegid();

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
