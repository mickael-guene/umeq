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
 #include "syscalls_neutral_types.h"
#include "syscalls_neutral.h"
#include "runtime.h"
#include "arm_syscall.h"
#include "cache.h"
#include "umeq.h"

void arm_hlp_syscall(uint64_t regs)
{
    struct arm_target *context = container_of(int_2_ptr(regs), struct arm_target, regs);
    uint32_t no = context->regs.r[7];
    Sysnum no_neutral;
    Syshow how;
    int res = -ENOSYS;

    /* syscall entry sequence */
    ptrace_syscall_enter(context);
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
    if (how == HOW_neutral_32) {
        res = syscall_adapter_guest32(no_neutral, context->regs.r[0], context->regs.r[1], context->regs.r[2], 
                                                  context->regs.r[3], context->regs.r[4], context->regs.r[5]);
    } else if (how == HOW_custom_implementation) {
        switch(no_neutral) {
            case 0:
                res = -ENOSYS;
                break;
            case PR_ARM_cacheflush:
                cleanCaches(0, ~0);
                break;
            case PR_ARM_set_tls:
                context->regs.c13_tls2 = context->regs.r[0];
                res = 0;
                break;
            case PR_arm_fadvise64_64:
                res = syscall_adapter_guest32(PR_fadvise64_64, context->regs.r[0], context->regs.r[2], context->regs.r[3], 
                                                               context->regs.r[4], context->regs.r[5], context->regs.r[1]);
                break;
            case PR_arm_sync_file_range:
                res = syscall_adapter_guest32(PR_sync_file_range, context->regs.r[0], context->regs.r[2], context->regs.r[3],
                                                                  context->regs.r[4], context->regs.r[5], context->regs.r[1]);
                break;
            case PR_brk:
                res = arm_brk(context);
                break;
            case PR_clone:
                res = arm_clone(context);
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
            case PR_fstat:
                res = arm_fstat(context);
                break;
            case PR_fstat64:
                res = arm_fstat64(context);
                break;
            case PR_fstatat64:
                res = arm_fstatat64(context);
                break;
            case PR_ftruncate64:
                res = syscall_adapter_guest32(PR_ftruncate64, context->regs.r[0], context->regs.r[2], context->regs.r[3],
                                                              context->regs.r[3], context->regs.r[4], context->regs.r[5]);
                break;
            case PR_lstat64:
                res = arm_lstat64(context);
                break;
            case PR_mmap2:
                res = arm_mmap2(context);
                break;
            case PR_munmap:
                res = arm_munmap(context);
                break;
            case PR_mremap:
                res = arm_mremap(context);
                break;
            case PR_open:
                res = arm_open(context);
                break;
            case PR_openat:
                res = arm_openat(context);
                break;
            case PR_pread64:
                res = syscall_adapter_guest32(PR_pread64, context->regs.r[0], context->regs.r[1], context->regs.r[2],
                                                          context->regs.r[4], context->regs.r[5], 0);
                break;
            case PR_process_vm_readv:
                res = arm_process_vm_readv(context);
                break;
            case PR_ptrace:
                res = arm_ptrace(context);
                break;
            case PR_pwrite64:
                res = syscall_adapter_guest32(PR_pwrite64, context->regs.r[0], context->regs.r[1], context->regs.r[2],
                                                           context->regs.r[4], context->regs.r[5], 0);
                break;
            case PR_readahead:
                res = syscall_adapter_guest32(PR_readahead, context->regs.r[0], context->regs.r[2], context->regs.r[3],
                                                            context->regs.r[4], context->regs.r[4], context->regs.r[5]);
                break;
            case PR_rt_sigaction:
                res = arm_rt_sigaction(context);
                break;
            case PR_sigaltstack:
                res = arm_sigaltstack(context);
                break;
            case PR_shmat:
                res = arm_shmat(context);
                break;
            case PR_shmdt:
                res = arm_shmdt(context);
                break;
            case PR_rt_sigqueueinfo:
                res = arm_rt_sigqueueinfo(context);
                break;
            case PR_rt_sigreturn:
            case PR_sigreturn:
                context->isLooping = 0;
                context->exitStatus = 0;
                res = 0;
                break;
            case PR_stat:
                res = arm_stat(context);
                break;
            case PR_stat64:
                res = arm_stat64(context);
                break;
            case PR_statfs64:
                /* NOTE : arm statfs64 size if 4 bytes less due to packing. We make the strong statement that
                   code below syscall_adapter_guest32 WILL NOT write in this compiler packing byte. */
                res = syscall_adapter_guest32(PR_statfs64,  context->regs.r[0], sizeof(struct neutral_statfs64_32), context->regs.r[2], 
                                                            context->regs.r[3], context->regs.r[4], context->regs.r[5]);
                break;
            case PR_fstatfs64:
                /* NOTE : arm statfs64 size if 4 bytes less due to packing. We make the strong statement that
                   code below syscall_adapter_guest32 WILL NOT write in this compiler packing byte. */
                res = syscall_adapter_guest32(PR_fstatfs64, context->regs.r[0], sizeof(struct neutral_statfs64_32), context->regs.r[2], 
                                                            context->regs.r[3], context->regs.r[4], context->regs.r[5]);
                break;
            case PR_truncate64:
                res = syscall_adapter_guest32(PR_truncate64, context->regs.r[0], context->regs.r[2], context->regs.r[3],
                                                             context->regs.r[3], context->regs.r[4], context->regs.r[5]);
                break;
            case PR_ugetrlimit:
                res = syscall_adapter_guest32(PR_getrlimit, context->regs.r[0], context->regs.r[1], context->regs.r[2], 
                                                            context->regs.r[3], context->regs.r[4], context->regs.r[5]);
                break;
            case PR_uname:
                res = arm_uname(context);
                break;
            case PR_wait4:
                res = arm_wait4(context);
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
    ptrace_syscall_exit(context);
}
