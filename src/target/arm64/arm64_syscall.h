#include "arm64_private.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM64_SYSCALL__
#define __ARM64_SYSCALL__ 1

#include "target64.h"

long arm64_uname(struct arm64_target *context);
long arm64_brk(struct arm64_target *context);
long arm64_openat(struct arm64_target *context);

#endif

#ifdef __cplusplus
}
#endif
