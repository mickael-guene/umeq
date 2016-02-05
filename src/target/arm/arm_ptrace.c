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

#include <sys/syscall.h>
#include <assert.h>
#include <errno.h>
#include <sys/ptrace.h>
#include <signal.h>
#include <asm/prctl.h>
#include <sys/prctl.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <sys/resource.h>

#include <stdio.h>
#include <string.h>

#include "runtime.h"
#include "arm_private.h"
#include "arm_syscall.h"
#include "arm_softfloat.h"
#include "umeq.h"
#include "syscall32_64_types.h"

#define MAGIC1  0x7a711e06171129a2UL
#define MAGIC2  0xc6925d1ed6dcd674UL

#ifndef PTRACE_GETVFPREGS
#define PTRACE_GETVFPREGS 27
#endif

#ifndef PTRACE_GETFDPIC
#define PTRACE_GETFDPIC 31 /* get the ELF fdpic loadmap address */
#define PTRACE_GETFDPIC_EXEC    0 /* [addr] request the executable loadmap */
#define PTRACE_GETFDPIC_INTERP  1 /* [addr] request the interpreter loadmap */
#endif

typedef union sigval_arm {
    uint32_t sival_int;
    uint32_t sival_ptr;
} sigval_t_arm;

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

struct user_regs_arm
{
    uint32_t uregs[18];
};

struct user_vfp_arm
{
    uint64_t fpregs[32];
    uint32_t fpscr;
};

typedef enum {
    ACTION_DELIVER = 0,
    ACTION_RESTART_SYSCALL,
} action_t;

/* this exec global will register host exec event and host exec syscall */
struct exec_ptrace_info {
    unsigned long exec_syscall_exit_info;
    unsigned long exec_event_info;
    unsigned long exec_event_in_signal_emulation;
} host_exec_info;

static long get_regs_base(int pid, unsigned long *regs_base) {
    struct user_regs_struct user_regs;
    long res;

    res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, 0, &user_regs);
#ifdef __i386__
    fatal("implement me\n");
#else
    if (!res)
        res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, user_regs.fs_base + 8, regs_base);
#endif

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
    unsigned long data;
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
    unsigned long data;
    long res;

    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, &host_exec_info.exec_event_in_signal_emulation, &data);
    if (res)
        return 0;

    return data;
}

static void clear_exec_event_in_signal_emulation(pid_t pid)
{
    /* we register both entry and exit but only exit info will be use in exec exit emulation */
    syscall(SYS_ptrace, PTRACE_POKEDATA, pid, &host_exec_info.exec_event_in_signal_emulation, 0);
    /* Note that here we don't assert on error since this function may be call on not ptraced process.
     * This is the case when a parent wait for STOP signal on a child.
     * This fix https://github.com/mickael-guene/umeq/issues/1 */
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
    res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs_base + offsetof(struct arm_registers, is_syscall_entry_show), &data);
    assert(res == 0);
    /* set value */
    res = syscall(SYS_ptrace, PTRACE_POKEDATA, pid, regs_base + offsetof(struct arm_registers, is_syscall_entry_show), is_entry);
    assert(res == 0);

    return data?1:0;
}

static int is_magic_syscall(pid_t pid, int *syscall_no, int *command, int *is_entry)
{
    long res;
    struct user_regs_struct regs;
    int is_magic;

#ifdef __i386__
    fatal("implement me\n");
#else
    res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, NULL, &regs);
    assert(res == 0);
    *syscall_no = regs.orig_rax;
    *command = regs.rdx;
    *is_entry = regs.rax==-ENOSYS?1:0;
    is_magic = regs.rdi== MAGIC1 && regs.rsi == MAGIC2;
#endif

    return is_magic;
}

static action_t handle_command_2(pid_t pid, int is_entry, int *status)
{
    long res;
    unsigned long exec_event_info;
    unsigned long exec_syscall_exit_info;

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

static int read_32(int pid, uint32_t *data, void *addr)
{
    int res;
    unsigned long data_long;

    res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, addr, &data_long);
    *data = data_long;

    return res;
}

static int write_32(int pid, uint32_t data, void *addr)
{
    int res;
    unsigned long data_long;

    res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, addr, &data_long);
    data_long = (data_long & 0xffffffff00000000UL) | data;
    res = syscall(SYS_ptrace, PTRACE_POKETEXT, pid, addr, data_long);

    return res;
}

/* FIXME: replace this dumb implementation */
static int read_bytes(int pid, void *dest, void *addr, int size)
{
    int res;

    assert(size);
    while(size >= 4) {
        res = read_32(pid, dest, addr);
        dest += 4;
        addr += 4;
        size -= 4;
    }
    if (size) {
        uint32_t data;

        res = read_32(pid, &data, addr);
        while(size--) {
            memcpy(dest, &data, 1);
            dest++;
            data = data >> 8;
        }
    }

    return res;
}

static uint32_t remote_fpscr_read(int pid)
{
    struct user_regs_struct user_regs;
    float_status fp_status;
    float_status fp_status_simd;
    uint32_t fpscr;
    unsigned long data_long;

    /* got base address */
    syscall(SYS_ptrace, PTRACE_GETREGS, pid, 0, &user_regs);
#ifdef __i386__
    fatal("implement me\n");
#else
    syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, user_regs.fs_base + 8, &data_long);
#endif

    /* copy tracee floating point info */
    read_bytes(pid, &fp_status, (void *)(data_long + offsetof(struct arm_registers, fp_status)), sizeof(fp_status));
    read_bytes(pid, &fp_status_simd, (void *)(data_long + offsetof(struct arm_registers, fp_status_simd)), sizeof(fp_status_simd));
    read_bytes(pid, &fpscr, (void *)(data_long + offsetof(struct arm_registers, fpscr)), sizeof(fpscr));

    return softfloat_to_arm_fpscr(fpscr, &fp_status, &fp_status_simd);
}

/* public api */
void ptrace_exec_event(struct arm_target *context)
{
    static int is_exec_event_need = 1;

    if (maybe_ptraced) {
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
}

void ptrace_syscall_enter(struct arm_target *context)
{
    if (maybe_ptraced) {
        context->regs.is_in_syscall = 1;
        if (is_under_proot)
            syscall((long) 313, 0);
        else
            syscall(SYS_gettid, MAGIC1, MAGIC2, 0);
    }
}

void ptrace_syscall_exit(struct arm_target *context)
{
    if (maybe_ptraced) {
        context->regs.is_in_syscall = 2;
        if (is_under_proot)
            syscall((long) 313, 1);
        else
            syscall(SYS_gettid, MAGIC1, MAGIC2, 1);
        context->regs.is_in_syscall = 0;
    }
}

/* syscall emulation */
int arm_ptrace(struct arm_target *context)
{
    int res;
    int request = context->regs.r[0];
    int pid = context->regs.r[1];
    unsigned int addr = context->regs.r[2];
    unsigned int data = context->regs.r[3];

    switch(request) {
        case PTRACE_TRACEME:
            maybe_ptraced = 1;
            /* fallthrough */
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
                unsigned long data_host;
                unsigned int *data_guest = (unsigned int *) g_2_h(data);

                res = syscall(SYS_ptrace, request, pid, addr, &data_host);
                *data_guest = data_host;
            }
            break;
        case PTRACE_PEEKTEXT:
        case PTRACE_PEEKDATA:
            {
                unsigned long data_host;
                unsigned int *data_guest = (unsigned int *) g_2_h(data);
                void *addr_host = g_2_h(addr);

                res = syscall(SYS_ptrace, request, pid, addr_host, &data_host);
                //fprintf(stderr, "PEEK: @0x%08x => res = %d\n", addr, res);
                *data_guest = data_host;
            }
            break;
        case PTRACE_POKETEXT:
        case PTRACE_POKEDATA:
            {
                unsigned long data_host;
                void *addr_host = g_2_h(addr);

                res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, addr_host, &data_host);
                data_host = (data_host & 0xffffffff00000000UL) | data;
                res = syscall(SYS_ptrace, request, pid, addr_host, data_host);
                //fprintf(stderr, "POKE: @0x%08x = 0x%08x => res = %d\n", addr, data, res);
            }
            break;
        case PTRACE_POKEUSER:
            {
                struct user_regs_struct user_regs;
                unsigned long data_long;
                unsigned long data_host;

                /* avoid poking too long ... */
                assert(addr < 16 * 4);
                /* FIXME: Need rework with a framework to handle tlsarea usage */
                res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, addr, &user_regs);
#ifdef __i386__
                fatal("implement me\n");
#else
                res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, user_regs.fs_base + 8, &data_long);
#endif

                res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, data_long + addr, &data_host);
                data_host = (data_host & 0xffffffff00000000UL) | data;
                res = syscall(SYS_ptrace, PTRACE_POKETEXT, pid, data_long + addr, data_host);
            }
            break;
        case PTRACE_GETSIGINFO:
            {
                siginfo_t siginfo;
                siginfo_t_arm *siginfo_arm = (siginfo_t_arm *) g_2_h(data);
                int signal = is_in_exec_event_in_signal_emulation(pid);
                
                res = syscall(SYS_ptrace, request, pid, addr, &siginfo);
                /* FIXME : need conversion numbers ? */
                siginfo_arm->si_signo = siginfo.si_signo;
                siginfo_arm->si_errno = siginfo.si_errno;
                siginfo_arm->si_code = siginfo.si_code;
                /* FIXME adapt according to signal ? */
                siginfo_arm->_sifields._sigchld._si_pid = siginfo.si_pid;
                siginfo_arm->_sifields._sigchld._si_uid = siginfo.si_uid;
                siginfo_arm->_sifields._sigchld._si_status = siginfo.si_status;
                siginfo_arm->_sifields._sigchld._si_utime = siginfo.si_utime;
                siginfo_arm->_sifields._sigchld._si_stime = siginfo.si_stime;
                if (is_under_proot == 0 && signal) {
                    siginfo_arm->si_signo = SIGTRAP;
                    siginfo_arm->si_code = signal==SIGTRAP?0:signal;
                }
            }
            break;
        case PTRACE_GETREGS:
            {
                struct user_regs_arm *user_regs_arm = (struct user_regs_arm *) g_2_h(data);
                struct user_regs_struct user_regs;
                unsigned long data_reg;
                unsigned long data_long;
                int i;
                uint32_t is_in_syscall;

                /* FIXME: Need rework with a framework to handle tlsarea usage */
                res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, addr, &user_regs);
#ifdef __i386__
                fatal("implement me\n");
#else
                res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, user_regs.fs_base + 8, &data_long);
#endif
                for(i=0;i<16;i++) {
                    res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, data_long + i * 4, &data_reg);
                    user_regs_arm->uregs[i] = (unsigned int) data_reg;
                }
                res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, data_long + 16 * 4, &data_reg);
                user_regs_arm->uregs[16] = (unsigned int) data_reg + ((user_regs_arm->uregs[15] & 1)?0x20:0);
                user_regs_arm->uregs[17] = user_regs_arm->uregs[0];
                res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, data_long + 20 * 4, &data_reg);
                is_in_syscall = (unsigned int) data_reg;
                /* if we are in kernel then r12 is use as a syscall enter/exit flag */
                if (is_in_syscall == 1) {
                    user_regs_arm->uregs[12] = 0;
                } else if (is_in_syscall == 2) {
                    user_regs_arm->uregs[12] = 1;
                }

                res = 0;
            }
            break;
        case PTRACE_SETREGS:
            {
                struct user_regs_arm *user_regs_arm = (struct user_regs_arm *) g_2_h(data);
                struct user_regs_struct user_regs;
                unsigned long data_long;
                int i;

                res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, addr, &user_regs);
#ifdef __i386__
                fatal("implement me\n");
#else
                res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, user_regs.fs_base + 8, &data_long);
#endif
                for(i=0;i<15;i++) {
                    res = write_32(pid, user_regs_arm->uregs[i], (void *) (data_long + i * 4));
                }
                res = write_32(pid, user_regs_arm->uregs[15] | (user_regs_arm->uregs[16]&0x20?1:0), (void *) (data_long + 15 * 4));
                res = write_32(pid, user_regs_arm->uregs[16] & 0xffffff00, (void *) (data_long + 16 * 4));

                res = 0;
            }
            break;
        case PTRACE_GETFPREGS:
            {
                /* FXIME: should be struct user_fp */
                /* in our case as we declare support for vfp. result of PTRACE_GETVFPREGS will be use */
                char *data_host = (char *) g_2_h(data);
                int i;

                for(i = 0; i < 116; i++) {
                    data_host[i] = 0;
                }

                res = 0;
            }
            break;
        case 22:/*PTRACE_GET_THREAD_AREA*/
            {
                unsigned int *data_guest = (unsigned int *) g_2_h(data);
                struct user_regs_struct user_regs;
                unsigned long data_tls;
                unsigned long data_long;

                res = syscall(SYS_ptrace, (long) PTRACE_GETREGS, (long) pid, (long) addr, (long) &user_regs);
#ifdef __i386__
                fatal("implement me\n");
#else
                res = syscall(SYS_ptrace, (long) PTRACE_PEEKTEXT, (long) pid, (long) user_regs.fs_base + 8, (long) &data_long);
#endif
                res = syscall(SYS_ptrace, (long) PTRACE_PEEKTEXT, (long) pid, (long) data_long + 17 * 4, (long) &data_tls);
                *data_guest = data_tls;

                res = 0;
            }
            break;
        case PTRACE_GETVFPREGS:
            {
                struct user_vfp_arm *user_vfp_regs_arm = (struct user_vfp_arm *) g_2_h(data);
                struct user_regs_struct user_regs;
                unsigned long data_reg;
                unsigned long data_long;
                int i;

                res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, addr, &user_regs);
#ifdef __i386__
                fatal("implement me\n");
#else
                res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, user_regs.fs_base + 8, &data_long);
#endif
                for(i=0;i<32;i++) {
                    res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, data_long + offsetof(struct arm_registers, e.d[i]), &data_reg);
                    user_vfp_regs_arm->fpregs[i] = data_reg;
                }
                user_vfp_regs_arm->fpscr = remote_fpscr_read(pid);

                res = 0;
            }
            break;
        case 29:/* PTRACE_GETHBPREGS */
            res = -EIO;
            break;
        case 0x4206: /* PTRACE_SEIZE */
            /* we don't support SEIZE */
            res = -EIO;
            break;
        case 0x4204: /* PTRACE_GETREGSET */
            /* can be supported but we clain to not support it */
            res = -EIO;
            break;
        default:
            fprintf(stderr, "ptrace unknown command : %d / 0x%x\n", request, request);
            res = -EIO;
            break;
    }

    return res;
}

int arm_process_vm_readv(struct arm_target *context)
{
    int res;
    pid_t pid = (pid_t) context->regs.r[0];
    struct iovec_32 *local_iov_guest = (struct iovec_32 *) g_2_h(context->regs.r[1]);
    unsigned long liovcnt = (unsigned long) context->regs.r[2];
    struct iovec_32 *remote_iov_guest = (struct iovec_32 *) g_2_h(context->regs.r[3]);
    unsigned long riovcnt = (unsigned long) context->regs.r[4];
    unsigned long flags = (unsigned long) context->regs.r[5];
    struct iovec *local_iov = (struct iovec *) alloca(liovcnt * sizeof(struct iovec));
    struct iovec *remote_iov = (struct iovec *) alloca(riovcnt * sizeof(struct iovec));
    int i;

    for(i = 0; i < liovcnt; i++) {
        local_iov[i].iov_base = (void *) g_2_h(local_iov_guest[i].iov_base);
        local_iov[i].iov_len = local_iov_guest[i].iov_len;
    }
    for(i = 0; i < riovcnt; i++) {
        remote_iov[i].iov_base = (void *) g_2_h(remote_iov_guest[i].iov_base);
        remote_iov[i].iov_len = remote_iov_guest[i].iov_len;
    }
    res = syscall(SYS_process_vm_readv, pid, local_iov, liovcnt, remote_iov, riovcnt, flags);

    return res;
}

int arm_wait4(struct arm_target *context)
{
    long res;
    pid_t pid = (pid_t) context->regs.r[0];
    int *status = (int *) (context->regs.r[1]?g_2_h(context->regs.r[1]):NULL);
    int options = (int) context->regs.r[2];
    uint32_t rusage_p = context->regs.r[3];
    struct rusage_32 *rusage_guest = (struct rusage_32 *) g_2_h(rusage_p);
    struct rusage rusage;
    //struct rusage *rusage = (struct rusage *) (context->regs.r[3]?g_2_h(context->regs.r[3]):NULL);
    action_t action;

    do {
        /* default behaviour is to deliver status */
        action = ACTION_DELIVER;
        res = syscall(SYS_wait4, pid, status, options, rusage_p?&rusage:0);
        if (rusage_p) {
            rusage_guest->ru_utime.tv_sec = rusage.ru_utime.tv_sec;
            rusage_guest->ru_utime.tv_usec = rusage.ru_utime.tv_usec;
            rusage_guest->ru_stime.tv_sec = rusage.ru_stime.tv_sec;
            rusage_guest->ru_stime.tv_usec = rusage.ru_stime.tv_usec;
            rusage_guest->ru_maxrss = rusage.ru_maxrss;
            rusage_guest->ru_ixrss = rusage.ru_ixrss;
            rusage_guest->ru_idrss = rusage.ru_idrss;
            rusage_guest->ru_isrss = rusage.ru_isrss;
            rusage_guest->ru_minflt = rusage.ru_minflt;
            rusage_guest->ru_majflt = rusage.ru_majflt;
            rusage_guest->ru_nswap = rusage.ru_nswap;
            rusage_guest->ru_inblock = rusage.ru_inblock;
            rusage_guest->ru_oublock = rusage.ru_oublock;
            rusage_guest->ru_msgsnd = rusage.ru_msgsnd;
            rusage_guest->ru_msgrcv = rusage.ru_msgrcv;
            rusage_guest->ru_nsignals = rusage.ru_nsignals;
            rusage_guest->ru_nvcsw = rusage.ru_nvcsw;
            rusage_guest->ru_nivcsw = rusage.ru_nivcsw;
        }
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
