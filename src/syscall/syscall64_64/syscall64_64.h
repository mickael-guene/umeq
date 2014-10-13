#ifdef __cplusplus
extern "C" {
#endif

#ifndef __SYSCALL64_64__
#define __SYSCALL64_64__ 1

#include "sysnum.h"

extern int syscall64_64(Sysnum no, uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5);

#endif

#ifdef __cplusplus
}
#endif
