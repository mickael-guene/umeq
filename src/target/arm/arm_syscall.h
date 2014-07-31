#include "arm_private.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM_SYSCALL__
#define __ARM_SYSCALL__ 1

extern int arm_uname(struct arm_target *context);
extern int arm_brk(struct arm_target *context);

#endif

#ifdef __cplusplus
}
#endif
