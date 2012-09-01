#ifndef _EXECFS_FILEOPS_H_
#define _EXECFS_FILEOPS_H_

/* Use newer version of FUSE API. */
#define FUSE_USE_VERSION 26
#include <fuse.h>

extern struct fuse_operations ops;

#endif
