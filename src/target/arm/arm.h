#include <stdlib.h>
#include "loader.h"

#include "umeq.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM_TARGET__
#define __ARM_TARGET__ 1

#define ARM_CONTEXT_SIZE     (4096)

extern struct target_arch arm_arch;

#endif

#ifdef __cplusplus
}
#endif
