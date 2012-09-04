#ifndef _EXECFS_GLOBALS_H_
#define _EXECFS_GLOBALS_H_

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include "entry.h"

extern entry_t **entries;
extern size_t entries_sz;

extern uid_t uid;
extern gid_t gid;

extern size_t size;

#endif
