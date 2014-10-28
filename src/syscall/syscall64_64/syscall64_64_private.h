#ifdef __cplusplus
extern "C" {
#endif

#ifndef __SYSCALL64_64_PRIVATE__
#define __SYSCALL64_64_PRIVATE__ 1

#include "target64.h"

extern long fcntl_s6464(uint64_t fd_p, uint64_t cmd_p, uint64_t opt_p);

#endif

#ifdef __cplusplus
}
#endif
