#ifndef _EXECFS_ENTRY_H_
#define _EXECFS_ENTRY_H_

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

#endif
