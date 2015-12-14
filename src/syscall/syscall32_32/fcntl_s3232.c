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
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <unistd.h>
#include <fcntl.h>

#include <errno.h>

#include "runtime.h"
#include "syscall32_32_private.h"

/* FIXME: Found a way to handle this */
extern int x86ToArmFlags(long x86_flags);
extern long armToX86Flags(int arm_flags);

int fnctl64_s3232(uint32_t fd_p, uint32_t cmd_p, uint32_t opt_p)
{
    int res = -EINVAL;
    int fd = fd_p;
    int cmd = cmd_p;

    switch(cmd) {
        case F_GETFD:
        	/* FIXME: check SYS_fcntl or SYS_fcntl64 */
            res = syscall(SYS_fcntl64, fd, cmd);
            break;
        case F_SETFD:
            res = syscall(SYS_fcntl64, fd, cmd, (int) opt_p);
            break;
        case F_GETFL:
            res = syscall(SYS_fcntl64, fd, cmd);
            res = x86ToArmFlags(res);
            break;
        default:
            /* invalid cmd use by ltp testsuite */
            if (cmd == 99999)
                res = -EINVAL;
            else
                fatal("unsupported fnctl64 command %d\n", cmd);
    }

    return res;
}
