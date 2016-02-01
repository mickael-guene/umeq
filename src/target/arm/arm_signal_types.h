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

#ifndef __ARM_SIGNAL_TYPES__
#define __ARM_SIGNAL_TYPES__ 1

#include <stdint.h>

typedef union sigval_arm {
    uint32_t sival_int;
    uint32_t sival_ptr;
} sigval_t_arm;

typedef struct stack_arm {
    uint32_t ss_sp;
    uint32_t ss_flags;
    uint32_t ss_size;
} stack_t_arm;

struct sigcontext_arm {
    uint32_t trap_no;
    uint32_t error_code;
    uint32_t oldmask;
    uint32_t arm_r0;
    uint32_t arm_r1;
    uint32_t arm_r2;
    uint32_t arm_r3;
    uint32_t arm_r4;
    uint32_t arm_r5;
    uint32_t arm_r6;
    uint32_t arm_r7;
    uint32_t arm_r8;
    uint32_t arm_r9;
    uint32_t arm_r10;
    uint32_t arm_fp;
    uint32_t arm_ip;
    uint32_t arm_sp;
    uint32_t arm_lr;
    uint32_t arm_pc;
    uint32_t arm_cpsr;
    uint32_t fault_address;
};

typedef struct {
    uint32_t sig[2/*_NSIG_WORDS*/];
} sigset_t_arm;

struct ucontext_arm {
    uint32_t                uc_flags;
    guest_ptr               uc_link;
    stack_t_arm             uc_stack;
    struct sigcontext_arm   uc_mcontext;
    sigset_t_arm            uc_sigmask;
    /* glibc uses a 1024-bit sigset_t */
    int                     __unused[32 - (sizeof (sigset_t_arm) / sizeof (int))];
    uint32_t                uc_regspace[128];
};

typedef struct siginfo_arm {
    uint32_t si_signo;
    uint32_t si_errno;
    uint32_t si_code;
    union {
        uint32_t _pad[29];
        /* kill */
        struct {
            uint32_t _si_pid;
            uint32_t _si_uid;
        } _kill;
        /* POSIX.1b timers */
        struct {
            uint32_t _si_tid;
            uint32_t _si_overrun;
            sigval_t_arm _si_sigval;
        } _timer;
        /* POSIX.1b signals */
        struct {
            uint32_t _si_pid;
            uint32_t _si_uid;
            sigval_t_arm _si_sigval;
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
            uint32_t _si_addr;
        } _sigfault;
        /* SIGPOLL */
        struct {
            uint32_t _si_band;
            uint32_t _si_fd;
        } _sigpoll;
    } _sifields;
} siginfo_t_arm;

struct sigframe_arm {
    struct ucontext_arm uc;
    uint32_t retcode[2];
};

struct rt_sigframe_arm {
    struct siginfo_arm info;
    struct sigframe_arm sig;
};

struct sigaction_arm {
    uint32_t _sa_handler;
    uint32_t sa_flags;
    uint32_t sa_restorer;
    uint32_t sa_mask[0];
} __attribute__ ((packed));

struct host_signal_info {
    siginfo_t *siginfo;
    void *context;
    int is_sigaction_handler;
};

#endif

#ifdef __cplusplus
}
#endif
