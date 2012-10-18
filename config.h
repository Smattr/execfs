#ifndef _EXECFS_CONFIG_H_
#define _EXECFS_CONFIG_H_

#include <stddef.h>
#include "entry.h"

#define PARSE_FAIL ((size_t)-1)
/* Parse a configuration file into directory entries. Returns an entry array
 * with its length in the output parameter len. PARSE_FAIL is returned in len
 * if parsing fails.
 */
entry_t *parse_config(size_t *len, char *filename,
        int(*debug_printf)(char *format, ...));

#endif
