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
#include "target64.h"
#include "syscall_x86_64.h"

long syscall_neutral_64(Sysnum no, uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5)
{
    int res = -ENOSYS;

    switch(no) {
        case PR_exit_group:
            res = syscall(SYS_exit_group, p0);
            break;
        case PR_fstat64:
            res = syscall_x86_64_fstat64(p0, p1);
            break;
        case PR_gettid:
            res = syscall(SYS_gettid);
            break;
        case PR_readlink:
            res = syscall(SYS_readlink, p0, p1, p2);
            break;
        case PR_rt_sigprocmask:
            res = syscall(SYS_rt_sigprocmask, p0, p1, p2, p3);
            break;
        case PR_tgkill:
            res = syscall(SYS_tgkill, p0, p1, p2);
            break;
        case PR_write:
            res = syscall(SYS_write, p0, p1, p2);
            break;
        default:
            fatal("syscall_neutral_64: unsupported neutral syscall %d\n", no);
    }

    return res;
}
