#include <stdlib.h>

#include <stddef.h>
#include <stdint.h>
#include "target.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM64_TARGET_PRIVATE__
#define __ARM64_TARGET_PRIVATE__ 1

#define container_of(ptr, type, member) ({			\
    const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
    (type *)( (char *)__mptr - offsetof(type,member) );})

union simd_register {
    __uint128_t v128  __attribute__ ((aligned (32)));
    struct {
        uint64_t lsb;
        uint64_t msb;
    } v;
    uint64_t d[2];
    uint32_t s[4];
    uint16_t h[8];
    uint8_t b[16];
    double df[2];
    float sf[4];
};

struct arm64_registers {
    uint64_t r[32];
    uint64_t pc;
    union simd_register v[32];
    uint64_t tpidr_el0;
    uint32_t nzcv;
    uint32_t fpcr;
    uint32_t fpsr;
    uint32_t is_in_syscall;
    uint32_t is_stepin;
};

struct arm64_target {
    struct target target;
    struct arm64_registers regs;
    uint64_t pc;
    uint64_t sp_init;
    uint32_t isLooping;
    uint32_t exitStatus;
    uint32_t is_in_signal;
    __uint128_t exclusive_value;
    struct gdb gdb;
};

extern void arm64_load_image(int argc, char **argv, void **additionnal_env, void **unset_env, void *target_argv0, uint64_t *entry, uint64_t *stack);
extern void disassemble_arm64(struct target *target, struct irInstructionAllocator *ir, uint64_t pc, int maxInsn);
extern void arm64_hlp_syscall(uint64_t regs);
extern void arm64_setup_brk(void);

#endif

#ifdef __cplusplus
}
#endif
