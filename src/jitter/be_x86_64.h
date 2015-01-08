#include "jitter.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __BE_X86_64__
#define __BE_X86_64__ 1

#define BE_X86_64_MIN_CONTEXT_SIZE     (32 * 1024)

struct backend *createX86_64Backend(void *memory, int size);
void deleteX86_64Backend(struct backend *backend);

#endif

#ifdef __cplusplus
}
#endif
