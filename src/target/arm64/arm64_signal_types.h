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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM64_SIGNAL_TYPES__
#define __ARM64_SIGNAL_TYPES__ 1

#include <stdint.h>

struct sigaction_arm64 {
    uint64_t _sa_handler;
    uint64_t sa_flags;
    uint64_t sa_restorer;
    uint64_t sa_mask[0];
};

typedef struct stack_arm64 {
    uint64_t ss_sp;
    uint32_t ss_flags;
    uint64_t ss_size;
} stack_t_arm64;

struct _aarch64_ctx {
    uint32_t magic;
    uint32_t size;
};

typedef union sigval_arm64 {
    uint32_t sival_int;
    uint64_t sival_ptr;
} sigval_t_arm64;

struct siginfo_arm64 {
    uint32_t si_signo;
    uint32_t si_errno;
    uint32_t si_code;
    union {
        uint32_t _pad[28];
        /* kill */
        struct {
            uint32_t _si_pid;
            uint32_t _si_uid;
        } _kill;
        /* POSIX.1b timers */
        struct {
            uint32_t _si_tid;
            uint32_t _si_overrun;
            sigval_t_arm64 _si_sigval;
        } _timer;
        /* POSIX.1b signals */
        struct {
            uint32_t _si_pid;
            uint32_t _si_uid;
            sigval_t_arm64 _si_sigval;
        } _rt;
        /* SIGCHLD */
        struct {
            uint32_t _si_pid;
            uint32_t _si_uid;
            uint32_t _si_status;
            uint32_t _si_utime;
            uint32_t _si_stime;
        } _sigchld;
        /* SIGILL, SIGFPE, SIGSEGV, SIGBUS */
        struct {
            uint64_t _si_addr;
        } _sigfault;
        /* SIGPOLL */
        struct {
            uint64_t _si_band;
            uint32_t _si_fd;
        } _sigpoll;
    } _sifields;
};

typedef struct {
    uint64_t sig[1/*_NSIG_WORDS*/];
} sigset_t_arm64;

struct sigcontext_arm64 {
    uint64_t fault_address;
    /* AArch64 registers */
    uint64_t regs[31];
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
    /* 4K reserved for FP/SIMD state and future expansion */
    uint8_t __reserved[4096] __attribute__((__aligned__(16)));
};

struct ucontext_arm64 {
    uint64_t                uc_flags;
    struct ucontext_arm64   *uc_link;
    stack_t_arm64           uc_stack;
    sigset_t_arm64          uc_sigmask;
    /* glibc uses a 1024-bit sigset_t */
    uint8_t                 __unused[1024 / 8 - sizeof(sigset_t_arm64)];
    /* last for future expansion */
    struct sigcontext_arm64 uc_mcontext;
};

struct rt_sigframe_arm64 {
    struct siginfo_arm64 info;
    struct ucontext_arm64 uc;
    uint64_t fp;
    uint64_t lr;
};

struct host_signal_info {
    siginfo_t *siginfo;
    void *context;
    int is_sigaction_handler;
};

extern uint64_t sigsp(struct arm64_target *prev_context, uint32_t signum);

#endif

#ifdef __cplusplus
}
#endif