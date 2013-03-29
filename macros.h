#ifndef _EXECFS_MACROS_H_
#define _EXECFS_MACROS_H_

#define BIT(n) (1ULL << (n))

#define R BIT(2)
#define W BIT(1)
#define X BIT(0)

#define RIGHTS_MASK 0x3

#endif
