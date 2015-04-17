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

#include <unistd.h>
#include <fcntl.h>

#include <errno.h>

#include "runtime.h"
#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

#ifndef F_SETSIG
# define F_SETSIG   10  /* Set number of signal to be sent.  */
#endif
#ifndef F_GETSIG
# define F_GETSIG   11  /* Get number of signal to be sent.  */
#endif
#ifndef F_SETLEASE
# define F_SETLEASE     1024    /* Set a lease.  */
#endif
#ifndef F_GETLEASE
# define F_GETLEASE 1025    /* Enquire what lease is active.  */
#endif
#ifndef F_SETPIPE_SZ
# define F_SETPIPE_SZ   1031    /* Set pipe page size array.  */
#endif
#ifndef F_GETPIPE_SZ
# define F_GETPIPE_SZ   1032    /* Set pipe page size array.  */
#endif
#ifndef F_GETOWN_EX
# define F_GETOWN_EX    16      /* Set owner (thread receiving SIGIO).  */
#endif


/* FIXME: Found a way to handle this */
extern int x86ToArmFlags(long x86_flags);
extern long armToX86Flags(int arm_flags);

int fnctl64_s3264(uint32_t fd_p, uint32_t cmd_p, uint32_t opt_p)
{
    int res = -EINVAL;
    int fd = fd_p;
    int cmd = cmd_p;

    switch(cmd) {
        case F_DUPFD:
            res = syscall(SYS_fcntl, fd, cmd, (int) opt_p);
            break;
        case F_GETFD:
            res = syscall(SYS_fcntl, fd, cmd);
            break;
        case F_SETFD:
            res = syscall(SYS_fcntl, fd, cmd, (int) opt_p);
            break;
        case F_GETFL:
            res = syscall(SYS_fcntl, fd, cmd);
            res = x86ToArmFlags(res);
            break;
        case F_SETFL:
            res = syscall(SYS_fcntl, fd, cmd, armToX86Flags(opt_p));
            break;
        case F_SETOWN:
            res = syscall(SYS_fcntl, fd, cmd, (int) opt_p);
            break;
        case F_GETOWN:
            res = syscall(SYS_fcntl, fd, cmd);
            break;
        case F_SETSIG:
            res = syscall(SYS_fcntl, fd, cmd, (int) opt_p);
            break;
        case F_GETSIG:
            res = syscall(SYS_fcntl, fd, cmd);
            break;
        case 12://F_GETLK64:
            {
                struct flock64_32 *lock_guest = (struct flock64_32 *) g_2_h(opt_p);
                struct flock lock;

                if (opt_p == 0 || opt_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    lock.l_type = lock_guest->l_type;
                    lock.l_whence = lock_guest->l_whence;
                    lock.l_start = lock_guest->l_start;
                    lock.l_len = lock_guest->l_len;
                    lock.l_pid = lock_guest->l_pid;
                    res = syscall(SYS_fcntl, fd, F_GETLK, &lock);
                    lock_guest->l_type = lock.l_type;
                    lock_guest->l_whence = lock.l_whence;
                    lock_guest->l_start = lock.l_start;
                    lock_guest->l_len = lock.l_len;
                    lock_guest->l_pid = lock.l_pid;
                }
            }
            break;
        case 13://F_SETLK64
            {
                struct flock64_32 *lock_guest = (struct flock64_32 *) g_2_h(opt_p);
                struct flock lock;

                if (opt_p == 0 || opt_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    lock.l_type = lock_guest->l_type;
                    lock.l_whence = lock_guest->l_whence;
                    lock.l_start = lock_guest->l_start;
                    lock.l_len = lock_guest->l_len;
                    lock.l_pid = lock_guest->l_pid;
                    res = syscall(SYS_fcntl, fd, F_SETLK, &lock);
                }
            }
            break;
        case 14://F_SETLKW64
            {
                struct flock64_32 *lock_guest = (struct flock64_32 *) g_2_h(opt_p);
                struct flock lock;

                if (opt_p == 0 || opt_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    lock.l_type = lock_guest->l_type;
                    lock.l_whence = lock_guest->l_whence;
                    lock.l_start = lock_guest->l_start;
                    lock.l_len = lock_guest->l_len;
                    lock.l_pid = lock_guest->l_pid;
                    res = syscall(SYS_fcntl, fd, F_SETLKW, &lock);
                }
            }
            break;
        case F_GETLK:
            {
                struct flock_32 *lock_guest = (struct flock_32 *) g_2_h(opt_p);
                struct flock lock;

                if (opt_p == 0 || opt_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    lock.l_type = lock_guest->l_type;
                    lock.l_whence = lock_guest->l_whence;
                    lock.l_start = lock_guest->l_start;
                    lock.l_len = lock_guest->l_len;
                    lock.l_pid = lock_guest->l_pid;
                    res = syscall(SYS_fcntl, fd, F_GETLK, &lock);
                    lock_guest->l_type = lock.l_type;
                    lock_guest->l_whence = lock.l_whence;
                    lock_guest->l_start = lock.l_start;
                    lock_guest->l_len = lock.l_len;
                    lock_guest->l_pid = lock.l_pid;
                }
            }
            break;
        case F_SETLK:
            /* Fallthrough */
        case F_SETLKW:
            {
                struct flock_32 *lock_guest = (struct flock_32 *) g_2_h(opt_p);
                struct flock lock;

                if (opt_p == 0 || opt_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    lock.l_type = lock_guest->l_type;
                    lock.l_whence = lock_guest->l_whence;
                    lock.l_start = lock_guest->l_start;
                    lock.l_len = lock_guest->l_len;
                    lock.l_pid = lock_guest->l_pid;
                    res = syscall(SYS_fcntl, fd, cmd, &lock);
                }
            }
            break;
        case F_GETOWN_EX:
            res = syscall(SYS_fcntl, fd, cmd, (struct f_owner_ex *) g_2_h(opt_p));
            break;
        case F_SETLEASE:
            res = syscall(SYS_fcntl, fd, cmd, (int) opt_p);
            break;
        case F_GETLEASE:
            res = syscall(SYS_fcntl, fd, cmd);
            break;
        case F_DUPFD_CLOEXEC:
            res = syscall(SYS_fcntl, fd, cmd, (int) opt_p);
            break;
        case F_SETPIPE_SZ:
            res = syscall(SYS_fcntl, fd, cmd, (int) opt_p);
            break;
        case F_GETPIPE_SZ:
            res = syscall(SYS_fcntl, fd, cmd);
            break;
        default:
            fatal("unsupported fnctl64 command %d\n", cmd);
    }

    return res;
}

int fnctl_s3264(uint32_t fd_p, uint32_t cmd_p, uint32_t opt_p)
{
    int res = -EINVAL;
    int fd = fd_p;
    int cmd = cmd_p;

    switch(cmd) {
        case F_SETFD:
            res = syscall(SYS_fcntl, fd, cmd, (int) opt_p);
            break;
        default:
            fatal("unsupported fnctl command %d\n", cmd);
    }

    return res;
}
