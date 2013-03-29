/* Generic FUSE stuff. This should be the only file that includes <fuse.h> to
 * ensure that we're always using the same API.
 */

#ifndef _EXECFS_FUSE_H_
#define _EXECFS_FUSE_H_

/* Use newer version of FUSE API. */
#define FUSE_USE_VERSION 26
#include <fuse.h>

typedef struct fuse_file_info info_t;

#endif
