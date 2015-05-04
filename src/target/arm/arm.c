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

#include "target.h"
#include "arm.h"
#include "arm_private.h"
#include "runtime.h"
#include "arm_signal_types.h"

#define ARM_CONTEXT_SIZE     (4096)

typedef void *armContext;
const char arch_name[] = "arm";

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
            info->_sifields._rt._si_sigval.sival_ptr = (uint64_t) siginfo->si_ptr;
            break;
        case SI_TIMER:
            info->_sifields._timer._si_tid = siginfo->si_timerid;
            info->_sifields._timer._si_overrun = siginfo->si_overrun;
            info->_sifields._timer._si_sigval.sival_int = siginfo->si_int;
            info->_sifields._timer._si_sigval.sival_ptr = (uint64_t) siginfo->si_ptr;
            break;
        case SI_MESGQ:
            info->_sifields._rt._si_pid = siginfo->si_pid;
            info->_sifields._rt._si_uid = siginfo->si_uid;
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

static void setup_sigframe(struct sigframe_arm *frame, struct arm_target *prev_context, siginfo_t *siginfo)
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
    frame->uc.uc_mcontext.arm_pc = prev_context->regs.r[15];
    frame->uc.uc_mcontext.arm_cpsr = prev_context->regs.cpsr;
    frame->uc.uc_mcontext.trap_no = 0;
    if (siginfo) {
        frame->uc.uc_mcontext.error_code = siginfo->si_errno;
        frame->uc.uc_mcontext.fault_address = h_2_g(siginfo->si_addr);
    } else {
        frame->uc.uc_mcontext.error_code = 0;
        frame->uc.uc_mcontext.fault_address = 0;
    }
    /* FIXME: is it ok */
    frame->uc.uc_mcontext.oldmask = 0;
}

static uint32_t setup_rt_frame(uint32_t sp, struct arm_target *prev_context, uint32_t signum, siginfo_t *info)
{
    struct rt_sigframe_arm *frame;

    sp = sp - sizeof(struct rt_sigframe_arm);
    frame = (struct rt_sigframe_arm *) g_2_h(sp);
    setup_siginfo(signum, info, &frame->info);
    frame->sig.uc.uc_flags = 0;
    frame->sig.uc.uc_link = 0;
    /* FIXME: __save_altstack to do ? */
    setup_sigframe(&frame->sig, prev_context, info);

    return sp;
}

static uint32_t setup_frame(uint32_t sp, struct arm_target *prev_context, uint32_t signum)
{
    struct sigframe_arm *frame;

    sp = sp - sizeof(struct sigframe_arm);
    frame = (struct sigframe_arm *) g_2_h(sp);
    frame->uc.uc_flags = 0x5ac3c35a;
    frame->uc.uc_link = 0;
    setup_sigframe(frame, prev_context, NULL);

    return sp;
}

static uint32_t setup_fdpic_return_frame(uint32_t sp, struct umeq_arm_signal_param *sig_param)
{
    unsigned int return_code[] = {0xe59fc004, /* ldr r12, [pc, #4] to read function descriptor */
                                  0xe59c9004, /* ldr r9, [r12, #4] to setup got */
                                  0xe59cf000, /* ldr pc, [r12] to jump into restorer */
                                  0x00000000};/* funcdesc */
    uint32_t *dst;
    int i;

    return_code[3] = sig_param->handler.guest.sa_restorer;
    sp = sp - sizeof(return_code);
    for(i = 0, dst = (unsigned int *)g_2_h(sp);i < sizeof(return_code)/sizeof(return_code[0]); i++)
        *dst++ = return_code[i];

    return sp;
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
        struct umeq_arm_signal_param *sig_param = (struct umeq_arm_signal_param *) param;
        uint32_t return_code_addr;
        uint32_t sp;

        /* choose stack to use */
        if (stack_ptr)
            sp = stack_ptr & ~15UL;
        else
            sp = (prev_context->regs.r[13] - 4 * 16) & ~7;
        /* insert return code sequence */
        if (sig_param->handler.is_fdpic) {
            sp = setup_fdpic_return_frame(sp, sig_param);
            return_code_addr = sp;
        }
        else if (sig_param->handler.guest.sa_restorer) {
            return_code_addr = sig_param->handler.guest.sa_restorer;
        } else {
            sp = setup_return_frame(sp, sig_param->siginfo?1:0);
            return_code_addr = sp;
        }
        /* fill signal frame */
        if (sig_param->siginfo)
            sp = setup_rt_frame(sp, prev_context, signum, sig_param->siginfo);
        else
            sp = setup_frame(sp, prev_context, signum);
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
        if (sig_param->siginfo) {
            struct rt_sigframe_arm *frame = (struct rt_sigframe_arm *) g_2_h(sp);

            context->regs.r[1] = h_2_g(&frame->info);
            context->regs.r[2] = h_2_g(&frame->sig.uc);
        }
        context->regs.is_in_syscall = 0;
        context->is_in_signal = 1 + (stack_ptr?2:0);
        context->trigger_exec = 0;
        context->fdpic_info = prev_context->fdpic_info;
        if (context->fdpic_info.is_fdpic) {
            struct fdpic_funcdesc *funcdesc = (struct fdpic_funcdesc *) g_2_h(sig_param->handler.guest._sa_handler);

            context->regs.r[9] = funcdesc->got;
        }
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
        context->trigger_exec = 0;
        context->fdpic_info = parent_context->fdpic_info;
    } else if (stack_ptr) {
        /* main thread */
        /* get host pointer on fdpic info save on guest stack */
        struct fdpic_info_32 *fdpic_info = g_2_h(*((uint32_t *)g_2_h(stack_ptr-4)));
        fdpic_info->executable_addr = h_2_g(&fdpic_info->executable);
        fdpic_info->dl_addr = h_2_g(&fdpic_info->dl);
        /* init context */
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
        context->fdpic_info = *fdpic_info;
        if (fdpic_info->is_fdpic) {
            /* see arm fdpic abi */
            context->regs.r[7] = fdpic_info->executable_addr;
            if (fdpic_info->dl.nsegs) {
                context->regs.r[8] = fdpic_info->dl_addr;
                context->regs.r[9] = fdpic_info->dl_dynamic_section_addr;
            }
        }
        context->trigger_exec = 1;
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

    if (context->trigger_exec) {
        /* syscall execve exit sequence */
         /* this will be translated into sysexec exit */
        context->regs.is_in_syscall = 2;
        syscall((long) 313, 1);
         /* this will be translated into SIGTRAP */
        context->regs.is_in_syscall = 0;
        syscall((long) 313, 2);
        context->trigger_exec = 0;
    }

    return context->isLooping;
}

static uint32_t getExitStatus(struct target *target)
{
    struct arm_target *context = container_of(target, struct arm_target, target);

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
        context->target.isLooping = isLooping;
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
