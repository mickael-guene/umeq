#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/ptrace.h>
#include <signal.h>
#include <sys/user.h>
#include <sys/uio.h>
#include <elf.h>
#include <errno.h>

#include <stdio.h>

#include "arm64_private.h"
#include "arm64_syscall.h"
#include "arm64_syscall_types.h"
#include "runtime.h"

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

/* FIXME: check returns value */
static long read_gpr(int pid, struct user_pt_regs_arm64 *regs)
{
    struct user_regs_struct user_regs;
    unsigned long data_long;
    unsigned long data_reg;
    long res;
    int i;

    res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, 0, &user_regs);
    res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, user_regs.fs_base + 8, &data_long);
    for(i=0;i<31;i++) {
        res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, data_long + i * 8, &data_reg);
        regs->regs[i] = data_reg;
    }
    res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, data_long + 31 * 8, &data_reg);
    regs->sp = data_reg;
    res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, data_long + 32 * 8, &data_reg);
    regs->pc = data_reg;
    /* FIXME: use container_of */
    regs->pstate = 0;

    return res;
}

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
                unsigned long data_host;
                unsigned long *data_guest = (unsigned long *) g_2_h(data);

                res = syscall(SYS_ptrace, request, pid, addr, &data_host);
                *data_guest = data_host;
            }
            break;
        case PTRACE_PEEKTEXT:
        case PTRACE_PEEKDATA:
            {
                unsigned long data_host;
                unsigned long *data_guest = (unsigned long *) g_2_h(data);
                void *addr_host = g_2_h(addr);

                res = syscall(SYS_ptrace, request, pid, addr_host, &data_host);
                fprintf(stderr, "PEEK: @0x%08lx => res = %ld => data = 0x%016lx\n", addr, res, data_host);
                *data_guest = data_host;
            }
            break;
        case PTRACE_POKETEXT:
        case PTRACE_POKEDATA:
            {
                void *addr_host = g_2_h(addr);

                res = syscall(SYS_ptrace, request, pid, addr_host, data);
                fprintf(stderr, "POKE: @0x%08lx = 0x%08lx => res = %ld\n", addr, data, res);
            }
            break;
        case PTRACE_GETSIGINFO:
            {
                siginfo_t siginfo;
                siginfo_t_arm64 *siginfo_arm64 = (siginfo_t_arm64 *) g_2_h(data);
                
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
            }
            break;
        case PTRACE_GETREGSET:
            if (addr == NT_PRSTATUS) {
                //fprintf(stderr, "read registers\n");
                /* read gpr registers */
                //struct user_pt_regs_arm64 regs;
                struct iovec *io = (struct iovec *) g_2_h(data);

                fprintf(stderr, "io_base = 0x%016lx\n", io->iov_base);
                fprintf(stderr, "len = %d / %d\n", io->iov_len, sizeof(struct user_pt_regs_arm64));
                res = read_gpr(pid, io->iov_base);
                io->iov_len = sizeof(struct user_pt_regs_arm64);

                /*{
                    int i;
                    struct user_pt_regs_arm64 *regs = (struct user_pt_regs_arm64 *) io->iov_base;

                    for(i = 0; i < 31; i++) {
                        fprintf(stderr, "r[%d]=0x%016lx\n", i, regs->regs[i]);
                    }
                    fprintf(stderr, "sp  =0x%016lx\n", regs->sp);
                    fprintf(stderr, "pc  =0x%016lx\n", regs->pc);
                }
                fprintf(stderr, "res = %ld\n", res);*/

                /* FIXME: add checks .... */
            } else if (addr == NT_ARM_HW_BREAK || addr == NT_ARM_HW_WATCH || addr == NT_PRFPREG) {
                res = -EINVAL;
            } else
                fatal("PTRACE_GETREGSET: addr = %d\n", addr);
            break;
        case PTRACE_SINGLESTEP:
            {
                struct user_pt_regs_arm64 regs;
                struct user_regs_struct user_regs;
                unsigned long data_host;

                /*res = read_gpr(pid, &regs);
                res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, regs.pc + 4, &data_host);
                fprintf(stderr, "data_host = 0x%016lx\n", data_host);
                data_host = (data_host & 0xffffffff00000000UL) | 0xd4200020;
                fprintf(stderr, "data_host = 0x%016lx\n", data_host);
                res = syscall(SYS_ptrace, PTRACE_POKEDATA, pid, regs.pc + 4, &data_host);
                fprintf(stderr, "res = %ld / pid = %d\n", res, pid);*/

                res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, addr, &user_regs);
                fprintf(stderr, "pc = 0x%016lx\n", user_regs.rip);

                res = syscall(SYS_ptrace, PTRACE_SINGLESTEP, pid, 0, 0);
                fprintf(stderr, "res = %ld\n", res);
            }

            break;
#if 0
        case PTRACE_SEIZE:
            res = syscall(SYS_ptrace, request, pid, 0, data);
            break;
        case PTRACE_GETREGSET:
            if (addr == NT_PRSTATUS) {
                /* read gpr registers */
                struct user_pt_regs_arm64 regs;

                res = read_gpr(pid, &regs);
            } else
                fatal("PTRACE_GETREGSET: addr = %d\n", addr);
            break;
#endif
        default:
            fprintf(stderr, "ptrace unknown command : %d / 0x%x / addr = %ld\n", request, request, addr);
            res = -EIO;
            break;
    }

    return res;
}
