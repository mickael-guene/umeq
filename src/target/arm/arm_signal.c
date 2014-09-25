#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <signal.h>
#include <errno.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/syscall.h>

#include "arm_syscall.h"

#define SA_RESTORER	0x04000000
#define MINSTKSZ	2048

extern int loop(uint32_t entry, uint32_t stack_entry, uint32_t signum, void *parent_target);

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

/* global array to hold guest signal handlers. not translated */
static uint32_t guest_signals_handler[NSIG];
static stack_t_arm ss = {0, SS_DISABLE, 0};

/* signal wrapper functions */
void wrap_signal_handler(int signum)
{
    loop(guest_signals_handler[signum], ss.ss_flags?0:ss.ss_sp, signum, NULL);
}

/* FIXME: Use of mmap/munmap here need to be rework */
void wrap_signal_sigaction(int signum, siginfo_t *siginfo, void *context)
{
    siginfo_t_arm *siginfo_arm = mmap(NULL, sizeof(siginfo_t_arm), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_32BIT, -1, 0);

    if (siginfo_arm) {
        siginfo_arm->si_signo = siginfo->si_signo;
        siginfo_arm->si_errno = siginfo->si_errno;
        siginfo_arm->si_code = siginfo->si_code;
        /* FIXME : fill in files according to case */

        loop(guest_signals_handler[signum], ss.ss_flags?0:ss.ss_sp, signum, siginfo_arm);
        munmap(siginfo_arm, sizeof(siginfo_t_arm));
    }
}

/* signal syscall handling */
int arm_rt_sigaction(struct arm_target *context)
{
	int res;
	int signum = context->regs.r[0];
	struct sigaction_arm *act_guest = (struct sigaction_arm *) g_2_h(context->regs.r[1]);
	struct sigaction_arm *oldact_guest = (struct sigaction_arm *) g_2_h(context->regs.r[2]);
	struct kernel_sigaction act;
	struct kernel_sigaction oldact;

    if (signum >= NSIG)
        res = -EINVAL;
    else {
	    memset(&act, 0, sizeof(act));
	    memset(&oldact, 0, sizeof(oldact));
	    //translate structure
	    if (h_2_g(act_guest)) {
	        if (act_guest->_sa_handler == (long)SIG_ERR ||
		        act_guest->_sa_handler == (long)SIG_DFL ||
		        act_guest->_sa_handler == (long)SIG_IGN) {
		        act.k_sa_handler = (__sighandler_t)(long) act_guest->_sa_handler;
		        act.sa_mask.__val[0] = act_guest->sa_mask[0];
	        } else {
		        act.k_sa_handler = (act_guest->sa_flags | SA_SIGINFO)?(__sighandler_t)&wrap_signal_sigaction:&wrap_signal_handler;
		        act.sa_mask.__val[0] = act_guest->sa_mask[0];
		        act.sa_flags = act_guest->sa_flags | SA_RESTORER;
		        act.sa_restorer = &wrap_signal_restorer;
	        }
	    }

	    if (h_2_g(oldact_guest)) {
		    oldact_guest->_sa_handler = guest_signals_handler[signum];
	    }

	    //QUESTION : since setting guest handler and host handler is not atomic here is a problem ?
	    //           if yes then this syscall must be redirecting towards proot (but is all threads stopped
	    //			  when we are ptracing a process ?)
	    //setup guest syscall handler
	    if (h_2_g(act_guest))
		    guest_signals_handler[signum] = act_guest->_sa_handler;

	    res = syscall(SYS_rt_sigaction,(long) signum,
								      (long) act_guest?&act:NULL,
								      (long) oldact_guest?&oldact:NULL,
								      (long) _NSIG / 8);

	    if (h_2_g(oldact_guest)) {
		    //TODO : add guest restorer support
		    oldact_guest->sa_flags = oldact.sa_flags & ~SA_RESTORER;
		    oldact_guest->sa_mask[0] = oldact.sa_mask.__val[0];
		    oldact_guest->sa_restorer = (long)NULL;
	    }
    }

	return res;
}
