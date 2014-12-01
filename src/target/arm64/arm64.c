#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include <signal.h>

#include "target.h"
#include "arm64.h"
#include "arm64_private.h"
#include "runtime.h"
#include "arm64_signal_types.h"

#define ARM64_CONTEXT_SIZE     (4096)

typedef void *arm64Context;
const char arch_name[] = "arm64";

/* FIXME: rework this. See kernel signal.c(copy_siginfo_to_user) */
static void setup_siginfo(uint32_t signum, siginfo_t *siginfo, struct siginfo_arm64 *info)
{
    info->si_signo = siginfo->si_signo;
    info->si_errno = siginfo->si_errno;
    info->si_code = siginfo->si_code;
    switch(siginfo->si_code) {
        case SI_USER:
        case SI_TKILL:
            info->_sifields._rt._si_pid = siginfo->si_pid;
            info->_sifields._rt._si_uid = siginfo->si_uid;
            break;
        case SI_KERNEL:
            if (signum == SIGPOLL) {
                info->_sifields._sigpoll._si_band = siginfo->si_band;
                info->_sifields._sigpoll._si_fd = siginfo->si_fd;
            } else if (signum == SIGCHLD) {
                info->_sifields._sigchld._si_pid = siginfo->si_pid;
                info->_sifields._sigchld._si_uid = siginfo->si_uid;
                info->_sifields._sigchld._si_status = siginfo->si_status;
                info->_sifields._sigchld._si_utime = siginfo->si_utime;
                info->_sifields._sigchld._si_stime = siginfo->si_stime;
            } else {
                info->_sifields._sigfault._si_addr = h_2_g(siginfo->si_addr);
            }
            break;
        case SI_QUEUE:
            info->_sifields._rt._si_pid = siginfo->si_pid;
            info->_sifields._rt._si_uid = siginfo->si_uid;
            info->_sifields._rt._si_sigval.sival_int = siginfo->si_int;
            info->_sifields._rt._si_sigval.sival_ptr = (uint64_t) siginfo->si_ptr;
            break;
        case SI_TIMER:
            info->_sifields._timer._si_tid = siginfo->si_timerid;
            info->_sifields._timer._si_overrun = siginfo->si_overrun;
            info->_sifields._timer._si_sigval.sival_int = siginfo->si_int;
            info->_sifields._timer._si_sigval.sival_ptr = (uint64_t) siginfo->si_ptr;
            break;
        case SI_MESGQ:
            info->_sifields._rt._si_sigval.sival_int = siginfo->si_int;
            info->_sifields._rt._si_sigval.sival_ptr = (uint64_t) siginfo->si_ptr;
            break;
        case SI_SIGIO:
        case SI_ASYNCIO:
            assert(0);
            break;
        default:
            if (siginfo->si_code < 0)
                fatal("si_code %d not yet implemented, signum = %d\n", siginfo->si_code, signum);
            switch(signum) {
                case SIGILL: case SIGFPE: case SIGSEGV: case SIGBUS: case SIGTRAP:
                    info->_sifields._sigfault._si_addr = h_2_g(siginfo->si_addr);
                    break;
                case SIGCHLD:
                    info->_sifields._sigchld._si_pid = siginfo->si_pid;
                    info->_sifields._sigchld._si_uid = siginfo->si_uid;
                    info->_sifields._sigchld._si_status = siginfo->si_status;
                    info->_sifields._sigchld._si_utime = siginfo->si_utime;
                    info->_sifields._sigchld._si_stime = siginfo->si_stime;
                    break;
                case SIGPOLL:
                    info->_sifields._sigpoll._si_band = siginfo->si_band;
                    info->_sifields._sigpoll._si_fd = siginfo->si_fd;
                    break;
                default:
                    fatal("si_code %d with signum not yet implemented, signum = %d\n", siginfo->si_code, signum);
            }
    }
}

static void setup_sigframe(struct rt_sigframe_arm64 *frame, struct arm64_target *prev_context)
{
    int i;
    struct _aarch64_ctx *end;

    frame->fp = prev_context->regs.r[29];
    frame->lr = prev_context->regs.r[30];
    for(i = 0; i < 31; i++)
        frame->uc.uc_mcontext.regs[i] = prev_context->regs.r[i];
    frame->uc.uc_mcontext.sp = prev_context->regs.r[31];
    frame->uc.uc_mcontext.pc = prev_context->regs.pc;
    frame->uc.uc_mcontext.pstate = prev_context->regs.nzcv;
    frame->uc.uc_mcontext.fault_address = 0;
      /* FIXME: need to save simd */
    end = (struct _aarch64_ctx *) frame->uc.uc_mcontext.__reserved;
    end->magic = 0;
    end->size = 0;
}

static uint64_t setup_return_frame(uint64_t sp)
{
    const unsigned int return_code[] = {0xd2801168, //1: mov     x8, #139
                                        0xd4000001, //svc     0x00000000
                                        0xd503201f, //nop
                                        0x17fffffd};//b 1b
    uint32_t *dst;
    int i;

    sp = sp - sizeof(return_code);
    for(i = 0, dst = (unsigned int *)g_2_h(sp);i < sizeof(return_code)/sizeof(return_code[0]); i++)
        *dst++ = return_code[i];

    return sp;
}

static uint64_t setup_rt_frame(uint64_t sp, struct arm64_target *prev_context, uint32_t signum, void *param)
{
    struct rt_sigframe_arm64 *frame;

    sp = sp - sizeof(struct rt_sigframe_arm64);
    frame = (struct rt_sigframe_arm64 *) g_2_h(sp);
    frame->uc.uc_flags = 0;
    frame->uc.uc_link = NULL;
    setup_sigframe(frame, prev_context);
    if (param)
        setup_siginfo(signum, (siginfo_t *) param, &frame->info);

    return sp;
}

/* backend implementation */
static void init(struct target *target, struct target *prev_target, uint64_t entry, uint64_t stack_ptr, uint32_t signum, void *param)
{
    struct arm64_target *context = container_of(target, struct arm64_target, target);
    struct arm64_target *prev_context = container_of(prev_target, struct arm64_target, target);
    int i;

    context-> isLooping = 1;
    if (signum) {
        uint64_t return_code_addr;
        struct rt_sigframe_arm64 *frame;
        uint64_t sp;

        /* choose stack to use */
        if (stack_ptr)
            sp = stack_ptr & ~15UL;
        else /* STP can be ongoing ... jump away and align on 16 bytes */
            sp = (prev_context->regs.r[31] - 128) & ~15UL;
        /* insert return code sequence */
        sp = setup_return_frame(sp);
        return_code_addr = sp;
        /* fill signal frame */
        sp = setup_rt_frame(sp, prev_context, signum, param);
        frame = (struct rt_sigframe_arm64 *) g_2_h(sp);

        /* setup new user context default value */
        for(i = 0; i < 32; i++) {
            context->regs.r[i] = 0;
            context->regs.v[i].v128 = 0;
        }
        context->regs.nzcv = 0;
        context->regs.tpidr_el0 = prev_context->regs.tpidr_el0;
        context->regs.fpcr = prev_context->regs.fpcr;
        context->regs.fpsr = prev_context->regs.fpsr;
        /* fixup register value */
        context->regs.r[0] = signum;
        context->regs.r[29] = sp + offsetof(struct rt_sigframe_arm64, fp);
        context->regs.r[30] = return_code_addr;
        context->regs.r[31] = sp;
        context->regs.pc = entry;
        if (param) {
            context->regs.r[1] = h_2_g(&frame->info);
            context->regs.r[2] = h_2_g(&frame->uc);
        }

        context->regs.is_in_syscall = 0;
        context->is_in_signal = 1;
        context->regs.is_stepin = 0;
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
