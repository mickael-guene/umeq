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
#include "runtime.h"

#define SA_RESTORER 0x04000000

extern int loop(uint64_t entry, uint64_t stack_entry, uint32_t signum, void *parent_target);
extern void wrap_signal_restorer(void);

struct kernel_sigaction {
    __sighandler_t k_sa_handler;
    unsigned long sa_flags;
    void (*sa_restorer) (void);
    sigset_t sa_mask;
};

struct sigaction_arm64 {
    uint64_t _sa_handler;
    uint64_t sa_mask[16];
    uint32_t sa_flags;
    uint64_t sa_restorer;
};

typedef struct stack_arm64 {
    uint64_t ss_sp;
    uint32_t ss_flags;
    uint32_t ss_size;
} stack_t_arm64;

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

/* global array to hold guest signal handlers. not translated */
static uint64_t guest_signals_handler[NSIG];
static stack_t_arm64 ss = {0, SS_DISABLE, 0};

/* signal wrapper functions */
void wrap_signal_handler(int signum)
{
    loop(guest_signals_handler[signum], ss.ss_flags?0:ss.ss_sp, signum, NULL);
}

/* FIXME: Use of mmap/munmap here need to be rework */
void wrap_signal_sigaction(int signum, siginfo_t *siginfo, void *context)
{
    assert(0);
    guest64_ptr siginfo_guest = mmap64_guest(0, sizeof(siginfo_t_arm64), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS , -1, 0);
    siginfo_t_arm64 *siginfo_arm64 = (siginfo_t_arm64 *) g_2_h_64(siginfo_guest);

    if (siginfo_arm64) {
        siginfo_arm64->si_signo = siginfo->si_signo;
        siginfo_arm64->si_errno = siginfo->si_errno;
        siginfo_arm64->si_code = siginfo->si_code;
        switch(siginfo->si_code) {
            case SI_USER:
            case SI_TKILL:
                siginfo_arm64->_sifields._rt._si_pid = siginfo->si_pid;
                siginfo_arm64->_sifields._rt._si_uid = siginfo->si_uid;
                break;
            case SI_KERNEL:
                if (signum == SIGPOLL) {
                    siginfo_arm64->_sifields._sigpoll._si_band = siginfo->si_band;
                    siginfo_arm64->_sifields._sigpoll._si_fd = siginfo->si_fd;
                } else if (signum == SIGCHLD) {
                    siginfo_arm64->_sifields._sigchld._si_pid = siginfo->si_pid;
                    siginfo_arm64->_sifields._sigchld._si_uid = siginfo->si_uid;
                    siginfo_arm64->_sifields._sigchld._si_status = siginfo->si_status;
                    siginfo_arm64->_sifields._sigchld._si_utime = siginfo->si_utime;
                    siginfo_arm64->_sifields._sigchld._si_stime = siginfo->si_stime;
                } else {
                    siginfo_arm64->_sifields._sigfault._si_addr = h_2_g_64(siginfo->si_addr);
                }
                break;
            case SI_QUEUE:
                siginfo_arm64->_sifields._rt._si_pid = siginfo->si_pid;
                siginfo_arm64->_sifields._rt._si_uid = siginfo->si_uid;
                siginfo_arm64->_sifields._rt._si_sigval.sival_int = siginfo->si_int;
                siginfo_arm64->_sifields._rt._si_sigval.sival_ptr = (uint64_t) siginfo->si_ptr;
                break;
            case SI_TIMER:
                siginfo_arm64->_sifields._timer._si_tid = siginfo->si_timerid;
                siginfo_arm64->_sifields._timer._si_overrun = siginfo->si_overrun;
                siginfo_arm64->_sifields._timer._si_sigval.sival_int = siginfo->si_int;
                siginfo_arm64->_sifields._timer._si_sigval.sival_ptr = (uint64_t) siginfo->si_ptr;
                break;
            case SI_MESGQ:
                siginfo_arm64->_sifields._rt._si_sigval.sival_int = siginfo->si_int;
                siginfo_arm64->_sifields._rt._si_sigval.sival_ptr = (uint64_t) siginfo->si_ptr;
                break;
            case SI_SIGIO:
            case SI_ASYNCIO:
                assert(0);
                break;
            default:
                fatal("si_code %d not yet implemented, signum = %d\n", siginfo->si_code, signum);
        }

        loop(guest_signals_handler[signum], ss.ss_flags?0:ss.ss_sp, signum, (void *)siginfo_guest);
        munmap64_guest(siginfo_guest, sizeof(siginfo_arm64));
    }
}

/* signal syscall handling */
long arm64_rt_sigaction(struct arm64_target *context)
{
    long res;
    uint64_t signum_p = context->regs.r[0];
    uint64_t act_p = context->regs.r[1];
    uint64_t oldact_p = context->regs.r[2];
    int signum = (int) signum_p;
    struct sigaction_arm64 *act_guest = (struct sigaction_arm64 *) g_2_h_64(act_p);
    struct sigaction_arm64 *oldact_guest = (struct sigaction_arm64 *) g_2_h_64(oldact_p);
    struct kernel_sigaction act;
    struct kernel_sigaction oldact;

    if (signum >= NSIG)
        res = -EINVAL;
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
        if (act_p)
            guest_signals_handler[signum] = act_guest->_sa_handler;

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
