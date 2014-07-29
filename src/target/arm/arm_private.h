#include <stdlib.h>

#include <stddef.h>
#include <stdint.h>
#include "target.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM_TARGET_PRIVATE__
#define __ARM_TARGET_PRIVATE__ 1

#define container_of(ptr, type, member) ({			\
    const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
    (type *)( (char *)__mptr - offsetof(type,member) );})

struct arm_registers {
    uint32_t r[16];
    uint32_t cpsr;
    uint32_t c13_tls2;
    uint32_t shifter_carry_out;
};

struct arm_target {
    struct target target;
    struct arm_registers regs;
    uint32_t pc;
    uint64_t sp_init;
    uint32_t isLooping;
    uint32_t exitStatus;
};

extern void disassemble_arm(struct target *target, struct irInstructionAllocator *irAlloc, uint64_t pc, int maxInsn);
extern void disassemble_thumb(struct target *target, struct irInstructionAllocator *irAlloc, uint64_t pc, int maxInsn);

#endif

#ifdef __cplusplus
}
#endif
