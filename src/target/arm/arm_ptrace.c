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
#include <linux/unistd.h>
#include <asm/ldt.h>

#include <stdio.h>

#include "arm_private.h"
#include "arm_syscall.h"

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

struct iovec_32 {
    uint32_t iov_base;
    uint32_t iov_len;
};

static unsigned long get_base(int pid)
{
    struct user_regs_struct user_regs;
    struct user_desc desc;
    int res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, 0, &user_regs);

    //fprintf(stderr, "get_base for %d\n", pid);
    assert(res == 0);
    desc.entry_number = user_regs.xgs >> 3;
    //fprintf(stderr, "xgs = 0x%08x\n", user_regs.xgs);
    res = syscall(SYS_ptrace, 25/*PTRACE_GET_THREAD_AREA*/, pid, user_regs.xgs >> 3, &desc);
    assert(res == 0);
    //fprintf(stderr, "desc.base_addr = 0x%08x\n", desc.base_addr);

    return desc.base_addr;
}

static unsigned long get_runtime(int pid)
{
    unsigned long base = get_base(pid);
    unsigned long data_long;
    int res;

    res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, base + 4, &data_long);
    assert(res == 0);
    //fprintf(stderr, "runtime = 0x%08x\n", data_long);

    return data_long;
}

int read_32(int pid, uint32_t *data, void *addr)
{
    int res;
    unsigned long data_long;

    res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, addr, &data_long);
    *data = data_long;

    return res;
}

int write_32(int pid, uint32_t data, void *addr)
{
    int res;
    unsigned long data_long;

    res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, addr, &data_long);
    data_long = (data_long & 0xffffffff00000000UL) | data;
    res = syscall(SYS_ptrace, PTRACE_POKETEXT, pid, addr, data_long);

    return res;
}

int arm_ptrace(struct arm_target *context)
{
    int res;
    int request = context->regs.r[0];
    int pid = context->regs.r[1];
    unsigned int addr = context->regs.r[2];
    unsigned int data = context->regs.r[3];

    //fprintf(stderr, "%d: request %d\n", pid, request);
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
#if 1
                (void) user_regs;
                data_long = get_runtime(pid);
#else
                /* FIXME: Need rework with a framework to handle tlsarea usage */
                res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, addr, &user_regs);
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

#if 1
                (void) user_regs;
                data_long = get_runtime(pid);
#else
                /* FIXME: Need rework with a framework to handle tlsarea usage */
                res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, addr, &user_regs);
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

#if 1
                (void) user_regs;
                data_long = get_runtime(pid);
#else
                /* FIXME: Need rework with a framework to handle tlsarea usage */
                res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, addr, &user_regs);
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

#if 1
                (void) user_regs;
                data_long = get_runtime(pid);
#else
                /* FIXME: Need rework with a framework to handle tlsarea usage */
                res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, addr, &user_regs);
                res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, user_regs.fs_base + 8, &data_long);
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

#if 1
                (void) user_regs;
                data_long = get_runtime(pid);
#else
                /* FIXME: Need rework with a framework to handle tlsarea usage */
                res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, addr, &user_regs);
                res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, user_regs.fs_base + 8, &data_long);
#endif
                for(i=0;i<32;i++) {
                    res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, data_long + offsetof(struct arm_registers, e.d[i]), &data_reg);
                    user_vfp_regs_arm->fpregs[i] = data_reg;
                }
                res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, data_long + offsetof(struct arm_registers, fpscr), &data_reg);
                user_vfp_regs_arm->fpscr = (uint32_t) data_reg;

                res = 0;
            }
            break;
        case 29:/* PTRACE_GETHBPREGS */
            res = -EIO;
            break;
        case PTRACE_GETFDPIC:
            {
                struct arm_target *arm_target_tracee;
                struct user_regs_struct user_regs;
                unsigned long data_long;
                void *addr_tracee;

#if 1
                (void) user_regs;
                data_long = get_runtime(pid);
#else
                /* FIXME: Need rework with a framework to handle tlsarea usage */
                res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, addr, &user_regs);
                res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, user_regs.fs_base + 8, &data_long);
#endif
                arm_target_tracee = container_of((void *)data_long, struct arm_target, regs);
                if (addr == PTRACE_GETFDPIC_INTERP)
                    addr_tracee = &arm_target_tracee->fdpic_info.dl_addr;
                else
                    addr_tracee = &arm_target_tracee->fdpic_info.executable_addr;
                res = read_32(pid, g_2_h(data), addr_tracee);
            }
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

    //fprintf(stderr, "%d: request %d => res = %d\n", pid, request, res);
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
