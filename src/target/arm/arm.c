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

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include <signal.h>
#include <string.h>
#include <asm/prctl.h>
#include <sys/prctl.h>

#include "target.h"
#include "arm.h"
#include "arm_private.h"
#include "runtime.h"
#include "arm_signal_types.h"
#include "jitter.h"
#include "target.h"
#include "be.h"

#define ARM_CONTEXT_SIZE     (4096)

/* get code boundaries */
extern char __executable_start;
extern char __etext;

typedef void *armContext;
const char arch_name[] = "arm";

#define SIGNAL_LOCATION_UNKNOWN     0
#define SIGNAL_LOCATION_UMEQ_CODE   1
#define SIGNAL_LOCATION_JITTER_EXEC 2

/* FIXME: rework this. See kernel signal.c(copy_siginfo_to_user) */
static void setup_siginfo(uint32_t signum, siginfo_t *siginfo, struct siginfo_arm *info)
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
            info->_sifields._rt._si_sigval.sival_ptr = (uint64_t) ptr_2_int(siginfo->si_ptr);
            break;
        case SI_TIMER:
            info->_sifields._timer._si_tid = siginfo->si_timerid;
            info->_sifields._timer._si_overrun = siginfo->si_overrun;
            info->_sifields._timer._si_sigval.sival_int = siginfo->si_int;
            info->_sifields._timer._si_sigval.sival_ptr = (uint64_t) ptr_2_int(siginfo->si_ptr);
            break;
        case SI_MESGQ:
            info->_sifields._rt._si_pid = siginfo->si_pid;
            info->_sifields._rt._si_uid = siginfo->si_uid;
            info->_sifields._rt._si_sigval.sival_int = siginfo->si_int;
            info->_sifields._rt._si_sigval.sival_ptr = (uint64_t) ptr_2_int(siginfo->si_ptr);
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

static uint32_t find_insn_offset(uint32_t guest_pc, int offset)
{
    struct backend *backend;
    jitContext handle;
    struct irInstructionAllocator *ir;
    struct arm_target context;
    char jitBuffer[16 * KB];
    char *beMemory = alloca(256 * KB);
    char *jitterMemory = alloca(256 * KB);

    memset(&context, 0, sizeof(context));

    backend = createBackend(beMemory, 256 * KB);
    handle = createJitter(jitterMemory, backend, 256 * KB);
    ir = getIrInstructionAllocator(handle);

    if (guest_pc & 1)
        disassemble_thumb_with_marker(&context, ir, guest_pc, 40);
    else
        disassemble_arm_with_marker(&context, ir, guest_pc, 40);

    return findInsn(handle, jitBuffer, sizeof(jitBuffer), offset);
}

static uint32_t restore_precise_pc(struct arm_target *prev_context, ucontext_t *ucp, int *in_signal_location)
{
    uint32_t res = prev_context->regs.r[15];

    if (ucp) {
#ifdef __i386__
        void *host_pc_signal = (void *)ucp->uc_mcontext.gregs[REG_EIP];
#else
        void *host_pc_signal = (void *)ucp->uc_mcontext.gregs[REG_RIP];
#endif
        /* we can be in three distinct area :
        - in jitting area
        - in umeq helper function
        - in umeq code, in that case we are not currently emulating guest instruction */
        if (prev_context->regs.helper_pc) {
            /* we are inside an helper */
            *in_signal_location = SIGNAL_LOCATION_JITTER_EXEC;
            res = prev_context->regs.helper_pc;
        } else if (host_pc_signal >= (void *)&__executable_start &&
               host_pc_signal < (void *)&__etext) {
            /* we are somewhere in umeq code */
            *in_signal_location = SIGNAL_LOCATION_UMEQ_CODE;
            res = prev_context->regs.r[15];
        } else {
            struct tls_context *current_tls_context;
            struct cache *cache;
            void *jit_host_start_pc;
            uint32_t jit_guest_start_pc;
            uint32_t insn_offset;

            current_tls_context = get_tls_context();
            cache = current_tls_context->cache;

            jit_guest_start_pc = cache->lookup_pc(cache, host_pc_signal, &jit_host_start_pc);
            assert(jit_guest_start_pc != 0);
            insn_offset = find_insn_offset(jit_guest_start_pc, host_pc_signal - jit_host_start_pc);
            assert(insn_offset != ~0);

            res = prev_context->regs.r[15] + insn_offset;
        }
    }

    return res;
}

static void setup_sigframe(struct sigframe_arm *frame, struct arm_target *prev_context,
                           struct host_signal_info *signal_info, int *in_signal_location)
{
    frame->uc.uc_mcontext.arm_r0 = prev_context->regs.r[0];
    frame->uc.uc_mcontext.arm_r1 = prev_context->regs.r[1];
    frame->uc.uc_mcontext.arm_r2 = prev_context->regs.r[2];
    frame->uc.uc_mcontext.arm_r3 = prev_context->regs.r[3];
    frame->uc.uc_mcontext.arm_r4 = prev_context->regs.r[4];
    frame->uc.uc_mcontext.arm_r5 = prev_context->regs.r[5];
    frame->uc.uc_mcontext.arm_r6 = prev_context->regs.r[6];
    frame->uc.uc_mcontext.arm_r7 = prev_context->regs.r[7];
    frame->uc.uc_mcontext.arm_r8 = prev_context->regs.r[8];
    frame->uc.uc_mcontext.arm_r9 = prev_context->regs.r[9];
    frame->uc.uc_mcontext.arm_r10 = prev_context->regs.r[10];
    frame->uc.uc_mcontext.arm_fp = prev_context->regs.r[11];
    frame->uc.uc_mcontext.arm_ip = prev_context->regs.r[12];
    frame->uc.uc_mcontext.arm_sp = prev_context->regs.r[13];
    frame->uc.uc_mcontext.arm_lr = prev_context->regs.r[14];
    /* Update prev_context pc so we are sure to detect pc modifications on signal exit */
    prev_context->regs.r[15] = restore_precise_pc(prev_context, signal_info->context, in_signal_location);
    frame->uc.uc_mcontext.arm_pc = prev_context->regs.r[15];
    frame->uc.uc_mcontext.arm_cpsr = prev_context->regs.cpsr;
    frame->uc.uc_mcontext.trap_no = 0;
    frame->uc.uc_mcontext.error_code = 0;
    frame->uc.uc_mcontext.fault_address = 0;
    if (signal_info->is_sigaction_handler) {
        frame->uc.uc_mcontext.error_code = signal_info->siginfo->si_errno;
        frame->uc.uc_mcontext.fault_address = h_2_g(signal_info->siginfo->si_addr);
    }
    /* FIXME: is it ok */
    frame->uc.uc_mcontext.oldmask = 0;
}

static uint32_t setup_rt_frame(uint32_t sp, struct arm_target *prev_context, uint32_t signum,
                               struct host_signal_info *signal_info, int *in_signal_location)
{
    struct rt_sigframe_arm *frame;

    sp = sp - sizeof(struct rt_sigframe_arm);
    frame = (struct rt_sigframe_arm *) g_2_h(sp);
    frame->sig.uc.uc_flags = 0;
    frame->sig.uc.uc_link = 0;
    setup_sigframe(&frame->sig, prev_context, signal_info, in_signal_location);
    setup_siginfo(signum, (siginfo_t *) signal_info->siginfo, &frame->info);

    return sp;
}

static uint32_t setup_frame(uint32_t sp, struct arm_target *prev_context, uint32_t signum,
                            struct host_signal_info *signal_info, int *in_signal_location)
{
    struct sigframe_arm *frame;

    sp = sp - sizeof(struct sigframe_arm);
    frame = (struct sigframe_arm *) g_2_h(sp);
    frame->uc.uc_flags = 0x5ac3c35a;
    frame->uc.uc_link = 0;
    setup_sigframe(frame, prev_context, signal_info, in_signal_location);

    return sp;
}

static int restore_sigframe(struct sigframe_arm *frame, struct arm_target *prev_context)
{
    int is_pc_change = (prev_context->regs.r[15] != frame->uc.uc_mcontext.arm_pc);

    prev_context->regs.r[0] = frame->uc.uc_mcontext.arm_r0;
    prev_context->regs.r[1] = frame->uc.uc_mcontext.arm_r1;
    prev_context->regs.r[2] = frame->uc.uc_mcontext.arm_r2;
    prev_context->regs.r[3] = frame->uc.uc_mcontext.arm_r3;
    prev_context->regs.r[4] = frame->uc.uc_mcontext.arm_r4;
    prev_context->regs.r[5] = frame->uc.uc_mcontext.arm_r5;
    prev_context->regs.r[6] = frame->uc.uc_mcontext.arm_r6;
    prev_context->regs.r[7] = frame->uc.uc_mcontext.arm_r7;
    prev_context->regs.r[8] = frame->uc.uc_mcontext.arm_r8;
    prev_context->regs.r[9] = frame->uc.uc_mcontext.arm_r9;
    prev_context->regs.r[10] = frame->uc.uc_mcontext.arm_r10;
    prev_context->regs.r[11] = frame->uc.uc_mcontext.arm_fp;
    prev_context->regs.r[12] = frame->uc.uc_mcontext.arm_ip;
    prev_context->regs.r[13] = frame->uc.uc_mcontext.arm_sp;
    prev_context->regs.r[14] = frame->uc.uc_mcontext.arm_lr;
    prev_context->regs.r[15] = frame->uc.uc_mcontext.arm_pc;
    prev_context->regs.cpsr = frame->uc.uc_mcontext.arm_cpsr;

    return is_pc_change;
}

static uint32_t setup_return_frame(uint32_t sp, int is_rt)
{
    unsigned int return_code[] = {0xe3a07077, //mov     r0, #119
                                  0xef000000, //svc     0x00000000
                                  0xe320f000, //nop
                                  0xeafffffd};//b svc
    uint32_t *dst;
    int i;

    if (is_rt)
        return_code[0] = 0xe3a070ad;
    sp = sp - sizeof(return_code);
    for(i = 0, dst = (unsigned int *)g_2_h(sp);i < sizeof(return_code)/sizeof(return_code[0]); i++)
        *dst++ = return_code[i];

    return sp;
}

/* backend implementation */
static void init(struct target *target, struct target *prev_target, uint64_t entry, uint64_t stack_ptr, uint32_t signum, void *param)
{
    struct arm_target *context = container_of(target, struct arm_target, target);
    struct arm_target *prev_context = container_of(prev_target, struct arm_target, target);
    int i;

    context-> isLooping = 1;
    if (signum) {
        uint32_t return_code_addr;
        struct sigframe_arm *frame;
        uint32_t sp;
        struct host_signal_info *signal_info = (struct host_signal_info *) param;

        /* choose stack to use */
        sp = sigsp(prev_context, signum);
        /* stm can be ongoing ... jump away and align on 16 bytes */
        sp = (sp - 128) & ~15UL;
        context->start_on_sig_stack = on_sig_stack(prev_context, sp);
        /* insert return code sequence */
        sp = setup_return_frame(sp, signal_info->is_sigaction_handler);
        return_code_addr = sp;

        /* fill signal frame */
        if (signal_info->is_sigaction_handler) {
            sp = setup_rt_frame(sp, prev_context, signum, signal_info, &context->in_signal_location);
            frame = &((struct rt_sigframe_arm *) g_2_h(sp))->sig;
        } else {
            sp = setup_frame(sp, prev_context, signum, signal_info, &context->in_signal_location);
            frame = (struct sigframe_arm *) g_2_h(sp);
        }

        /* setup new user context default value */
        for(i = 0; i < 16; i++)
            context->regs.r[i] = prev_context->regs.r[i];
        for(i = 0; i < 32; i++)
            context->regs.e.d[i] = prev_context->regs.e.d[i];
        context->regs.cpsr = prev_context->regs.cpsr;
        context->regs.fpscr = prev_context->regs.fpscr;
        context->regs.c13_tls2 = prev_context->regs.c13_tls2;
        context->regs.shifter_carry_out = prev_context->regs.shifter_carry_out;
        context->regs.reg_itstate = 0;
        context->disa_itstate = 0;
        /* fixup register value */
        context->regs.r[0] = signum;
        context->regs.r[13] = sp;
        context->regs.r[14] = return_code_addr;
        context->regs.r[15] = entry;
        if (signal_info->is_sigaction_handler) {
            struct rt_sigframe_arm *rt_frame = (struct rt_sigframe_arm *) g_2_h(sp);

            context->regs.r[1] = h_2_g(&rt_frame->info);
            context->regs.r[2] = h_2_g(&rt_frame->sig.uc);
        }
        context->frame = frame;
        context->param = param;
        context->prev_context = prev_context;
        context->regs.is_in_syscall = 0;
        context->is_in_signal = 1;
        context->regs.fp_status = prev_context->regs.fp_status;
        context->regs.fp_status_simd = prev_context->regs.fp_status_simd;
        context->sas_ss_sp = prev_context->sas_ss_sp;
        context->sas_ss_size = prev_context->sas_ss_size;
        context->exitStatus = 0;
    } else if (param) {
        /* new thread */
        struct arm_target *parent_context = container_of(param, struct arm_target, target);

        for(i = 0; i < 15; i++)
            context->regs.r[i] = parent_context->regs.r[i];
        context->regs.c13_tls2 = parent_context->regs.r[3];
        context->regs.r[0] = 0;
        context->regs.r[13] = (uint32_t) stack_ptr;
        context->regs.r[15] = (uint32_t) entry;
        context->regs.cpsr = 0;
        context->regs.reg_itstate = parent_context->regs.reg_itstate; //should be zero
        assert(context->regs.reg_itstate == 0);
        context->disa_itstate = 0;
        context->regs.is_in_syscall = 0;
        context->regs.fpscr = 0;
        context->is_in_signal = 0;
        context->regs.fp_status = parent_context->regs.fp_status;
        context->regs.fp_status_simd = prev_context->regs.fp_status_simd;
        context->sas_ss_sp = 0;
        context->sas_ss_size = 0;
        context->start_on_sig_stack = 0;
        context->in_signal_location = SIGNAL_LOCATION_UNKNOWN;
    } else if (stack_ptr) {
        /* main thread */
        for(i = 0; i < 15; i++)
            context->regs.r[i] = 0;
        context->regs.r[13] = (uint32_t) stack_ptr;
        context->regs.r[15] = (uint32_t) entry;
        context->regs.cpsr = 0;
        context->sp_init = (uint32_t) stack_ptr;
        context->regs.reg_itstate = 0;
        context->disa_itstate = 0;
        context->is_in_signal = 0;
        context->regs.fpscr = 0;
        set_float_detect_tininess(float_tininess_before_rounding, &context->regs.fp_status);
        set_float_rounding_mode(float_round_nearest_even, &context->regs.fp_status);
        set_float_exception_flags(0, &context->regs.fp_status);
        set_floatx80_rounding_precision(0, &context->regs.fp_status);
        set_flush_to_zero(0, &context->regs.fp_status);
        set_flush_inputs_to_zero(0, &context->regs.fp_status);
        set_default_nan_mode(0, &context->regs.fp_status);
        set_float_detect_tininess(float_tininess_before_rounding, &context->regs.fp_status_simd);
        set_float_rounding_mode(float_round_nearest_even, &context->regs.fp_status_simd);
        set_float_exception_flags(0, &context->regs.fp_status_simd);
        set_floatx80_rounding_precision(0, &context->regs.fp_status_simd);
        set_flush_to_zero(1, &context->regs.fp_status_simd);
        set_flush_inputs_to_zero(1, &context->regs.fp_status_simd);
        set_default_nan_mode(1, &context->regs.fp_status_simd);
        context->sas_ss_sp = 0;
        context->sas_ss_size = 0;
        context->start_on_sig_stack = 0;
        context->in_signal_location = SIGNAL_LOCATION_UNKNOWN;
    } else {
        //fork;
        //nothing to do
    }
}

static void disassemble(struct target *target, struct irInstructionAllocator *ir, uint64_t pc, int maxInsn)
{
    if (pc & 1)
        disassemble_thumb(target, ir, pc, maxInsn);
    else
        disassemble_arm(target, ir, pc, maxInsn);
}

static uint32_t isLooping(struct target *target)
{
    struct arm_target *context = container_of(target, struct arm_target, target);

    /* here we detect a siglongjmp (or equivalent code) call that return in a
       previous context. But we cannot return in previous context using
       request_signal_alternate_exit() if we were not in jitter executing code.
       So in that case we stay in signal context. This can occur on asynchronous
       signal that exit using siglongjmp. An example of such code is in
       pthread_cancellation. */
    /* FIXME: in UMEQ_CODE case can we return in previous context using custom
       sigsetjmp / siglongjmp sequence .... */
    if (context->is_in_signal &&
        context->in_signal_location == SIGNAL_LOCATION_JITTER_EXEC &&
        is_out_of_signal_stack(context)) {
        context->isLooping = 0;
        context->exitStatus = 1;
    }

    return context->isLooping;
}

static uint32_t isLooping_firstcall(struct target *target)
{
    struct arm_target *context = container_of(target, struct arm_target, target);

    ptrace_exec_event(context);
    /* avoid calling ptrace_exec_event for following isLooping call. */
    /* this will save cycles in main loop */
    context->target.isLooping = isLooping;

    return context->isLooping;
}

static uint32_t getExitStatus(struct target *target)
{
    struct arm_target *context = container_of(target, struct arm_target, target);

    if (context->is_in_signal) {
        if (context->exitStatus) {
            struct host_signal_info *signal_info = (struct host_signal_info *) context->param;
            int i;

            assert(context->in_signal_location == SIGNAL_LOCATION_JITTER_EXEC);
            for(i = 0; i < 16; i++)
                context->prev_context->regs.r[i] = context->regs.r[i];
            context->prev_context->regs.cpsr = context->regs.cpsr;
            context->prev_context->backend->request_signal_alternate_exit(context->prev_context->backend,
                                                                          signal_info->context,
                                                                          context->prev_context->regs.r[15]);
        } else {
            /* we check if parent frame was modify by signal handler */
            int is_pc_change = restore_sigframe(context->frame, context->prev_context);
            if (is_pc_change) {
                struct host_signal_info *signal_info = (struct host_signal_info *) context->param;

                assert(context->in_signal_location == SIGNAL_LOCATION_JITTER_EXEC);
                context->prev_context->backend->request_signal_alternate_exit(context->prev_context->backend,
                                                                              signal_info->context,
                                                                              context->prev_context->regs.r[15]);
            }
        }
    }

    return context->exitStatus;
}

static int getArmContextSize()
{
    return ARM_CONTEXT_SIZE;
}

static armContext createArmContext(void *memory, struct backend *backend)
{
    struct arm_target *context;

    assert(ARM_CONTEXT_SIZE >= sizeof(*context));
    context = (struct arm_target *) memory;
    if (context) {
        context->target.init = init;
        context->target.disassemble = disassemble;
        context->target.isLooping = isLooping_firstcall;
        context->target.getExitStatus = getExitStatus;
        context->backend = backend;
    }

    return (armContext) context;
}

static void deleteArmContext(armContext handle)
{
    ;
}

static struct target *getArmTarget(armContext handle)
{
    struct arm_target *context = (struct arm_target *) handle;

    return &context->target;
}

static void *getArmContext(armContext handle)
{
    struct arm_target *context = (struct arm_target *) handle;

    return &context->regs;
}

static int getArmNbOfPcBitToDrop()
{
    return 1;
}

/* api */
struct target_arch current_target_arch = {
    arm_load_image,
    getArmContextSize,
    createArmContext,
    deleteArmContext,
    getArmTarget,
    getArmContext,
    getArmNbOfPcBitToDrop
};
