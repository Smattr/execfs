#ifndef _EXECFS_ENTRY_H_
#define _EXECFS_ENTRY_H_

#include <stddef.h>
#include <unistd.h>

typedef struct {
    char *path;
    int u_r : 1;
    int u_w : 1;
    int u_x : 1;
    int g_r : 1;
    int g_w : 1;
    int g_x : 1;
    int o_r : 1;
    int o_w : 1;
    int o_x : 1;
    char *command;
} entry_t;

extern entry_t **entries;
extern size_t entries_sz;

extern uid_t uid;
extern gid_t gid;

#endif
