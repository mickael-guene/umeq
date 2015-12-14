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
#include <errno.h>

#include "arm_private.h"
#include "arm_helpers.h"
#include "sysnums-arm.h"
#include "hownums-arm.h"
#include "syscall32_32.h"
#include "runtime.h"
#include "arm_syscall.h"
#include "cache.h"

void arm_hlp_syscall(uint64_t regs)
{
    struct arm_target *context = container_of((void *) regs, struct arm_target, regs);
    uint32_t no = context->regs.r[7];
    Sysnum no_neutral;
    Syshow how;
    int res = -ENOSYS;

    /* syscall entry sequence */
    context->regs.is_in_syscall = 1;
    //syscall((long) 313, 0);
    /* translate syscall nb into neutral no */
    if (no == 0xf0005)
        no_neutral = PR_ARM_set_tls;
    else if (no == 0xf0002)
        no_neutral = PR_ARM_cacheflush;
    else if (no >= sysnums_arm_nb)
        fatal("Out of range syscall number %d >= %d\n", no, sysnums_arm_nb);
    else
        no_neutral = sysnums_arm[context->regs.r[7]];
    how = syshow_arm[no_neutral];
    /* so how we handle this sycall */
    if (how == HOW_32_to_32) {
        res = syscall32_32(no_neutral, context->regs.r[0], context->regs.r[1], context->regs.r[2], 
                                            context->regs.r[3], context->regs.r[4], context->regs.r[5]);
    } else if (how == HOW_custom_implementation) {
        switch(no_neutral) {
            case PR_mmap2:
                res = arm_mmap2(context);
                break;
            case PR_munmap:
                res = arm_munmap(context);
                break;
            case PR_open:
                res = arm_open(context);
                break;
            case PR_uname:
                res = arm_uname(context);
                break;
            case PR_brk:
                res = arm_brk(context);
                break;
            case PR_ARM_set_tls:
                context->regs.c13_tls2 = context->regs.r[0];
                res = 0;
                break;
            case PR_rt_sigaction:
                res = arm_rt_sigaction(context);
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
            case PR_arm_fadvise64_64:
                res = syscall(SYS_fadvise64, (int) context->regs.r[0],
                                             (off_t) (((uint64_t)context->regs.r[3] << 32) + (uint64_t)context->regs.r[2]),
                                             (off_t) (((uint64_t)context->regs.r[5] << 32) + (uint64_t)context->regs.r[4]),
                                             (int) context->regs.r[1]);
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

    context->regs.r[0] = res;
    /* syscall exit sequence */
    context->regs.is_in_syscall = 2;
    //syscall((long) 313, 1);
    /* no more in syscall */
    context->regs.is_in_syscall = 0;
}
