#ifndef _EXECFS_IMPL_H_
#define _EXECFS_IMPL_H_

#include <string.h>
#include "entry.h"
#include "fuse.h"

int file_open(entry_t *e, unsigned int rights, info_t *fi);
int file_read(char *buf, size_t size, off_t offset, info_t *fi);
int file_write(const char *buf, size_t size, off_t offset, info_t *fi);
int file_close(info_t *fi);

#endif
