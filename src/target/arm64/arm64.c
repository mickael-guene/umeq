#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>

#include "target.h"
#include "arm64.h"
#include "arm64_private.h"
#include "runtime.h"

#define ARM64_CONTEXT_SIZE     (4096)

typedef void *arm64Context;
const char arch_name[] = "arm64";

struct _aarch64_ctx {
    uint32_t magic;
    uint32_t size;
};

typedef union sigval_arm64 {
    uint32_t sival_int;
    uint64_t sival_ptr;
} sigval_t_arm64;

struct siginfo_arm64 {
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
};

typedef struct stack_arm64 {
    uint64_t ss_sp;
    uint32_t ss_flags;
    uint64_t ss_size;
} stack_t_arm64;

typedef struct {
    uint64_t sig[1/*_NSIG_WORDS*/];
} sigset_t_arm64;

struct sigcontext_arm64 {
    uint64_t fault_address;
    /* AArch64 registers */
    uint64_t regs[31];
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
    /* 4K reserved for FP/SIMD state and future expansion */
    uint8_t __reserved[4096] __attribute__((__aligned__(16)));
};

struct ucontext_arm64 {
    uint64_t                uc_flags;
    struct ucontext_arm64   *uc_link;
    stack_t_arm64           uc_stack;
    sigset_t_arm64          uc_sigmask;
    /* glibc uses a 1024-bit sigset_t */
    uint8_t                 __unused[1024 / 8 - sizeof(sigset_t_arm64)];
    /* last for future expansion */
    struct sigcontext_arm64 uc_mcontext;
};

struct rt_sigframe_arm64 {
    struct siginfo_arm64 info;
    struct ucontext_arm64 uc;
    uint64_t fp;
    uint64_t lr;
};

/* backend implementation */
static void init(struct target *target, struct target *prev_target, uint64_t entry, uint64_t stack_ptr, uint32_t signum, void *param)
{
    struct arm64_target *context = container_of(target, struct arm64_target, target);
    struct arm64_target *prev_context = container_of(prev_target, struct arm64_target, target);
    int i;

    context-> isLooping = 1;
    if (signum) {
#if 1
        const unsigned int return_code[] = {0xd2801168, //1: mov     x8, #139
                                            0xd4000001, //svc     0x00000000
                                            0xd503201f, //nop
                                            0x17fffffd};//b 1b
        uint32_t *dst;
        uint64_t return_code_addr;
        struct rt_sigframe_arm64 *frame;
        struct _aarch64_ctx *end;
        /* stp can be ongoing ... jump away and align on 16 bytes */
        /* FIXME: altstack stuff */
        uint64_t sp = (prev_context->regs.r[31] - 128) & ~15UL;
        /* insert return code sequence */
        sp = sp - sizeof(return_code);
        return_code_addr = sp;
        for(i = 0, dst = (unsigned int *)g_2_h(sp);i < sizeof(return_code)/sizeof(return_code[0]); i++)
            *dst++ = return_code[i];
        /* fill signal frame */
        sp = sp - sizeof(struct rt_sigframe_arm64);
        frame = (struct rt_sigframe_arm64 *) g_2_h(sp);
        frame->uc.uc_flags = 0;
        frame->uc.uc_link = NULL;
        /* no need to do __save_altstack */
        frame->fp = prev_context->regs.r[29];
        frame->lr = prev_context->regs.r[30];
        for(i = 0; i < 31; i++)
            frame->uc.uc_mcontext.regs[i] = prev_context->regs.r[i];
        frame->uc.uc_mcontext.sp = prev_context->regs.r[31];
        frame->uc.uc_mcontext.pc = prev_context->regs.pc;
        frame->uc.uc_mcontext.pstate = prev_context->regs.nzcv;
        /* FIXME: fault address ? */
        /* FIXME: sigmask ? */
        end = (struct _aarch64_ctx *) frame->uc.uc_mcontext.__reserved;
        end->magic = 0;
        end->size = 0;

        /* setup new user context */
        for(i = 0; i < 32; i++) {
            context->regs.r[i] = 0;
            context->regs.v[i].v128 = 0;
        }
        context->regs.r[0] = signum;
        /* FIXME: cleanup code with signal once */
        context->regs.r[1] = (uint64_t) param; /* siginfo_t * */
        /* FIXME: use frame->uc address */
        context->regs.r[2] = 0; /* void * */
        context->regs.r[29] = sp + offsetof(struct rt_sigframe_arm64, fp);
        context->regs.r[30] = return_code_addr;
        context->regs.r[31] = sp;
        context->regs.pc = entry;
        context->regs.tpidr_el0 = prev_context->regs.tpidr_el0;
        context->regs.fpcr = prev_context->regs.fpcr;
        context->regs.fpsr = prev_context->regs.fpsr;
        context->regs.nzcv = 0;
        context->pc = entry;
        context->regs.is_in_syscall = 0;
        context->is_in_signal = 1;
        context->regs.is_stepin = 0;
#else
        /* new signal frame */
        uint32_t *dst;
        uint64_t sp;
        unsigned int return_code[] = {0xd2801168, //1: mov     x8, #139
                                      0xd4000001, //svc     0x00000000
                                      0xd503201f, //nop
                                      0x17fffffd};//b 1b

        /* write sigreturn sequence on stack */
        sp = (prev_context->regs.r[31] - 4 * 16 - sizeof(return_code)) & ~7;
        for(i = 0, dst = (unsigned int *)g_2_h(sp);i < sizeof(return_code)/sizeof(return_code[0]); i++)
            *dst++ = return_code[i];
        /* setup context */
        for(i = 0; i < 32; i++) {
            context->regs.r[i] = 0;
            context->regs.v[i].v128 = 0;
        }
        context->regs.r[0] = signum;
        context->regs.r[1] = (uint64_t) param; /* siginfo_t * */
        context->regs.r[2] = 0; /* void * */
        context->regs.r[30] = sp;
        context->regs.r[31] = sp;
        context->regs.pc = entry;
        context->regs.tpidr_el0 = prev_context->regs.tpidr_el0;
        context->regs.fpcr = prev_context->regs.fpcr;
        context->regs.fpsr = prev_context->regs.fpsr;
        context->regs.nzcv = 0;
        context->pc = entry;
        context->regs.is_in_syscall = 0;
        context->is_in_signal = 1;
        context->regs.is_stepin = 0;
#endif
    } else if (param) {
        /* new thread */
        struct arm64_target *parent_context = container_of(param, struct arm64_target, target);
        for(i = 0; i < 32; i++) {
            context->regs.r[i] = parent_context->regs.r[i];
            context->regs.v[i].v128 = parent_context->regs.v[i].v128;
        }
        context->regs.nzcv = parent_context->regs.nzcv;
        context->regs.fpcr = parent_context->regs.fpcr;
        context->regs.fpsr = parent_context->regs.fpsr;
        context->regs.tpidr_el0 = parent_context->regs.r[3];
        context->regs.r[0] = 0;
        context->regs.r[31] = stack_ptr;
        context->regs.pc = entry;
        context->regs.is_in_syscall = 0;
        context->is_in_signal = 0;
        context->regs.is_stepin = 0;
    } else if (stack_ptr) {
        /* main thread */
        for(i = 0; i < 32; i++) {
            context->regs.r[i] = 0;
            context->regs.v[i].v128 = 0;
        }
       	context->regs.r[31] = stack_ptr;
       	context->regs.pc = entry;
        context->regs.tpidr_el0 = 0;
        context->regs.fpcr = 0;
        context->regs.fpsr = 0;
        context->regs.nzcv = 0;
        context->sp_init = stack_ptr;
        context->pc = entry;
        context->regs.is_in_syscall = 0;
        context->is_in_signal = 0;
        context->regs.is_stepin = 0;
        /* syscall execve exit sequence */
         /* this will be translated into sysexec exit */
        context->regs.is_in_syscall = 2;
        syscall((long) 313, 1);
         /* this will be translated into SIGTRAP */
        context->regs.is_in_syscall = 0;
        syscall((long) 313, 2);
    } else {
        //fork;
        //nothing to do
        fatal("Should not come here\n");
    }
}

static void disassemble(struct target *target, struct irInstructionAllocator *ir, uint64_t pc, int maxInsn)
{
    disassemble_arm64(target, ir, pc, maxInsn);
}

static uint32_t isLooping(struct target *target)
{
    struct arm64_target *context = container_of(target, struct arm64_target, target);

    return context->isLooping;
}

static uint32_t getExitStatus(struct target *target)
{
    struct arm64_target *context = container_of(target, struct arm64_target, target);

    return context->exitStatus;
}

/* gdb stuff */
static struct gdb *gdb(struct target *target)
{
    struct arm64_target *context = container_of(target, struct arm64_target, target);

    return &context->gdb;
}

static void gdb_read_registers(struct gdb *gdb, char *buf)
{
    struct arm64_target *context = container_of(gdb, struct arm64_target, gdb);
    int i ,j;
    unsigned long val;
    //uint32_t pc = context->regs.pc;

    for(i=0;i<32;i++) {
        val = context->regs.r[i];
        for(j=0;j<8;j++) {
            unsigned int byte = (val >> (j * 8)) & 0xff;
            unsigned int hnibble = (byte >> 4);
            unsigned int lnibble = (byte & 0xf);

            buf[1] = gdb_tohex(lnibble);
            buf[0] = gdb_tohex(hnibble);

            buf += 2;
        }
    }
    val = context->regs.pc;
    for(j=0;j<8;j++) {
        unsigned int byte = (val >> (j * 8)) & 0xff;
        unsigned int hnibble = (byte >> 4);
        unsigned int lnibble = (byte & 0xf);

        buf[1] = gdb_tohex(lnibble);
        buf[0] = gdb_tohex(hnibble);

        buf += 2;
    }
    val = context->regs.nzcv;
    for(j=0;j<4;j++) {
        unsigned int byte = (val >> (j * 8)) & 0xff;
        unsigned int hnibble = (byte >> 4);
        unsigned int lnibble = (byte & 0xf);

        buf[1] = gdb_tohex(lnibble);
        buf[0] = gdb_tohex(hnibble);

        buf += 2;
    }

    for(i=0;i<12;i++) {
        val = context->regs.v[i].v.lsb;
        for(j=0;j<8;j++) {
            unsigned int byte = (val >> (j * 8)) & 0xff;
            unsigned int hnibble = (byte >> 4);
            unsigned int lnibble = (byte & 0xf);

            buf[1] = gdb_tohex(lnibble);
            buf[0] = gdb_tohex(hnibble);

            buf += 2;
        }
        val = context->regs.v[i].v.msb;
        for(j=0;j<8;j++) {
            unsigned int byte = (val >> (j * 8)) & 0xff;
            unsigned int hnibble = (byte >> 4);
            unsigned int lnibble = (byte & 0xf);

            buf[1] = gdb_tohex(lnibble);
            buf[0] = gdb_tohex(hnibble);

            buf += 2;
        }
    }

    *buf = '\0';
}

/* context handling code */
static int getArm64ContextSize()
{
    return ARM64_CONTEXT_SIZE;
}

static arm64Context createArm64Context(void *memory)
{
    struct arm64_target *context;

    assert(ARM64_CONTEXT_SIZE >= sizeof(*context));
    context = (struct arm64_target *) memory;
    if (context) {
        context->target.init = init;
        context->target.disassemble = disassemble;
        context->target.isLooping = isLooping;
        context->target.getExitStatus = getExitStatus;
        context->target.gdb = gdb;
        context->gdb.state = GDB_STATE_SYNCHRO;
        context->gdb.commandPos = 0;
        context->gdb.isContinue = 0;
        context->gdb.isSingleStepping = 1;
        context->gdb.read_registers = gdb_read_registers;
    }

    return (arm64Context) context;
}

static void deleteArm64Context(arm64Context handle)
{
    ;
}

static struct target *getArm64Target(arm64Context handle)
{
    struct arm64_target *context = (struct arm64_target *) handle;

    return &context->target;
}

static void *getArm64Context(arm64Context handle)
{
    struct arm64_target *context = (struct arm64_target *) handle;

    return &context->regs;
}

/* api */
struct target_arch current_target_arch = {
    arm64_load_image,
    getArm64ContextSize,
    createArm64Context,
    deleteArm64Context,
    getArm64Target,
    getArm64Context
};
