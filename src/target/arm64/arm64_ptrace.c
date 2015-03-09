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

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/ptrace.h>
#include <signal.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <elf.h>
#include <errno.h>

#include <stdio.h>

#include "arm64_private.h"
#include "arm64_syscall.h"
#include "arm64_syscall_types.h"
#include "runtime.h"
#include "arm64_helpers.h"

/* FIXME: move this in global config */
#ifndef PTRACE_GETREGSET
#define PTRACE_GETREGSET 0x4204
#endif
#ifndef NT_ARM_TLS
#define NT_ARM_TLS 0x401
#endif
#ifndef NT_ARM_HW_BREAK
#define NT_ARM_HW_BREAK 0x402
#endif
#ifndef NT_ARM_HW_WATCH
#define NT_ARM_HW_WATCH 0x403
#endif
#ifndef PTRACE_SEIZE
#define PTRACE_SEIZE 0x4206
#endif

#ifndef NT_ARM_SYSTEM_CALL
#define NT_ARM_SYSTEM_CALL  0x404
#endif

#define INSN(msb, lsb) ((insn >> (lsb)) & ((1 << ((msb) - (lsb) + 1))-1))

#define NT_PRFPREG  2

typedef union sigval_arm64 {
    uint32_t sival_int;
    uint64_t sival_ptr;
} sigval_t_arm64;

typedef struct {
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
} siginfo_t_arm64;

struct user_pt_regs_arm64 {
    uint64_t    regs[31];
    uint64_t    sp;
    uint64_t    pc;
    uint64_t    pstate;
};

struct user_fpsimd_state_arm64 {
    union       simd_register vregs[32];
    uint32_t    fpsr;
    uint32_t    fpcr;
};

static long get_regs_base(int pid, uint64_t *regs_base) {
    struct user_regs_struct user_regs;
    long res;

    res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, 0, &user_regs);
    if (!res)
        res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, user_regs.fs_base + 8, regs_base);

    return res;
}

/* FIXME: check returns value */
static long read_gpr(int pid, struct user_pt_regs_arm64 *regs)
{
    uint32_t is_in_syscall;
    uint64_t regs_base;
    uint64_t data;
    long res;
    int i;

    res = get_regs_base(pid, &regs_base);
    if (res)
        goto read_gpr_error;

    for(i=0;i<31;i++) {
        res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm64_registers, r[i]), &regs->regs[i]);
        if (res)
            goto read_gpr_error;
    }
    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm64_registers, r[31]), &regs->sp);
    if (res)
        goto read_gpr_error;
    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm64_registers, pc), &regs->pc);
    if (res)
        goto read_gpr_error;
    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm64_registers, nzcv), &data);
    regs->pstate = (uint32_t) data;
    if (res)
        goto read_gpr_error;
    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm64_registers, is_in_syscall), &data);
    is_in_syscall = (uint32_t) data;
    if (res)
        goto read_gpr_error;
    /* if we are in 'kerne'l then x7 is use as a syscall enter/exit flag */
    if (is_in_syscall == 1)
        regs->regs[7] = 0;
    else if (is_in_syscall == 2)
        regs->regs[7] = 1;

    read_gpr_error:
    return res;
}

static long write_gpr(int pid, struct user_pt_regs_arm64 *regs)
{
    uint32_t is_in_syscall;
    uint64_t regs_base;
    uint64_t data;
    long res;
    int i;

    res = get_regs_base(pid, &regs_base);
    if (res)
        goto write_gpr_error;
    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm64_registers, is_in_syscall), &data);
    is_in_syscall = (uint32_t) data;
    if (res)
        goto write_gpr_error;

    for(i=0;i<31;i++) {
        if (!(i == 7 && is_in_syscall))
            res = syscall(SYS_ptrace, PTRACE_POKEDATA, pid, regs_base + offsetof(struct arm64_registers, r[i]), regs->regs[i]);
            if (res)
                goto write_gpr_error;
    }
    res = syscall(SYS_ptrace, PTRACE_POKEDATA, pid, regs_base + offsetof(struct arm64_registers, r[31]), regs->sp);
    if (res)
        goto write_gpr_error;
    res = syscall(SYS_ptrace, PTRACE_POKEDATA, pid, regs_base + offsetof(struct arm64_registers, pc), regs->pc);
    if (res)
        goto write_gpr_error;
    /* update NZCV. For that we need to do read/modify/write sequence */
    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm64_registers, nzcv), &data);
    if (res)
        goto write_gpr_error;
    data = (data & 0xffffffff00000000UL) | regs->pstate;
    res = syscall(SYS_ptrace, PTRACE_POKEDATA, pid, regs_base + offsetof(struct arm64_registers, nzcv), data);
    if (res)
        goto write_gpr_error;

    write_gpr_error:
    return res;
}

static long read_simd(int pid, struct user_fpsimd_state_arm64 *regs)
{
    uint64_t regs_base;
    uint64_t data;
    long res;
    int i;

    res = get_regs_base(pid, &regs_base);
    if (res)
        goto read_simd_error;

    for(i=0;i<32;i++) {
        res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm64_registers, v[i].v.lsb), &regs->vregs[i].v.lsb);
        if (res)
            goto read_simd_error;
        res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm64_registers, v[i].v.msb), &regs->vregs[i].v.msb);
        if (res)
            goto read_simd_error;
    }
    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm64_registers, fpcr), &data);
    regs->fpsr = (uint32_t) data;
    if (res)
        goto read_simd_error;
    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm64_registers, fpsr), &data);
    regs->fpsr = (uint32_t) data;
    if (res)
        goto read_simd_error;

    read_simd_error:
    return res;
}

long arm64_ptrace(struct arm64_target *context)
{
    long res;
    int request = context->regs.r[0];
    int pid = context->regs.r[1];
    uint64_t addr = context->regs.r[2];
    uint64_t data = context->regs.r[3];

    switch(request) {
        case PTRACE_TRACEME:
        case PTRACE_CONT:
        case PTRACE_SETOPTIONS:
        case PTRACE_KILL:
        case PTRACE_SYSCALL:
        case PTRACE_DETACH:
            /* not translated syscalls */
            res = syscall(SYS_ptrace, request, pid, addr, data);
            break;
        case PTRACE_GETEVENTMSG:
            {
                unsigned long *data_guest = (unsigned long *) g_2_h(data);

                res = syscall(SYS_ptrace, request, pid, addr, data_guest);
            }
            break;
        case PTRACE_PEEKTEXT:
        case PTRACE_PEEKDATA:
            {
                unsigned long *data_guest = (unsigned long *) g_2_h(data);
                void *addr_host = g_2_h(addr);

                res = syscall(SYS_ptrace, request, pid, addr_host, data_guest);
                //fprintf(stderr, "PEEK: @0x%08lx => res = %ld => data = 0x%016lx\n", addr, res, *data_guest);
            }
            break;
        case PTRACE_POKETEXT:
        case PTRACE_POKEDATA:
            {
                void *addr_host = g_2_h(addr);

                res = syscall(SYS_ptrace, request, pid, addr_host, data);
                //fprintf(stderr, "POKE: @0x%08lx = 0x%08lx => res = %ld\n", addr, data, res);
            }
            break;
        case PTRACE_GETSIGINFO:
            {
                siginfo_t siginfo;
                siginfo_t_arm64 *siginfo_arm64 = (siginfo_t_arm64 *) g_2_h(data);
                
                res = syscall(SYS_ptrace, request, pid, addr, &siginfo);
                /* FIXME : need conversion numbers ? */
                siginfo_arm64->si_signo = siginfo.si_signo;
                siginfo_arm64->si_errno = siginfo.si_errno;
                siginfo_arm64->si_code = siginfo.si_code;
                /* FIXME adapt according to signal ? */
                siginfo_arm64->_sifields._sigchld._si_pid = siginfo.si_pid;
                siginfo_arm64->_sifields._sigchld._si_uid = siginfo.si_uid;
                siginfo_arm64->_sifields._sigchld._si_status = siginfo.si_status;
                siginfo_arm64->_sifields._sigchld._si_utime = siginfo.si_utime;
                siginfo_arm64->_sifields._sigchld._si_stime = siginfo.si_stime;
            }
            break;
        case PTRACE_GETREGSET:
            if (addr == NT_PRSTATUS) {
                struct iovec *io = (struct iovec *) g_2_h(data);

                assert(io->iov_len >= sizeof(struct user_pt_regs_arm64));
                res = read_gpr(pid, io->iov_base);
                io->iov_len = sizeof(struct user_pt_regs_arm64);
            } else if (addr == NT_PRFPREG) {
                struct iovec *io = (struct iovec *) g_2_h(data);

                assert(io->iov_len >= sizeof(struct user_fpsimd_state_arm64));
                res = read_simd(pid, io->iov_base);
                io->iov_len = sizeof(struct user_fpsimd_state_arm64);
            } else if (addr == NT_ARM_TLS) {
                struct iovec *io = (struct iovec *) g_2_h(data);
                uint64_t regs_base;

                res = get_regs_base(pid, &regs_base);
                if (res)
                    goto ptrace_getregset_error;
                assert(io->iov_len >= sizeof(uint64_t));
                res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm64_registers, tpidr_el0), io->iov_base);
                if (res)
                    goto ptrace_getregset_error;
                io->iov_len = sizeof(uint64_t);
            } else if (addr == NT_ARM_HW_BREAK || addr == NT_ARM_HW_WATCH) {
                res = -EINVAL;
            } else
                fatal("PTRACE_GETREGSET: addr = %d\n", addr);
            ptrace_getregset_error:
            break;
        case PTRACE_SETREGSET:
            if (addr == NT_PRSTATUS) {
                struct iovec *io = (struct iovec *) g_2_h(data);

                assert(io->iov_len == sizeof(struct user_pt_regs_arm64));
                res = write_gpr(pid, io->iov_base);
            } else if (addr == NT_ARM_SYSTEM_CALL) {
                /* here we do nothing since we re-read syscall number after x8 change */
                /* see comment in arm64_syscall.c */
                res = 0;
            } else {
                fatal("PTRACE_SETREGSET: addr = %d\n", addr);
            }
            break;
        case PTRACE_SINGLESTEP:
            {
                uint64_t regs_base;
                uint64_t data;

                res = get_regs_base(pid, &regs_base);
                if (res)
                    goto ptrace_singlestep_error;
                res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm64_registers, is_stepin), &data);
                if (res)
                    goto ptrace_singlestep_error;
                data = (data & 0xffffffff00000000UL) | 1;
                res = syscall(SYS_ptrace, PTRACE_POKEDATA, pid, regs_base + offsetof(struct arm64_registers, is_stepin), data);
                if (res)
                    goto ptrace_singlestep_error;
                res = syscall(SYS_ptrace, PTRACE_CONT, pid, 0, 0);
            }
            ptrace_singlestep_error:
            break;
        case PTRACE_SEIZE:
            res = syscall(SYS_ptrace, PTRACE_SEIZE, pid, addr, data);
            break;
        default:
            fprintf(stderr, "ptrace unknown command : %d / 0x%x / addr = %ld\n", request, request, addr);
            res = -EIO;
            break;
    }

    return res;
}
