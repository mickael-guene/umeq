#include "arm64_private.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM64_SYSCALL__
#define __ARM64_SYSCALL__ 1

#include "target64.h"

extern long arm64_uname(struct arm64_target *context);
extern long arm64_brk(struct arm64_target *context);
extern long arm64_openat(struct arm64_target *context);
extern long arm64_fstat(struct arm64_target *context);
extern long arm64_rt_sigaction(struct arm64_target *context);
extern long arm64_fstatat64(struct arm64_target *context);

#endif

#ifdef __cplusplus
}
#endif
