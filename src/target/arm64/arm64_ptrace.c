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
#include "arm64_helpers.h"

#define INSN(msb, lsb) ((insn >> (lsb)) & ((1 << ((msb) - (lsb) + 1))-1))

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
    res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, data_long + offsetof(struct arm64_registers, nzcv), &data_reg);
    regs->pstate = (uint32_t) data_reg;

    return res;
}

static long save_step_insn(int pid, int insn)
{
    long res;
    struct user_regs_struct user_regs;
    unsigned long data_long;
    unsigned long data_reg;

    res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, 0, &user_regs);
    res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, user_regs.fs_base + 8, &data_long);
    res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, data_long + offsetof(struct arm64_registers, step_insn), &data_reg);
    data_reg = (data_reg & 0xffffffff00000000UL) | insn;
    res = syscall(SYS_ptrace, PTRACE_POKETEXT, pid, data_long + offsetof(struct arm64_registers, step_insn), data_reg);

    return res;
}

static long compute_next_pc(int pid, uint64_t *next_pc)
{
    long res;
    struct user_pt_regs_arm64 regs;
    unsigned long data_reg;
    uint32_t insn;

    res = read_gpr(pid, &regs);
    if (res)
        goto compute_next_pc_error;
    res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, regs.pc, &data_reg);
    if (res)
        goto compute_next_pc_error;
    insn = data_reg;
    if (INSN(30, 26) == 0x5) {
        int64_t imm26 = INSN(25,0) << 2;

        imm26 = ((imm26 << 36) >> 36);
        *next_pc = regs.pc + imm26;
    } else if (INSN(30, 25) == 0x1a) {
        assert(0 && "compare & branch immediate");
    } else if (INSN(30, 25) == 0x1b) {
        assert(0 && "test & branch immediate");
    } else if (INSN(31, 25) == 0x2a) {
        int64_t imm19 = INSN(23,5) << 2;
        int cond = INSN(3,0);
        int pred = arm64_hlp_compute_flags_pred(0/*context*/, cond, (uint32_t)regs.pstate);

        imm19 = (imm19 << 43) >> 43;
        if (pred) {
            *next_pc = regs.pc + imm19;
        } else
            *next_pc = regs.pc + 4;
    } else if (INSN(31, 25) == 0x6b) {
        int rn = INSN(9, 5);

        *next_pc = rn==31?regs.sp:regs.regs[rn];
    } else
        *next_pc = regs.pc + 4;

    compute_next_pc_error:
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
                unsigned long *data_guest = (unsigned long *) g_2_h(data);

                res = syscall(SYS_ptrace, request, pid, addr, data_guest);
            }
            break;
        case PTRACE_PEEKTEXT:
        case PTRACE_PEEKDATA:
            {
                unsigned long *data_guest = (unsigned long *) g_2_h(data);
                void *addr_host = g_2_h(addr);

                res = syscall(SYS_ptrace, request, pid, addr_host, data_guest);
                //fprintf(stderr, "PEEK: @0x%08lx => res = %ld => data = 0x%016lx\n", addr, res, *data_guest);
            }
            break;
        case PTRACE_POKETEXT:
        case PTRACE_POKEDATA:
            {
                void *addr_host = g_2_h(addr);

                res = syscall(SYS_ptrace, request, pid, addr_host, data);
                //fprintf(stderr, "POKE: @0x%08lx = 0x%08lx => res = %ld\n", addr, data, res);
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
                struct iovec *io = (struct iovec *) g_2_h(data);

                assert(io->iov_len >= sizeof(struct user_pt_regs_arm64));
                res = read_gpr(pid, io->iov_base);
                io->iov_len = sizeof(struct user_pt_regs_arm64);
            } else if (addr == NT_PRFPREG) {
                /* FIXME: implement simd */
                res = -EINVAL;
            } else if (addr == NT_ARM_TLS) {
                struct iovec *io = (struct iovec *) g_2_h(data);
                struct user_regs_struct user_regs;
                unsigned long data_long;

                res = syscall(SYS_ptrace, PTRACE_GETREGS, pid, 0, &user_regs);
                res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, user_regs.fs_base + 8, &data_long);
                assert(io->iov_len >= sizeof(uint64_t));
                res = syscall(SYS_ptrace, PTRACE_PEEKTEXT, pid, data_long + offsetof(struct arm64_registers, tpidr_el0), io->iov_base);
                io->iov_len = sizeof(uint64_t);
            } else if (addr == NT_ARM_HW_BREAK || addr == NT_ARM_HW_WATCH) {
                res = -EINVAL;
            } else
                fatal("PTRACE_GETREGSET: addr = %d\n", addr);
            break;
        case PTRACE_SINGLESTEP:
            {
                uint64_t next_pc;
                uint64_t data_host;

                res = compute_next_pc(pid, &next_pc);
                if (res)
                    goto ptrace_single_step_error;
                /* save next insn and replace it with brk #1 */
                res = syscall(SYS_ptrace, PTRACE_PEEKDATA, pid, next_pc, &data_host);
                if (res)
                    goto ptrace_single_step_error;
                res = save_step_insn(pid, (uint32_t) data_host);
                if (res)
                    goto ptrace_single_step_error;
                data_host = (data_host & 0xffffffff00000000UL) | 0xd4200020;
                res = syscall(SYS_ptrace, PTRACE_POKEDATA, pid, next_pc, data_host);
                if (res)
                    goto ptrace_single_step_error;
                /* continue */
                res = syscall(SYS_ptrace, PTRACE_CONT, pid, 0, 0);
            }
            ptrace_single_step_error:
            break;
        default:
            fprintf(stderr, "ptrace unknown command : %d / 0x%x / addr = %ld\n", request, request, addr);
            res = -EIO;
            break;
    }

    return res;
}
