#include <stdlib.h>
#include <stdint.h>

#include "jitter.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __TARGET__
#define __TARGET__ 1

struct target {
    void (*init)(struct target *target, struct target *prev_target, uint64_t entry, uint64_t stack_ptr, uint32_t signum, void *param);
    void (*disassemble)(struct target *target, struct irInstructionAllocator *irAlloc, uint64_t pc, int maxInsn);
    uint32_t (*isLooping)(struct target *target);
    uint32_t (*getExitStatus)(struct target *target);
};

#endif

#ifdef __cplusplus
}
#endif
