#include <sys/syscall.h>
#include <assert.h>
#include <errno.h>
#include <sys/ptrace.h>
#include <signal.h>
#include <asm/prctl.h>
#include <sys/prctl.h>
#include <sys/user.h>

#include <stdio.h>

#include "arm_private.h"
#include "arm_syscall.h"

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

int arm_ptrace(struct arm_target *context)
{
 	int res;
    int request = context->regs.r[0];
    int pid = context->regs.r[1];
    unsigned int addr = context->regs.r[2];
    unsigned int data = context->regs.r[3];

    switch(request) {
    	case PTRACE_TRACEME:
    	case PTRACE_CONT:
        case PTRACE_SETOPTIONS:
        case PTRACE_KILL:
        case PTRACE_SYSCALL:
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

                res = syscall(SYS_ptrace, request, pid, addr, &data_host);
                //fprintf(stderr, "PEEK: @0x%08x => res = %d\n", addr, res);
                *data_guest = data_host;
            }
            break;
        case PTRACE_POKETEXT:
        case PTRACE_POKEDATA:
            {
                unsigned long data_host;

                res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, addr, &data_host);
                data_host = (data_host & 0xffffffff00000000UL) | data;
                res = syscall(SYS_ptrace, request, pid, addr, data_host);
                //fprintf(stderr, "POKE: @0x%08x = 0x%08x => res = %d\n", addr, data, res);
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

                /* FIXME: Need rework with a framework to handle tlsarea usage */
                res = syscall(SYS_ptrace, request, pid, addr, &user_regs);
                res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, user_regs.fs_base + 8, &data_long);
                for(i=0;i<16;i++) {
                    res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, data_long + i * 4, &data_reg);
                    user_regs_arm->uregs[i] = (unsigned int) data_reg;
                }
                res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, data_long + 16 * 4, &data_reg);
                user_regs_arm->uregs[16] = (unsigned int) data_reg;
                user_regs_arm->uregs[17] = user_regs_arm->uregs[0];

                res = 0;
            }
            break;
        case 29:/* PTRACE_GETHBPREGS */
            res = -EIO;
            break;
    	default:
            fprintf(stderr, "ptrace unknown command : %d / 0x%x\n", request, request);
            res = -EIO;
            break;
    }

    return res;
}
