#ifdef __cplusplus
extern "C" {
#endif

#ifndef __SYSCALL64_64_PRIVATE__
#define __SYSCALL64_64_PRIVATE__ 1

#include "target64.h"

extern long fcntl_s6464(uint64_t fd_p, uint64_t cmd_p, uint64_t opt_p);
extern long execve_s6464(uint64_t filename_p, uint64_t argv_p, uint64_t envp_p);
extern long futex_s6464(uint64_t uaddr_p, uint64_t op_p, uint64_t val_p, uint64_t timeout_p, uint64_t uaddr2_p, uint64_t val3_p);
extern long pselect6_s6464(uint64_t nfds_p, uint64_t readfds_p, uint64_t writefds_p, uint64_t exceptfds_p, uint64_t timeout_p, uint64_t data_p);

#endif

#ifdef __cplusplus
}
#endif
