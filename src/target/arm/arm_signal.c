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
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/syscall.h>
#include <assert.h>

#include "arm_syscall.h"
#include "runtime.h"

#define SA_RESTORER 0x04000000
#define MINSTKSZ    2048

extern int loop(uint64_t entry, uint64_t stack_entry, uint32_t signum, void *parent_target);

extern void wrap_signal_restorer(void);
/* This is the sigaction structure from the Linux 2.1.68 kernel.  */
struct kernel_sigaction {
    __sighandler_t k_sa_handler;
    unsigned long sa_flags;
    void (*sa_restorer) (void);
    sigset_t sa_mask;
};

struct sigaction_arm {
    uint32_t _sa_handler;
    uint32_t sa_flags;
    uint32_t sa_restorer;
    uint32_t sa_mask[0];
} __attribute__ ((packed));

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

typedef struct stack_arm {
    uint32_t ss_sp;
    uint32_t ss_flags;
    uint32_t ss_size;
} stack_t_arm;

struct arm_fdpic_handler {
    uint32_t handler;
    uint32_t entry;
    uint32_t got;
};

/* global array to hold guest signal handlers. not translated */
static struct arm_fdpic_handler guest_signals_handler[NSIG];
static uint32_t sa_flags[NSIG];
static stack_t_arm ss = {0, SS_DISABLE, 0};

/* allow to read got value for fdpic binaries */
uint32_t get_got_handler(int signum)
{
    return guest_signals_handler[signum].got;
}

/* signal wrapper functions */
void wrap_signal_handler(int signum)
{
    uint64_t stack_entry = (sa_flags[signum]&SA_ONSTACK)?(ss.ss_flags?0:ss.ss_sp + ss.ss_size):0;

    loop(guest_signals_handler[signum].entry, stack_entry, signum, NULL);
}

/* FIXME: BUG: Avoid usage of mmap_guest/munmap_guest since we can deadlock if we were already inside
   mmap_guest/munmap_guest when signal is generated. Solution is to allocate this structure from
   parent guest stack.
 */
void wrap_signal_sigaction(int signum, siginfo_t *siginfo, void *context)
{
    uint64_t stack_entry = (sa_flags[signum]&SA_ONSTACK)?(ss.ss_flags?0:ss.ss_sp + ss.ss_size):0;
    guest_ptr siginfo_guest = mmap_guest(0, sizeof(siginfo_t_arm), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1, 0);
    siginfo_t_arm *siginfo_arm = (siginfo_t_arm *) g_2_h(siginfo_guest);

    if (siginfo_arm) {
        siginfo_arm->si_signo = siginfo->si_signo;
        siginfo_arm->si_errno = siginfo->si_errno;
        siginfo_arm->si_code = siginfo->si_code;
        switch(siginfo->si_code) {
            case SI_USER:
            case SI_TKILL:
                siginfo_arm->_sifields._rt._si_pid = siginfo->si_pid;
                siginfo_arm->_sifields._rt._si_uid = siginfo->si_uid;
                break;
            case SI_KERNEL:
                if (signum == SIGPOLL) {
                    siginfo_arm->_sifields._sigpoll._si_band = siginfo->si_band;
                    siginfo_arm->_sifields._sigpoll._si_fd = siginfo->si_fd;
                } else if (signum == SIGCHLD) {
                    siginfo_arm->_sifields._sigchld._si_pid = siginfo->si_pid;
                    siginfo_arm->_sifields._sigchld._si_uid = siginfo->si_uid;
                    siginfo_arm->_sifields._sigchld._si_status = siginfo->si_status;
                    siginfo_arm->_sifields._sigchld._si_utime = siginfo->si_utime;
                    siginfo_arm->_sifields._sigchld._si_stime = siginfo->si_stime;
                } else {
                    siginfo_arm->_sifields._sigfault._si_addr = h_2_g(siginfo->si_addr);
                }
                break;
            case SI_QUEUE:
                siginfo_arm->_sifields._rt._si_pid = siginfo->si_pid;
                siginfo_arm->_sifields._rt._si_uid = siginfo->si_uid;
                siginfo_arm->_sifields._rt._si_sigval.sival_int = siginfo->si_int;
                siginfo_arm->_sifields._rt._si_sigval.sival_ptr = (uint64_t) siginfo->si_ptr;
                break;
            case SI_TIMER:
                siginfo_arm->_sifields._timer._si_tid = siginfo->si_timerid;
                siginfo_arm->_sifields._timer._si_overrun = siginfo->si_overrun;
                siginfo_arm->_sifields._timer._si_sigval.sival_int = siginfo->si_int;
                siginfo_arm->_sifields._timer._si_sigval.sival_ptr = (uint64_t) siginfo->si_ptr;
                break;
            case SI_MESGQ:
                siginfo_arm->_sifields._rt._si_sigval.sival_int = siginfo->si_int;
                siginfo_arm->_sifields._rt._si_sigval.sival_ptr = (uint64_t) siginfo->si_ptr;
                break;
            case SI_SIGIO:
            case SI_ASYNCIO:
                assert(0);
                break;
            default:
                fatal("si_code %d not yet implemented, signum = %d\n", siginfo->si_code, signum);
        }
        loop(guest_signals_handler[signum].entry, stack_entry, signum, (void *)(uint64_t) siginfo_guest);
        munmap_guest(siginfo_guest, sizeof(siginfo_t_arm));
    }
}

/* signal syscall handling */
int arm_rt_sigaction(struct arm_target *context)
{
    int res;
    uint32_t signum_p = context->regs.r[0];
    uint32_t act_p = context->regs.r[1];
    uint32_t oldact_p = context->regs.r[2];
    int signum = (int) signum_p;
    struct sigaction_arm *act_guest = (struct sigaction_arm *) g_2_h(act_p);
    struct sigaction_arm *oldact_guest = (struct sigaction_arm *) g_2_h(oldact_p);
    struct kernel_sigaction act;
    struct kernel_sigaction oldact;

    if (signum < 1 || signum >= NSIG)
        res = -EINVAL;
    else {
        memset(&act, 0, sizeof(act));
        memset(&oldact, 0, sizeof(oldact));
        //translate structure
        if (act_p) {
            if (act_guest->_sa_handler == (uint32_t)(uint64_t)SIG_ERR ||
                act_guest->_sa_handler == (uint32_t)(uint64_t)SIG_DFL ||
                act_guest->_sa_handler == (uint32_t)(uint64_t)SIG_IGN) {
                act.k_sa_handler = (__sighandler_t)(long) act_guest->_sa_handler;
                act.sa_mask.__val[0] = act_guest->sa_mask[0];
            } else {
                act.k_sa_handler = (act_guest->sa_flags & SA_SIGINFO)?(__sighandler_t)&wrap_signal_sigaction:&wrap_signal_handler;
                act.sa_mask.__val[0] = act_guest->sa_mask[0];
                act.sa_flags = act_guest->sa_flags | SA_RESTORER;
                act.sa_restorer = &wrap_signal_restorer;
            }
        }

        if (oldact_p) {
            oldact_guest->_sa_handler = guest_signals_handler[signum].handler;
        }

        //QUESTION : since setting guest handler and host handler is not atomic here is a problem ?
        //           if yes then this syscall must be redirecting towards proot (but is all threads stopped
        //            when we are ptracing a process ?)
        //setup guest syscall handler
        if (act_p) {
            sa_flags[signum] = act_guest->sa_flags;
            guest_signals_handler[signum].handler = act_guest->_sa_handler;
            if (context->fdpic_info.is_fdpic) {
                if (act_guest->_sa_handler != (uint32_t)(uint64_t)SIG_ERR &&
                    act_guest->_sa_handler != (uint32_t)(uint64_t)SIG_DFL &&
                    act_guest->_sa_handler != (uint32_t)(uint64_t)SIG_IGN) {
                    guest_signals_handler[signum].entry = *((uint32_t *) g_2_h(guest_signals_handler[signum].handler + 0));
                    guest_signals_handler[signum].got = *((uint32_t *) g_2_h(guest_signals_handler[signum].handler + 4));
                } else {
                    guest_signals_handler[signum].entry = -1;
                    guest_signals_handler[signum].got = -1;
                }
            } else {
                guest_signals_handler[signum].entry = act_guest->_sa_handler;
                guest_signals_handler[signum].got = 0;
            }
        }

        res = syscall(SYS_rt_sigaction, signum, act_p?&act:NULL, oldact_p?&oldact:NULL, _NSIG / 8);

        if (oldact_p) {
            //TODO : add guest restorer support
            oldact_guest->sa_flags = oldact.sa_flags & ~SA_RESTORER;
            oldact_guest->sa_mask[0] = oldact.sa_mask.__val[0];
            oldact_guest->sa_restorer = (long)NULL;
        }
    }

    return res;
}

int arm_sigaltstack(struct arm_target *context)
{
    uint32_t ss_p = context->regs.r[0];
    uint32_t oss_p = context->regs.r[1];
    stack_t_arm *ss_arm = (stack_t_arm *) g_2_h(ss_p);
    stack_t_arm *oss_arm = (stack_t_arm *) g_2_h(oss_p);
    int res = 0;

    if (oss_p) {
        oss_arm->ss_sp = ss.ss_sp;
        oss_arm->ss_flags = ss.ss_flags + (context->is_in_signal&2?SS_ONSTACK:0);
        oss_arm->ss_size = ss.ss_size;
    }
    if (ss_p) {
        if (ss_arm->ss_flags == 0) {
            if (ss_arm->ss_size < MINSTKSZ) {
                res = -ENOMEM;
            } else if (context->is_in_signal&2) {
                res = -EPERM;
            } else {
                ss.ss_sp = ss_arm->ss_sp;
                ss.ss_flags = ss_arm->ss_flags;
                ss.ss_size = ss_arm->ss_size;
            }
        } else if (ss_arm->ss_flags == SS_DISABLE) {
            ss.ss_flags = ss_arm->ss_flags;
        } else
            res = -EINVAL;
    }

    return res;
}
