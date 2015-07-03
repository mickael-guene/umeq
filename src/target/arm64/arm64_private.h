/* This file is part of Umeq, an equivalent of qemu user mode emulation with improved robustness.
 *
 * Copyright (C) 2015 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#include <stdlib.h>

#include <stddef.h>
#include <stdint.h>
#include "target.h"
#include "target64.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM64_TARGET_PRIVATE__
#define __ARM64_TARGET_PRIVATE__ 1

#define container_of(ptr, type, member) ({			\
    const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
    (type *)( (char *)__mptr - offsetof(type,member) );})

union simd_register {
    __uint128_t v128  __attribute__ ((aligned (16)));
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
    uint64_t is_syscall_entry_show;
    uint64_t helper_pc;
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
    struct backend *backend;
    /* stuff need to support guest context change during signal handler */
    struct rt_sigframe_arm64 *frame;
    void *param;
    struct arm64_target *prev_context;
};

/* globals */
extern guest_ptr *arm64_env_startup_pointer;
/* functions */
extern void arm64_load_image(int argc, char **argv, void **additionnal_env, void **unset_env, void *target_argv0, uint64_t *entry, uint64_t *stack);
extern void disassemble_arm64(struct target *target, struct irInstructionAllocator *ir, uint64_t pc, int maxInsn);
extern void disassemble_arm64_with_marker(struct arm64_target *context, struct irInstructionAllocator *ir, uint64_t pc, int maxInsn);
extern void arm64_hlp_syscall(uint64_t regs);
extern void arm64_setup_brk(void);
extern void ptrace_exec_event(struct arm64_target *context);
extern void ptrace_syscall_enter(struct arm64_target *context);
extern void ptrace_syscall_exit(struct arm64_target *context);

#endif

#ifdef __cplusplus
}
#endif
