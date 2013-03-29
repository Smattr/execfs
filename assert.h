#ifndef _EXECFS_ASSERT_H_
#define _EXECFS_ASSERT_H_

#include "log.h"

/* Many functions in this code are invoked as FUSE callbacks, which results
 * in assertion failures being invisible to the user. To provide meaningful
 * assertion functionality we provide our own assert() that prints failures to
 * the log.
 */
#undef assert
#ifdef NDEBUG
    #define assert(expr) ((void)0)
#else
    #define assert(expr) do { \
        if (!(expr)) { \
            LOG("%s:%d: %s: Assertion `%s' failed", __FILE__, __LINE__, __func__, #expr); \
            return -1; \
        } \
    } while(0)
#endif

#endif
