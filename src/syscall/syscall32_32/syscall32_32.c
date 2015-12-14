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
#include <stdint.h>
#include <errno.h>

#include "syscall32_32.h"
#include "runtime.h"
#include "syscall32_32_private.h"

#define IS_NULL(px,type) ((px)?(type *)g_2_h((px)):NULL)

int syscall32_32(Sysnum no, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5)
{
    int res = -ENOSYS;

    switch(no) {
        case PR_access:
            res = syscall(SYS_access, (const char *) g_2_h(p0), (int) p1);
            break;
        case PR_close:
            res = syscall(SYS_close, (int)p0);
            break;
        case PR_read:
            res = syscall(SYS_read, (int)p0, (void *) g_2_h(p1), (size_t) p2);
            break;
        case PR_write:
            res = syscall(SYS_write, (int)p0, (void *)g_2_h(p1), (size_t)p2);
            break;
        case PR_geteuid32:
            res = syscall(SYS_geteuid);
            break;
        case PR_getuid32:
            res = syscall(SYS_getuid);
            break;
        case PR_getegid32:
            res = syscall(SYS_getegid);
            break;
        case PR_getgid32:
            res = syscall(SYS_getgid);
            break;
        /* FIXME: Handle readlink of /proc/self/exe => cf syscall64_64 */
        case PR_readlink:
            res = syscall(SYS_readlink, (const char *)g_2_h(p0), (char *)g_2_h(p1), (size_t)p2);
            break;
        case PR_fstat64:
            res = fstat64_s3232(p0,p1);
            break;
        case PR_stat64:
            res = stat64_s3232(p0,p1);
            break;
        case PR_exit_group:
            res = syscall(SYS_exit_group, (int)p0);
            break;
        case PR_lseek:
            res = syscall(SYS_lseek, (int)p0, (off_t)p1, (int)p2);
            break;
        case PR_mprotect:
            res = syscall(SYS_mprotect, (void *) g_2_h(p0), (size_t) p1, (int) p2);
            break;
        case PR_writev:
            res = writev_s3232(p0,p1,p2);
            break;
        case PR_set_tid_address:
            res = syscall(SYS_set_tid_address, (int *) g_2_h(p0));
            break;
        case PR_set_robust_list:
            /* FIXME: implement this correctly */
            res = -EPERM;
            break;
        case PR_futex:
            res = futex_s3232(p0,p1,p2,p3,p4,p5);
            break;
        case PR_rt_sigprocmask:
            res = syscall(SYS_rt_sigprocmask, (int)p0, IS_NULL(p1, const sigset_t),
                                              IS_NULL(p2, sigset_t), (size_t) p3);
            break;
        case PR_ugetrlimit:
            res = syscall(SYS_getrlimit, (unsigned int) p0, (struct rlimit *) g_2_h(p1));
            break;
        case PR_statfs64:
            /* FIXME: to be check */
            res = syscall(SYS_statfs64, (const char *)g_2_h(p0), (struct statfs *) g_2_h(p1));
            break;
        case PR_ioctl:
            /* FIXME: need specific version to offset param according to ioctl */
            res = syscall(SYS_ioctl, (int) p0, (unsigned long) p1, (char *) g_2_h(p2));
            break;
        case PR_fcntl64:
            res = fnctl64_s3232(p0,p1,p2);
            break;
        case PR_getdents64:
            res = syscall(SYS_getdents64, (int) p0, (struct linux_dirent *)g_2_h(p1), (unsigned int) p2);
            break; 
        case PR_lstat64:
            res = lstat64_s3232(p0,p1);
            break;
        case PR_lgetxattr:
            res = syscall(SYS_lgetxattr, (const char *) g_2_h(p0), (const char *) g_2_h(p1), (void *) g_2_h(p2), (size_t) p3);
            break;
        case PR_socket:
            {
                unsigned long args[3];

                args[0] = (int) p0;
                args[1] = (int) p1;
                args[2] = (int) p2;
                res = syscall(SYS_socketcall, 1 /*SYS_socket*/, args);
            }
            break;
        case PR_connect:
            {
                unsigned long args[3];

                args[0] = (int) p0;
                args[1] = (unsigned long)(const struct sockaddr *) g_2_h(p1);
                args[2] = (socklen_t) p2;
                res = syscall(SYS_socketcall, 3 /*SYS_connect*/, args);
            }
            break;
        case PR__llseek:
            res = syscall(SYS__llseek, (unsigned int) p0, (unsigned long) p1, (unsigned long) p2,
                          (loff_t *)g_2_h(p3), (unsigned int) p4);
            break;
        case PR_clock_gettime:
            res = syscall(SYS_clock_gettime, (clockid_t) p0, (struct timespec *)g_2_h(p1));
            break;
        case PR_gettimeofday:
            res = syscall(SYS_gettimeofday, IS_NULL(p0, struct timeval), IS_NULL(p1, struct timezone));
            break;
        case PR_getcwd:
            res = syscall(SYS_getcwd, (char *) g_2_h(p0), (size_t) p1);
            break;
        case PR_chdir:
            res = syscall(SYS_chdir, (const char *) g_2_h(p0));
            break;
        case PR_sysinfo:
            res = syscall(SYS_sysinfo, g_2_h(p0));
            break;
        case PR_prlimit64:
            res = syscall(SYS_prlimit64, (pid_t) p0, (int) p1, (struct rlimit *)g_2_h(p2), (struct rlimit *)g_2_h(p3));
            break;
        case PR_fchdir:
            res = syscall(SYS_fchdir, (int) p0);
            break;
        case PR_getpid:
            res = syscall(SYS_getpid);
            break;
        case PR__newselect:
            res = syscall(SYS__newselect, (int) p0, IS_NULL(p1, fd_set), IS_NULL(p2, fd_set), IS_NULL(p3, fd_set), IS_NULL(p4, struct timeval));
            break;
        case PR_umask:
            res = syscall(SYS_umask, (mode_t) p0);
            break;
        case PR_fchown32:
            res = syscall(SYS_fchown, (int) p0, (uid_t) p1, (gid_t) p2);
            break;
        case PR_unlink:
            res = syscall(SYS_unlink, (const char *) g_2_h(p0));
            break;
        case PR_rename:
            res = syscall(SYS_rename, (const char *) g_2_h(p0), (const char *) g_2_h(p1));
            break;
        case PR_fsync:
            res = syscall(SYS_fsync, (int) p0);
            break;
        case PR_kill:
            res = syscall(SYS_kill, (pid_t) p0, (int) p1);
            break;
        case PR_chmod:
            res = syscall(SYS_chmod, (const char *) g_2_h(p0), (mode_t) p1);
            break;
        case PR_getxattr:
            res = syscall(SYS_getxattr, (char *) g_2_h(p0), (char *) g_2_h(p1), (void *) g_2_h(p2), (size_t) p3);
            break;
        case PR_setxattr:
            res = syscall(SYS_setxattr, (const char *) g_2_h(p0), (const char *) g_2_h(p1), (const void *) g_2_h(p2), (size_t) p3, (int) p4);
            break;
        default:
            fatal("syscall_32_to_32: unsupported neutral syscall %d\n", no);
    }

    return res;
}
