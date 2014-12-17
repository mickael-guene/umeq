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
static stack_t_arm64 ss = {0, SS_DISABLE, 0};

/* signal wrapper functions */
void wrap_signal_handler(int signum)
{
    uint64_t stack_entry = (sa_flags[signum]&SA_ONSTACK)?(ss.ss_flags?0:ss.ss_sp + ss.ss_size):0;

    loop(guest_signals_handler[signum], stack_entry, signum, NULL);
}

void wrap_signal_sigaction(int signum, siginfo_t *siginfo, void *context)
{
    uint64_t stack_entry = (sa_flags[signum]&SA_ONSTACK)?(ss.ss_flags?0:ss.ss_sp + ss.ss_size):0;

    loop(guest_signals_handler[signum], stack_entry, signum, (void *)siginfo);
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

    if (signum >= NSIG)
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
                act.k_sa_handler = (act_guest->sa_flags & SA_SIGINFO)?(__sighandler_t)&wrap_signal_sigaction:&wrap_signal_handler;
                act.sa_mask.__val[0] = act_guest->sa_mask[0];
                act.sa_flags = act_guest->sa_flags | SA_RESTORER;
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
            oldact_guest->sa_flags = oldact.sa_flags & ~SA_RESTORER;
            oldact_guest->sa_mask[0] = oldact.sa_mask.__val[0];
            oldact_guest->sa_restorer = (long)NULL;
        }
    }

    return res;
}

/* FIXME : need to handle SS_ONSTACK case ? (EPERM) */
long arm64_sigaltstack(struct arm64_target *context)
{
    uint64_t ss_p = context->regs.r[0];
    uint64_t oss_p = context->regs.r[1];
    stack_t_arm64 *ss_guest = (stack_t_arm64 *) g_2_h(ss_p);
    stack_t_arm64 *oss_guest = (stack_t_arm64 *) g_2_h(oss_p);
    long res = 0;

    if (oss_p) {
        oss_guest->ss_sp = ss.ss_sp;
        oss_guest->ss_flags = ss.ss_flags;
        oss_guest->ss_size = ss.ss_size;
    }
    if (ss_p) {
        if (ss_guest->ss_flags == 0) {
            if (ss_guest->ss_size < MINSTKSZ) {
                res = -ENOMEM;
            } else {
                ss.ss_sp = ss_guest->ss_sp;
                ss.ss_flags = ss_guest->ss_flags;
                ss.ss_size = ss_guest->ss_size;
            }
        } else if (ss_guest->ss_flags == SS_DISABLE) {
            ss.ss_flags = ss_guest->ss_flags;
        } else
            res = -EINVAL;
    }

    return res;
}
