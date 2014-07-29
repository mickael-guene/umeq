#ifdef __cplusplus
extern "C" {
#endif

#ifndef __SYSCALL32_64__
#define __SYSCALL32_64__ 1

#include "sysnum.h"

extern int syscall32_64(Sysnum no, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5);

#endif

#ifdef __cplusplus
}
#endif
