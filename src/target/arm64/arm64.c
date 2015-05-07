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

static int restore_sigframe(struct rt_sigframe_arm64 *frame, struct arm64_target *prev_context)
{
    int is_pc_change = (prev_context->regs.pc != frame->uc.uc_mcontext.pc);
    int i;

    for(i = 0; i < 31; i++)
        prev_context->regs.r[i] = frame->uc.uc_mcontext.regs[i];
    prev_context->regs.r[31] = frame->uc.uc_mcontext.sp;
    prev_context->regs.pc = frame->uc.uc_mcontext.pc;
    prev_context->regs.nzcv = frame->uc.uc_mcontext.pstate;

    return is_pc_change;
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

static uint64_t setup_rt_frame(uint64_t sp, struct arm64_target *prev_context, uint32_t signum, struct host_signal_info *signal_info)
{
    struct rt_sigframe_arm64 *frame;

    sp = sp - sizeof(struct rt_sigframe_arm64);
    frame = (struct rt_sigframe_arm64 *) g_2_h(sp);
    frame->uc.uc_flags = 0;
    frame->uc.uc_link = NULL;
    setup_sigframe(frame, prev_context);
    if (signal_info)
        setup_siginfo(signum, (siginfo_t *) signal_info->siginfo, &frame->info);

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
        context->frame = frame;
        context->param = param;
        context->prev_context = prev_context;

        context->regs.is_in_syscall = 0;
        context->is_in_signal = 1 + (stack_ptr?2:0);
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

static uint32_t isLooping_firstcall(struct target *target)
{
    struct arm64_target *context = container_of(target, struct arm64_target, target);

    ptrace_exec_event(context);
    /* avoid calling ptrace_exec_event for following isLooping call. */
    /* this will save cycles in main loop */
    context->target.isLooping = isLooping;

    return context->isLooping;
}

static uint32_t getExitStatus(struct target *target)
{
    struct arm64_target *context = container_of(target, struct arm64_target, target);

    if (context->is_in_signal && context->param) {
        /* we check if parent frame was modify by signal handler */
        int is_pc_change = restore_sigframe(context->frame, context->prev_context);
        if (is_pc_change) {
            struct host_signal_info *signal_info = (struct host_signal_info *) context->param;

            context->prev_context->backend->request_signal_alternate_exit(context->prev_context->backend,
                                                                          signal_info->context,
                                                                          context->prev_context->regs.pc);
        }
    }

    return context->exitStatus;
}

/* context handling code */
static int getArm64ContextSize()
{
    return ARM64_CONTEXT_SIZE;
}

static arm64Context createArm64Context(void *memory, struct backend *backend)
{
    struct arm64_target *context;

    assert(ARM64_CONTEXT_SIZE >= sizeof(*context));
    context = (struct arm64_target *) memory;
    if (context) {
        context->target.init = init;
        context->target.disassemble = disassemble;
        context->target.isLooping = isLooping_firstcall;
        context->target.getExitStatus = getExitStatus;
        context->backend = backend;
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

static int getArm64NbOfPcBitToDrop()
{
    return 2;
}

/* api */
struct target_arch current_target_arch = {
    arm64_load_image,
    getArm64ContextSize,
    createArm64Context,
    deleteArm64Context,
    getArm64Target,
    getArm64Context,
    getArm64NbOfPcBitToDrop
};
