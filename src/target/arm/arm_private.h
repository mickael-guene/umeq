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
    uint32_t reg_itstate;
    /* FIXME: move both field below to struct arm_target (need to have helpers to use tlsarea before) */
    /* is_in_syscall : indicate if we are inside a syscall or not
        0 : not inside a syscall
        1 : inside syscall entry sequence
        2 : inside syscall exit sequence
    */
    uint32_t is_in_syscall;
    uint32_t dummy;
    union {
        uint64_t d[32];
        uint32_t s[64];
    } e;
};

struct arm_target {
    struct target target;
    struct arm_registers regs;
    uint32_t pc;
    uint64_t sp_init;
    uint32_t isLooping;
    uint32_t exitStatus;
    uint64_t exclusive_value;
    uint32_t disa_itstate;
    uint32_t is_in_signal;
};

extern void disassemble_arm(struct target *target, struct irInstructionAllocator *irAlloc, uint64_t pc, int maxInsn);
extern void disassemble_thumb(struct target *target, struct irInstructionAllocator *irAlloc, uint64_t pc, int maxInsn);
extern void arm_setup_brk(void);
extern void arm_load_image(int argc, char **argv, void **additionnal_env, void **unset_env, void *target_argv0, uint64_t *entry, uint64_t *stack);

#endif

#ifdef __cplusplus
}
#endif
