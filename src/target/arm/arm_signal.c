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
#include "arm_signal_types.h"

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

/* global array to hold guest signal handlers. not translated */
static struct umeq_signal_handler_arm handlers[NSIG];
static stack_t_arm ss = {0, SS_DISABLE, 0};

/* signal wrapper functions */
void wrap_signal_handler(int signum)
{
    uint32_t sa_flags = handlers[signum].guest.sa_flags;
    uint64_t stack_entry = (sa_flags&SA_ONSTACK)?(ss.ss_flags?0:ss.ss_sp + ss.ss_size):0;
    struct umeq_arm_signal_param param;

    assert(0);
    param.siginfo = NULL;
    param.handler = handlers[signum];
    if (handlers[signum].is_fdpic) {
        struct fdpic_funcdesc *funcdesc = (struct fdpic_funcdesc *) g_2_h(handlers[signum].guest._sa_handler);

        loop(funcdesc->fct, stack_entry, signum, &param);
    } else
        loop(handlers[signum].guest._sa_handler, stack_entry, signum, &param);
}

void wrap_signal_sigaction(int signum, siginfo_t *siginfo, void *context)
{
    uint32_t sa_flags = handlers[signum].guest.sa_flags;
    uint64_t stack_entry = (sa_flags&SA_ONSTACK)?(ss.ss_flags?0:ss.ss_sp + ss.ss_size):0;
    struct umeq_arm_signal_param param;

    assert(0);
    param.siginfo = siginfo;
    param.handler = handlers[signum];
    if (handlers[signum].is_fdpic) {
        struct fdpic_funcdesc *funcdesc = (struct fdpic_funcdesc *) g_2_h(handlers[signum].guest._sa_handler);

        loop(funcdesc->fct, stack_entry, signum, &param);
    } else
        loop(handlers[signum].guest._sa_handler, stack_entry, signum, &param);
}

/* signal syscall handling */
int arm_rt_sigaction(struct arm_target *context)
{
    int res;
    uint32_t signum_p = context->regs.r[0];
    uint32_t act_p = context->regs.r[1];
    uint32_t oldact_p = context->regs.r[2];
    uint32_t sigsetsize_p = context->regs.r[3];
    int signum = (int) signum_p;
    struct sigaction_arm *act_guest = (struct sigaction_arm *) g_2_h(act_p);
    struct sigaction_arm *oldact_guest = (struct sigaction_arm *) g_2_h(oldact_p);
    size_t sigsetsize = (size_t) sigsetsize_p;
    struct kernel_sigaction act;
    struct kernel_sigaction oldact;

    if (signum < 1 || signum >= NSIG)
        res = -EINVAL;
    else if (act_p == 0xffffffff || oldact_p == 0xffffffff)
        res = -EFAULT;
    else {
        memset(&act, 0, sizeof(act));
        memset(&oldact, 0, sizeof(oldact));
        //translate structure
        if (oldact_p) {
            *oldact_guest = handlers[signum].guest;
        }
        if (act_p) {
            handlers[signum].guest = *act_guest;
            handlers[signum].is_fdpic = context->fdpic_info.is_fdpic;
            /* now handle host handler to setup */
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

        /* now register handler */
        res = syscall(SYS_rt_sigaction, signum, act_p?&act:NULL, oldact_p?&oldact:NULL, sigsetsize);
        if (oldact_p) {
            oldact_guest->sa_mask[0] = oldact.sa_mask.__val[0];
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

int arm_rt_sigqueueinfo(struct arm_target *context)
{
    int res;
    uint32_t tgid_p = context->regs.r[0];
    uint32_t sig_p = context->regs.r[1];
    uint32_t uinfo_p = context->regs.r[2];
    pid_t tgid = (pid_t) tgid_p;
    int sig = (int) sig_p;
    siginfo_t_arm *uinfo_arm = (siginfo_t_arm *) g_2_h(uinfo_p);
    siginfo_t uinfo;

    uinfo.si_code = uinfo_arm->si_code;
    uinfo.si_pid = uinfo_arm->_sifields._rt._si_pid;
    uinfo.si_uid = uinfo_arm->_sifields._rt._si_uid;
    uinfo.si_value.sival_int = uinfo_arm->_sifields._rt._si_sigval.sival_int;

    res = syscall(SYS_rt_sigqueueinfo, tgid, sig, &uinfo);

    return res;
}
