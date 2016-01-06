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

#include "arm64_syscall.h"
#include "arm64_signal_types.h"
#include "runtime.h"

#define SA_RESTORER 0x04000000
#define MINSTKSZ    2048

struct kernel_sigaction {
    __sighandler_t k_sa_handler;
    unsigned long sa_flags;
    void (*sa_restorer) (void);
    sigset_t sa_mask;
};

/* FIXME: this need to be in an include file */
extern int loop(uint64_t entry, uint64_t stack_entry, uint32_t signum, void *parent_target);
extern void wrap_signal_restorer(void);

/* global array to hold guest signal handlers. not translated */
static uint64_t guest_signals_handler[NSIG];
static uint64_t sa_flags[NSIG];

/* signal wrapper functions */
void wrap_signal_handler(int signum, siginfo_t *siginfo, void *context)
{
    struct host_signal_info signal_info = {siginfo, context, 0};

    loop(guest_signals_handler[signum], 0, signum, (void *)&signal_info);
}

void wrap_signal_sigaction(int signum, siginfo_t *siginfo, void *context)
{
    struct host_signal_info signal_info = {siginfo, context, 1};

    loop(guest_signals_handler[signum], 0, signum, (void *)&signal_info);
}

/* signal syscall handling */
long arm64_rt_sigaction(struct arm64_target *context)
{
    long res;
    uint64_t signum_p = context->regs.r[0];
    uint64_t act_p = context->regs.r[1];
    uint64_t oldact_p = context->regs.r[2];
    uint64_t sigset_size = context->regs.r[3];
    int signum = (int) signum_p;
    struct sigaction_arm64 *act_guest = (struct sigaction_arm64 *) g_2_h(act_p);
    struct sigaction_arm64 *oldact_guest = (struct sigaction_arm64 *) g_2_h(oldact_p);
    struct kernel_sigaction act;
    struct kernel_sigaction oldact;

    if (signum < 1 || signum >= NSIG)
        res = -EINVAL;
    else if (act_p == ~0 || oldact_p == ~0)
        res = -EFAULT;
    else {
        memset(&act, 0, sizeof(act));
        memset(&oldact, 0, sizeof(oldact));
        //translate structure
        if (act_p) {
            if (act_guest->_sa_handler == (long)SIG_ERR ||
                act_guest->_sa_handler == (long)SIG_DFL ||
                act_guest->_sa_handler == (long)SIG_IGN) {
                act.k_sa_handler = (__sighandler_t)(long) act_guest->_sa_handler;
                act.sa_mask.__val[0] = act_guest->sa_mask[0];
            } else {
                act.k_sa_handler = (act_guest->sa_flags & SA_SIGINFO)?(__sighandler_t)&wrap_signal_sigaction:(__sighandler_t)&wrap_signal_handler;
                act.sa_mask.__val[0] = act_guest->sa_mask[0];
                act.sa_flags = act_guest->sa_flags | SA_RESTORER | SA_SIGINFO;
                act.sa_restorer = &wrap_signal_restorer;
            }
        }

        if (oldact_p) {
            oldact_guest->_sa_handler = guest_signals_handler[signum];
        }

        //QUESTION : since setting guest handler and host handler is not atomic here is a problem ?
        //           if yes then this syscall must be redirecting towards proot (but is all threads stopped
        //            when we are ptracing a process ?)
        //setup guest syscall handler
        if (act_p) {
            sa_flags[signum] = act_guest->sa_flags;
            guest_signals_handler[signum] = act_guest->_sa_handler;
        }

        res = syscall(SYS_rt_sigaction, signum, act_p?&act:NULL, oldact_p?&oldact:NULL, sigset_size);

        if (oldact_p) {
            //TODO : add guest restorer support
            oldact_guest->sa_flags = sa_flags[signum];
            oldact_guest->sa_mask[0] = oldact.sa_mask.__val[0];
            oldact_guest->sa_restorer = (long)NULL;
        }
    }

    return res;
}

static uint32_t on_sig_stack(struct arm64_target *context, uint64_t sp)
{
    return (sp > context->sas_ss_sp) && (sp - context->sas_ss_sp <= context->sas_ss_size);
}

static uint32_t sas_ss_flags(struct arm64_target *context, uint64_t sp)
{
    if (!context->sas_ss_size)
        return SS_DISABLE;
    return on_sig_stack(context, sp)?SS_ONSTACK:0;
}

uint64_t sigsp(struct arm64_target *prev_context, uint32_t signum)
{
    if ((sa_flags[signum] & SA_ONSTACK) && !sas_ss_flags(prev_context, prev_context->regs.r[31]))
        return prev_context->sas_ss_sp + prev_context->sas_ss_size;

    return prev_context->regs.r[31];
}

long arm64_sigaltstack (struct arm64_target *context)
{
    uint64_t ss_p = context->regs.r[0];
    uint64_t oss_p = context->regs.r[1];
    stack_t_arm64 *ss_guest = (stack_t_arm64 *) g_2_h(ss_p);
    stack_t_arm64 *oss_guest = (stack_t_arm64 *) g_2_h(oss_p);
    long res = 0;
    stack_t_arm64 oss;

    oss.ss_sp = context->sas_ss_sp;
    oss.ss_size = context->sas_ss_size;
    oss.ss_flags = sas_ss_flags(context, context->regs.r[31]);

    if (ss_p) {
        uint64_t ss_sp = ss_guest->ss_sp;
        uint32_t ss_flags = ss_guest->ss_flags;
        uint64_t ss_size = ss_guest->ss_size;

        res = -EPERM;
        if (on_sig_stack(context, context->regs.r[31]))
            goto out;
        res = -EINVAL;
        if (ss_flags != SS_DISABLE && ss_flags != SS_ONSTACK && ss_flags != 0)
            goto out;
        if (ss_flags == SS_DISABLE) {
            ss_size = 0;
            ss_sp = 0;
        } else {
            res = -ENOMEM;
            if (ss_size < MINSIGSTKSZ)
                goto out;
        }
        context->sas_ss_sp = ss_sp;
        context->sas_ss_size = ss_size;
    }
    res = 0;
    if (oss_p)
        *oss_guest = oss;

out:
    return res;
}
