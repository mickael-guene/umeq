/* This file is part of Umeq, an equivalent of qemu user mode emulation with improved robustness.
 *
 * Copyright (C) 2016 STMicroelectronics
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
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <errno.h>
#include <stdint.h>

#include "syscalls_neutral.h"
#include "runtime.h"
#include "target32.h"

#define IS_NULL(px,type) ((px)?(type *)g_2_h((px)):NULL)

int syscall_adapter_guest32(Sysnum no, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5)
{
    int res = -ENOSYS;

    switch(no) {
        case PR_exit_group:
            res = syscall_neutral_32(PR_exit_group, p0, p1, p2, p3, p4, p5);
            break;
        case PR_fstat64:
            res = syscall_neutral_32(PR_fstat64, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_geteuid32:
            res = syscall_neutral_32(PR_geteuid32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_gettid:
            res = syscall_neutral_32(PR_gettid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_readlink:
            res = syscall_neutral_32(PR_readlink, (uint32_t)g_2_h(p0), (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_rt_sigprocmask:
            res = syscall_neutral_32(PR_rt_sigprocmask, p0, (uint32_t) IS_NULL(p1, const sigset_t),
                                     (uint32_t) IS_NULL(p2, sigset_t), p3, p4, p5);
            break;
        case PR_tgkill:
            res = syscall_neutral_32(PR_tgkill, p0, p1, p2, p3, p4, p5);
            break;
        case PR_write:
            res = syscall_neutral_32(PR_write, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        default:
            fatal("syscall_adapter_guest32: unsupported neutral syscall %d\n", no);
    }

    return res;
}
