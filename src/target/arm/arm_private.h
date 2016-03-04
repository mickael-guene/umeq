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
#include <ucontext.h>

#include "loader32.h"
#include "target.h"

#include "softfloat.h"
#include "umeq.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM_TARGET_PRIVATE__
#define __ARM_TARGET_PRIVATE__ 1

#define container_of(ptr, type, member) ({			\
    const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
    (type *)( (char *)__mptr - offsetof(type,member) );})

union simd_d_register {
    uint8_t u8[8];
    uint16_t u16[4];
    uint32_t u32[2];
    uint64_t u64[1];
    int8_t s8[8];
    int16_t s16[4];
    int32_t s32[2];
    int64_t s64[1];
    float sf[2];
};

union simd_q_register {
    uint8_t u8[16];
    uint16_t u16[8];
    uint32_t u32[4];
    uint64_t u64[2];
    int8_t s8[16];
    int16_t s16[8];
    int32_t s32[4];
    int64_t s64[2];
    float sf[4];
};

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
    unsigned long is_syscall_entry_show;
    uint32_t fpscr;
    union {
        uint64_t d[32];
        uint32_t s[64];
        double df[32];
        float sf[64];
        union simd_d_register simd[32];
        union simd_q_register simq[16];
    } e;
    float_status fp_status;
    float_status fp_status_simd;
    uint32_t helper_pc;
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
    int32_t in_signal_location;
    struct backend *backend;
    /* stuff need to support guest context change during signal handler */
    struct sigframe_arm *frame;
    void *param;
    struct arm_target *prev_context;
    uint32_t sas_ss_sp;
    uint32_t sas_ss_size;
    int start_on_sig_stack;
};

/* globals */
extern guest_ptr *arm_env_startup_pointer;
/* functions */
extern void disassemble_arm(struct target *target, struct irInstructionAllocator *irAlloc, uint64_t pc, int maxInsn);
extern void disassemble_thumb(struct target *target, struct irInstructionAllocator *irAlloc, uint64_t pc, int maxInsn);
extern void arm_setup_brk(void);
extern void ptrace_exec_event(struct arm_target *context);
extern void ptrace_syscall_enter(struct arm_target *context);
extern void ptrace_syscall_exit(struct arm_target *context);
extern void arm_load_image(int argc, char **argv, void **additionnal_env, void **unset_env, void *target_argv0, uint64_t *entry, uint64_t *stack);
extern void disassemble_arm_with_marker(struct arm_target *context, struct irInstructionAllocator *irAlloc, uint64_t pc, int maxInsn);
extern void disassemble_thumb_with_marker(struct arm_target *context, struct irInstructionAllocator *irAlloc, uint64_t pc, int maxInsn);
extern int on_sig_stack(struct arm_target *context, uint32_t sp);
extern int is_out_of_signal_stack(struct arm_target *context);
extern uint32_t sigsp(struct arm_target *prev_context, uint32_t signum);
extern int clone_fork_host(struct arm_target *context);
extern int clone_thread_host(struct arm_target *context, void *stack, struct tls_context *new_thread_tls_context);
extern void ptrace_event_proot(int event);
extern void ptrace_event_chroot(int event);
extern long get_regs_base(int pid, unsigned long *regs_base);
extern int is_magic_syscall(pid_t pid, int *syscall_no, int *command, int *is_entry);
extern void *get_host_pc_from_user_context(ucontext_t *ucp);

#endif

#ifdef __cplusplus
}
#endif
