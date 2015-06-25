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
#include <assert.h>
#include <errno.h>

#include "arm64_private.h"
#include "sysnums-arm64.h"
#include "hownums-arm64.h"
#include "runtime.h"
#include "syscall64_64.h"
#include "arm64_syscall.h"

#define PROOT_SYSCALL_VOID      -2

void arm64_hlp_syscall(uint64_t regs)
{
    struct arm64_target *context = container_of((void *) regs, struct arm64_target, regs);
    uint32_t no;
    Sysnum no_neutral;
    Syshow how;
    long res = -ENOSYS;

    /* syscall entry sequence */
    ptrace_syscall_enter(context);
    /* read syscall number in case x8 has been change on syscall entry by a tracer.
        Note that we don't follow the way kernel is doing it. Kernel is not re-reading
       x8 but need PTRACE_SETREGSET/NT_ARM_SYSTEM_CALL ptrace call to change syscall number.
       In out case we do it the other way, we re-read x8 but don't care about PTRACE_SETREGSET/NT_ARM_SYSTEM_CALL
       since it's easier to do it this way
    */
    no = context->regs.r[8];
    /* proot use number syscall PROOT_SYSCALL_VOID number to nullify syscall. */
    if (no == PROOT_SYSCALL_VOID) {
        res = 0;
        goto skip_syscall;
    }
    /* translate syscall nb into neutral no */
    if (no >= sysnums_arm64_nb)
        fatal("Out of range syscall number %d >= %d\n", no, sysnums_arm64_nb);
    else
        no_neutral = sysnums_arm64[no];
    how = syshow_arm64[no_neutral];

	/* so how we handle this sycall */
    if (how == HOW_64_to_64) {
        res = syscall64_64(no_neutral, context->regs.r[0], context->regs.r[1], context->regs.r[2], 
                                            context->regs.r[3], context->regs.r[4], context->regs.r[5]);
    } else if (how == HOW_custom_implementation) {
        switch(no_neutral) {
            case PR_uname:
                res = arm64_uname(context);
                break;
            case PR_brk:
                res = arm64_brk(context);
                break;
            case PR_openat:
                res = arm64_openat(context);
                break;
            case PR_fstat:
                res = arm64_fstat(context);
                break;
            case PR_rt_sigaction:
                res = arm64_rt_sigaction(context);
                break;
            case PR_fstatat64:
                res = arm64_fstatat64(context);
                break;
            case PR_clone:
                res = arm64_clone(context);
                break;
            case PR_rt_sigreturn:
                context->isLooping = 0;
                context->exitStatus = 0;
                res = 0;
                break;
            case PR_sigaltstack:
                res = arm64_sigaltstack(context);
                break;
            case PR_ptrace:
                res = arm64_ptrace(context);
                break;
            case PR_exit:
                if (context->is_in_signal) {
                    /* in case we are in signal handler we leave immediatly */
                    res = syscall(SYS_exit, context->regs.r[0]);
                } else {
                    /* if inside the thread then we will release context memory */
                    context->isLooping = 0;
                    context->exitStatus = context->regs.r[0];
                    /* stay in syscall */
                    return ;
                }
                break;
            case PR_mmap:
                res = arm64_mmap(context);
                break;
            case PR_munmap:
                res = arm64_munmap(context);
                break;
            case PR_mremap:
                res = arm64_mremap(context);
                break;
            case PR_shmat:
                res = arm64_shmat(context);
                break;
            case PR_shmdt:
                res = arm64_shmdt(context);
                break;
            case PR_wait4:
                res = arm64_wait4(context);
                break;
            default:
                fatal("You say custom but you don't implement it %d\n", no_neutral);
        }
    } else if (how == HOW_not_yet_supported) {
        fatal("Syscall not yet implemented no=%d/no_neutral=%d\n", no, no_neutral);
    } else if (how == HOW_not_implemented) {
        res = -ENOSYS;
    } else {
        fatal("Unknown how .... %d\n", how);
    }

    skip_syscall:
    context->regs.r[0] = res;
    /* syscall exit sequence */
    ptrace_syscall_exit(context);
}
