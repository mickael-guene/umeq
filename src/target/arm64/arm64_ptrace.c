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
#include <string.h>

#include <stdio.h>

#include "arm64_private.h"
#include "arm64_syscall.h"
#include "arm64_syscall_types.h"
#include "runtime.h"
#include "arm64_helpers.h"
#include "umeq.h"
#include "arm64_softfloat.h"

#define MAGIC1  0x7a711e06171129a2UL
#define MAGIC2  0xc6925d1ed6dcd674UL

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

typedef enum {
    ACTION_DELIVER = 0,
    ACTION_RESTART_SYSCALL,
} action_t;

/* this exec global will register host exec event and host exec syscall */
struct exec_ptrace_info {
    uint64_t exec_syscall_exit_info;
    uint64_t exec_event_info;
    uint64_t exec_event_in_signal_emulation;
} host_exec_info;

static long get_regs_base(int pid, uint64_t *regs_base) {
    struct user_regs_struct user_regs;
    long res;

    res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, 0, &user_regs);
    if (!res)
        res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, user_regs.fs_base + 8, regs_base);

    return res;
}

static long read_fp_status(int pid, float_status *fp_status)
{
    long res;
    uint64_t regs_base;
    uint64_t data[2];

    res = get_regs_base(pid, &regs_base);
    if (res)
        goto read_fp_status_error;
    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm64_registers, fp_status), &data[0]);
    if (res)
        goto read_fp_status_error;
    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm64_registers, fp_status) + 8, &data[1]);
    if (res)
        goto read_fp_status_error;
    /* copy to float_status */
    memcpy(fp_status, data, sizeof(float_status));

    read_fp_status_error:
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
        if (!(i == 7 && is_in_syscall)) {
            res = syscall(SYS_ptrace, PTRACE_POKEDATA, pid, regs_base + offsetof(struct arm64_registers, r[i]), regs->regs[i]);
            if (res)
                goto write_gpr_error;
        }
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
    float_status fp_status;
    uint32_t qc;
    uint32_t fpcr_others;

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
    res = read_fp_status(pid, &fp_status);
    if (res)
        goto read_simd_error;
    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm64_registers, qc), &data);
    if (res)
        goto read_simd_error;
    qc = (uint32_t) data;
    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm64_registers, fpcr_others), &data);
    if (res)
        goto read_simd_error;
    fpcr_others = (uint32_t) data;
    regs->fpcr = softfloat_to_arm64_cpsr(&fp_status, fpcr_others);
    regs->fpsr = softfloat_to_arm64_fpsr(&fp_status, qc);

    read_simd_error:
    return res;
}

static int is_syscall_stop(pid_t pid)
{
    long res;
    siginfo_t siginfo;

    res = syscall(SYS_ptrace, PTRACE_GETSIGINFO, pid, 0, &siginfo);
    assert(res == 0);

    return (siginfo.si_code == SIGTRAP)?1:0;
}

static int is_execve_event(pid_t pid)
{
    uint64_t data;
    long res;

    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, &host_exec_info.exec_event_info, &data);
    assert(res == 0);

    return data?0:1;
}

static action_t register_execve_event(pid_t pid, int signal)
{
    long res;

    res = syscall(SYS_ptrace, PTRACE_POKEDATA, pid, &host_exec_info.exec_event_info, signal);
    assert(res == 0);

    return ACTION_RESTART_SYSCALL;
}

static void register_execve_syscall_exit(pid_t pid, int signal)
{
    long res;

    /* we register both entry and exit but only exit info will be use in exec exit emulation */
    res = syscall(SYS_ptrace, PTRACE_POKEDATA, pid, &host_exec_info.exec_syscall_exit_info, signal);
    assert(res == 0);
}

static int is_in_exec_event_in_signal_emulation(pid_t pid)
{
    uint64_t data;
    long res;

    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, &host_exec_info.exec_event_in_signal_emulation, &data);
    if (res)
        return 0;

    return data;
}

static void clear_exec_event_in_signal_emulation(pid_t pid)
{
    long res;

    /* we register both entry and exit but only exit info will be use in exec exit emulation */
    res = syscall(SYS_ptrace, PTRACE_POKEDATA, pid, &host_exec_info.exec_event_in_signal_emulation, 0);
    assert(res == 0);
}

static void set_exec_event_in_signal_emulation(pid_t pid, int signal)
{
    long res;

    /* we register both entry and exit but only exit info will be use in exec exit emulation */
    res = syscall(SYS_ptrace, PTRACE_POKEDATA, pid, &host_exec_info.exec_event_in_signal_emulation, signal);
    assert(res == 0);
}

/*  We mustn't generate wait event on syscall exit if we don't generate one in syscall entry */
/*  Typical case in which this occur if when the tracee send to himself a SIGSTOP so the
 * tracer can set options and restart te way it want.
 */
static int get_and_set_entry_show(pid_t pid, int command, int is_entry)
{
    uint64_t regs_base;
    long res;
    long data;

    /* return 0 if nothing to do */
    if (command == 2 ||
        (command == 0 && is_entry == 0) ||
        (command == 1 && is_entry == 1))
        return 0;

    /* read base first */
    res = get_regs_base(pid, &regs_base);
    assert(res == 0);
    /* read value */
    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm64_registers, is_syscall_entry_show), &data);
    assert(res == 0);
    /* set value */
    res = syscall(SYS_ptrace, PTRACE_POKEDATA, pid, regs_base + offsetof(struct arm64_registers, is_syscall_entry_show), is_entry);
    assert(res == 0);

    return data?1:0;
}

static int is_magic_syscall(pid_t pid, int *syscall_no, int *command, int *is_entry)
{
    long res;
    struct user_regs_struct regs;
    int is_magic;

    res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, NULL, &regs);
    assert(res == 0);
    *syscall_no = regs.orig_rax;
    *command = regs.rdx;
    *is_entry = regs.rax==-ENOSYS?1:0;
    is_magic = regs.rdi== MAGIC1 && regs.rsi == MAGIC2;

    return is_magic;
}

static action_t handle_command_2(pid_t pid, int is_entry, int *status)
{
    long res;
    long exec_event_info;
    long exec_syscall_exit_info;

    /* read exec info */
    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, &host_exec_info.exec_event_info, &exec_event_info);
    assert(res == 0);
    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, &host_exec_info.exec_syscall_exit_info, &exec_syscall_exit_info);
    assert(res == 0);

    /* now emulate */
    if (exec_event_info == SIGTRAP) {
        /* in this case sequence of event is the following :
         *  - exec syscall entry event
         *  - exec syscall exit event
         *  - exec event (SIGTRAP)
         * so when we got SIGTRAP we are sure that either syscall exit event occur
         * or not (in case tracer restart tracee with PTRACE_CONT).
         */
        if (is_entry)
            return exec_syscall_exit_info?ACTION_DELIVER:ACTION_RESTART_SYSCALL;
        else {
            set_exec_event_in_signal_emulation(pid, SIGTRAP);
            *status = (SIGTRAP << 8) | 0x7f;
            return ACTION_DELIVER;
        }
    } else if (exec_event_info == (SIGTRAP | PTRACE_EVENT_EXEC  << 8)) {
        /* in this case sequence of event is the following :
         *  - exec syscall entry event
         *  - exec event (SIGTRAP | PTRACE_EVENT_EXEC  << 8)
         *  - exec syscall exit event
         * in this case as we force PRACE_SYSCALL after host signal event we
         * are sure we reveive exec_syscall_exit_info. But do we need to
         * deliver it ? For that we let the tracer decide. If it restart
         * with PTRACE_CONT after signal event delivery then it doesn't want
         * to receive syscall exit event. If it restart with PTRACE_SYSCALL then
         * we will stop in command exit sequence and we will deliver exec
         * exit syscall.
         */
        if (is_entry) {
            set_exec_event_in_signal_emulation(pid, SIGTRAP | PTRACE_EVENT_EXEC  << 8);
            *status = ((SIGTRAP | PTRACE_EVENT_EXEC  << 8) << 8) | 0x7f;
            return ACTION_DELIVER;
        } else {
            /* So if we stop here it means tracer restart tracee with PTRACE_SYSCALL.
             * We are also sure we got exec_syscall_exit_info since we force PTRACE_SYSCALL
             * after we receive host SIGTRAP | PTRACE_EVENT_EXEC  << 8.
             */
            assert(exec_syscall_exit_info);
            return ACTION_DELIVER;
        }
    } else
        fatal("should not occur\n");
}

static action_t handle_syscall_stop(pid_t pid, int signal, int *status)
{
    action_t action;
    int syscall_no;
    int command;
    int is_entry;

    if (is_magic_syscall(pid, &syscall_no, &command, &is_entry)) {
        /* allow us to emulate syscall entry/exit and exec event */
        /* we will send an event on command == 0 && is_entry to emulate syscall entry
         * and when command == 1 && !is_entry to emulate syscall exit
         * command 2 is here to handle exec event and event syscall exit generation.
         */
        int is_entry_was_show = get_and_set_entry_show(pid, command, is_entry);
        switch(command) {
            case 0:
                /* we want to emulate syscall entry */
                action = is_entry?ACTION_DELIVER:ACTION_RESTART_SYSCALL;
                break;
            case 1:
                /* we want to emulate syscall exit */
                action = is_entry?ACTION_RESTART_SYSCALL:ACTION_DELIVER;
                /* but don't generate event if syscall entry was not deliver */
                action = is_entry_was_show?action:ACTION_RESTART_SYSCALL;
                break;
            case 2:
                /* we emulate exec event */
                action = handle_command_2(pid, is_entry, status);
                break;
            default:
                fatal("Unknown command %d\n", command);
        }
    } else if (syscall_no == SYS_execve) {
        register_execve_syscall_exit(pid, signal);
        action = ACTION_RESTART_SYSCALL;
    } else
        action = ACTION_RESTART_SYSCALL;//do nothing

    return action;
}

static int action_is_restart(action_t action)
{
    return (action==ACTION_RESTART_SYSCALL)?1:0;
}

static void restart_tracee(pid_t pid, action_t action)
{
    long res;

    if (action == ACTION_RESTART_SYSCALL) {
        res = syscall(SYS_ptrace, PTRACE_SYSCALL, pid, NULL, 0);
        assert(res == 0);
    } else
        assert(0);
}

/* public api */
void ptrace_exec_event(struct arm64_target *context)
{
    static int is_exec_event_need = 1;

    if (is_exec_event_need) {
        if (is_under_proot) {
            /* syscall execve exit sequence */
             /* this will be translated into sysexec exit */
            context->regs.is_in_syscall = 2;
            syscall((long) 313, 1);
             /* this will be translated into SIGTRAP */
            context->regs.is_in_syscall = 0;
            syscall((long) 313, 2);
        } else {
            //context->regs.r[8] = 221;
            syscall(SYS_gettid, MAGIC1, MAGIC2, 2);
            context->regs.is_in_syscall = 0;
        }
        is_exec_event_need = 0;
    }
}

void ptrace_syscall_enter(struct arm64_target *context)
{
    context->regs.is_in_syscall = 1;
    if (is_under_proot)
        syscall((long) 313, 0);
    else
        syscall(SYS_gettid, MAGIC1, MAGIC2, 0);
}

void ptrace_syscall_exit(struct arm64_target *context)
{
    context->regs.is_in_syscall = 2;
    if (is_under_proot)
        syscall((long) 313, 1);
    else
        syscall(SYS_gettid, MAGIC1, MAGIC2, 1);
    context->regs.is_in_syscall = 0;
}

/* syscall emulation */
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
                int signal = is_in_exec_event_in_signal_emulation(pid);
                
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
                if (is_under_proot == 0 && signal) {
                    siginfo_arm64->si_signo = SIGTRAP;
                    siginfo_arm64->si_code = signal==SIGTRAP?0:signal;
                }
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
        default:
            fprintf(stderr, "ptrace unknown command : %d / 0x%x / addr = %ld\n", request, request, addr);
            res = -EIO;
            break;
    }

    return res;
}

long arm64_wait4(struct arm64_target *context)
{
    long res;
    pid_t pid = (pid_t) context->regs.r[0];
    int *status = (int *) (context->regs.r[1]?g_2_h(context->regs.r[1]):NULL);
    int options = (int) context->regs.r[2];
    struct rusage *rusage = (struct rusage *) (context->regs.r[3]?g_2_h(context->regs.r[3]):NULL);
    action_t action;

    do {
        /* default behaviour is to deliver status */
        action = ACTION_DELIVER;
        res = syscall(SYS_wait4, pid, status, options, rusage);
        /* handle ptrace emulation on chroot environment */
        if (res > 0 && is_under_proot == 0 && status) {
            pid_t tracee_pid = res;

            /* for the moment catch every case. But in fact we are only
             * concern with WIFSTOPPED case.
             */
            if (WIFEXITED(*status)) {
                ;//do nothing
            } else if (WIFSIGNALED(*status)) {
                ;//do nothing
            } else if (WIFSTOPPED(*status)) {
                // ptracee event
                int signal = (*status & 0xfff00) >> 8;

                clear_exec_event_in_signal_emulation(tracee_pid);
                switch(signal) {
                    case SIGTRAP:
                        if (is_syscall_stop(tracee_pid)) {
                            action = handle_syscall_stop(tracee_pid, signal, status);
                        } else if (is_execve_event(tracee_pid)) {
                            action = register_execve_event(tracee_pid, signal);
                        } else
                            ;//do nothing since it's a real sigtrap
                        break;
                    case SIGTRAP | 0x80:
                        /* for sure it's a syscall stop */
                        action = handle_syscall_stop(tracee_pid, signal, status);
                        break;
                    case SIGTRAP | PTRACE_EVENT_EXEC  << 8:
                        /* for sure it's an execve event */
                        action = register_execve_event(tracee_pid, signal);
                        break;
                    default:
                        ;//do nothing for this signal
                }
            } else if (WIFCONTINUED(*status)) {
                ;//do nothing
            } else
                fatal("Unknown status\n");
        }
        /* exit loop */
        if (action_is_restart(action))
            restart_tracee(res, action);
        else
            break;
    } while(1);

    return res;
}
