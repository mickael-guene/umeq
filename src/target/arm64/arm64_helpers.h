#include <stdlib.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM64_HELPERS__
#define __ARM64_HELPERS__ 1

#include "target64.h"

extern void arm64_hlp_dump(uint64_t regs);

#endif

#ifdef __cplusplus
}
#endif
