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
#include "syscall32_32_types.h"
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
        case F_DUPFD:
            res = syscall(SYS_fcntl64, fd, cmd, (int) opt_p);
            break;
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
        case F_SETFL:
            res = syscall(SYS_fcntl64, fd, cmd, armToX86Flags(opt_p));
            break;
        case F_GETLK:
            res = syscall(SYS_fcntl64, fd, cmd, (struct flock *) g_2_h(opt_p));
            break;
        case F_SETLK:
            res = syscall(SYS_fcntl64, fd, cmd, (struct flock *) g_2_h(opt_p));
            break;
        case F_SETLKW:
            res = syscall(SYS_fcntl64, fd, cmd, (struct flock *) g_2_h(opt_p));
            break;
        case F_SETOWN:
            res = syscall(SYS_fcntl64, fd, cmd, (int) opt_p);
            break;
        case F_GETOWN:
            res = syscall(SYS_fcntl64, fd, cmd);
            break;
        case F_SETSIG:
            res = syscall(SYS_fcntl64, fd, cmd, (int) opt_p);
            break;
        case F_GETSIG:
            res = syscall(SYS_fcntl64, fd, cmd);
            break;
        case F_GETLK64:
            {
                struct flock64_32 *lock_guest = (struct flock64_32 *) g_2_h(opt_p);
                struct flock64 lock;

                if (opt_p == 0 || opt_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    lock.l_type = lock_guest->l_type;
                    lock.l_whence = lock_guest->l_whence;
                    lock.l_start = lock_guest->l_start;
                    lock.l_len = lock_guest->l_len;
                    lock.l_pid = lock_guest->l_pid;
                    res = syscall(SYS_fcntl64, fd, cmd, &lock);
                    lock_guest->l_type = lock.l_type;
                    lock_guest->l_whence = lock.l_whence;
                    lock_guest->l_start = lock.l_start;
                    lock_guest->l_len = lock.l_len;
                    lock_guest->l_pid = lock.l_pid;
                }
            }
            break;
        case F_SETLK64:
            {
                struct flock64_32 *lock_guest = (struct flock64_32 *) g_2_h(opt_p);
                struct flock64 lock;

                if (opt_p == 0 || opt_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    lock.l_type = lock_guest->l_type;
                    lock.l_whence = lock_guest->l_whence;
                    lock.l_start = lock_guest->l_start;
                    lock.l_len = lock_guest->l_len;
                    lock.l_pid = lock_guest->l_pid;
                    res = syscall(SYS_fcntl64, fd, cmd, &lock);
                }
            }
            break;
        case F_SETLKW64:
            {
                struct flock64_32 *lock_guest = (struct flock64_32 *) g_2_h(opt_p);
                struct flock64 lock;

                if (opt_p == 0 || opt_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    lock.l_type = lock_guest->l_type;
                    lock.l_whence = lock_guest->l_whence;
                    lock.l_start = lock_guest->l_start;
                    lock.l_len = lock_guest->l_len;
                    lock.l_pid = lock_guest->l_pid;
                    res = syscall(SYS_fcntl64, fd, cmd, &lock);
                }
            }
            break;
        case F_SETOWN_EX:
            res = syscall(SYS_fcntl64, fd, cmd, (struct f_owner_ex *) g_2_h(opt_p));
            break;
        case F_GETOWN_EX:
            res = syscall(SYS_fcntl64, fd, cmd, (struct f_owner_ex *) g_2_h(opt_p));
            break;
        case F_SETLEASE:
            res = syscall(SYS_fcntl64, fd, cmd, (int) opt_p);
            break;
        case F_GETLEASE:
            res = syscall(SYS_fcntl64, fd, cmd);
            break;
        case F_DUPFD_CLOEXEC:
            res = syscall(SYS_fcntl64, fd, cmd, (int) opt_p);
            break;
        case F_SETPIPE_SZ:
            res = syscall(SYS_fcntl64, fd, cmd, (int) opt_p);
            break;
        case F_GETPIPE_SZ:
            res = syscall(SYS_fcntl64, fd, cmd);
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
