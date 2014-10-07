#include "arm_private.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM_SYSCALL__
#define __ARM_SYSCALL__ 1

#include "target32.h"

extern int arm_uname(struct arm_target *context);
extern int arm_brk(struct arm_target *context);
extern int arm_open(struct arm_target *context);
extern int arm_openat(struct arm_target *context);
extern int arm_rt_sigaction(struct arm_target *context);
extern int arm_clone(struct arm_target *context);
extern int arm_ptrace(struct arm_target *context);
extern int arm_sigaltstack(struct arm_target *context);
extern int arm_mmap(struct arm_target *context);
extern int arm_mmap2(struct arm_target *context);
extern int arm_munmap(struct arm_target *context);

#endif

#ifdef __cplusplus
}
#endif
