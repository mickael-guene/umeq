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
#include <fcntl.h>

#include "syscalls_neutral.h"
#include "runtime.h"
#include "target64.h"
#include "syscall_x86_64.h"

struct convertFlags {
    long neutralFlag;
    long x86Flag;
};

const static struct convertFlags neutralToX86FlagsTable[] = {
        {01,O_WRONLY},
        {02,O_RDWR},
        {0100,O_CREAT},
        {0200,O_EXCL},
        {0400,O_NOCTTY},
        {01000,O_TRUNC},
        {02000,O_APPEND},
        {04000,O_NONBLOCK},
        {04010000,O_SYNC},
        {020000,O_ASYNC},
        {040000,O_DIRECTORY},
        {0100000,O_NOFOLLOW},
        {02000000,O_CLOEXEC},
        {0200000,O_DIRECT},
        {01000000,O_NOATIME},
        //{010000000,O_PATH}, //Not on my machine
        {0400000,O_LARGEFILE},
};

long neutralToX86Flags(long neutral_flags)
{
    long res = 0;
    int i;

    for(i=0;i<sizeof(neutralToX86FlagsTable) / sizeof(struct convertFlags); i++) {
        if (neutral_flags & neutralToX86FlagsTable[i].neutralFlag)
            res |= neutralToX86FlagsTable[i].x86Flag;
    }

    return res;
}

long x86ToNeutralFlags(long x86_flags)
{
    long res = 0;
    int i;

    for(i=0;i<sizeof(neutralToX86FlagsTable) / sizeof(struct convertFlags); i++) {
        if (x86_flags & neutralToX86FlagsTable[i].x86Flag)
            res |= neutralToX86FlagsTable[i].neutralFlag;
    }

    return res;
}

long fcntl_neutral(int fd, int cmd, uint64_t opt_p)
{
    long res = -EINVAL;

    switch(cmd) {
        case F_GETFL:
            res = syscall(SYS_fcntl, fd, cmd);
            res = x86ToNeutralFlags(res);
            break;
        case F_SETFL:
            res = syscall(SYS_fcntl, fd, cmd, neutralToX86Flags(opt_p));
            break;
        /* following are neutral approved !!!! */
        case F_DUPFD:/*0*/
        case F_GETFD:/*1*/
        case F_SETFD:/*2*/
        case F_SETLKW:/*7*/
            res = syscall(SYS_fcntl, fd,cmd, opt_p);
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

long syscall_neutral_64(Sysnum no, uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5)
{
    int res = -ENOSYS;

    switch(no) {
        case PR_access:
            res = syscall(SYS_access, p0, p1);
            break;
        case PR_chdir:
            res = syscall(SYS_chdir, p0);
            break;
        case PR_chmod:
            res = syscall(SYS_chmod, p0, p1);
            break;
        case PR_clock_gettime:
            res = syscall(SYS_clock_gettime, p0, p1);
            break;
        case PR_close:
            res = syscall(SYS_close, p0);
            break;
        case PR_connect:
            res = syscall(SYS_connect, p0, p1, p2);
            break;
        case PR_dup:
            res = syscall(SYS_dup, p0);
            break;
        case PR_dup2:
            res = syscall(SYS_dup2, p0, p1);
            break;
        case PR_execve:
            res = syscall(SYS_execve, p0, p1, p2);
            break;
        case PR_exit_group:
            res = syscall(SYS_exit_group, p0);
            break;
        case PR_faccessat:
            res = syscall(SYS_faccessat, p0, p1, p2);
            break;
        case PR_fadvise64:
            res = syscall(SYS_fadvise64, p0, p1, p2, p3);
            break;
        case PR_fchdir:
            res = syscall(SYS_fchdir, p0);
            break;
        case PR_fchown:
            res = syscall(SYS_fchown, p0, p1, p2);
            break;
        case PR_fcntl:
            res = fcntl_neutral(p0, p1, p2);
            break;
        case PR_futex:
            res = syscall(SYS_futex, p0, p1, p2, p3, p4, p5);
            break;
        case PR_fstat64:
            res = syscall_x86_64_fstat64(p0, p1);
            break;
        case PR_fstatat64:
            res = syscall_x86_64_fstatat64(p0, p1, p2, p3);
            break;
        case PR_fsync:
            res = syscall(SYS_fsync, p0);
            break;
        case PR_getcwd:
            res = syscall(SYS_getcwd, p0, p1);
            break;
        case PR_getdents:
            res = syscall(SYS_getdents, p0, p1, p2);
            break;
        case PR_getdents64:
            res = syscall(SYS_getdents64, p0, p1, p2);
            break;
        case PR_getegid:
            res = syscall(SYS_getegid);
            break;
        case PR_geteuid:
            res = syscall(SYS_geteuid);
            break;
        case PR_getgid:
            res = syscall(SYS_getgid);
            break;
        case PR_getpid:
            res = syscall(SYS_getpid);
            break;
        case PR_getpgrp:
            res = syscall(SYS_getpgrp);
            break;
        case PR_getppid:
            res = syscall(SYS_getppid);
            break;
        case PR_getrlimit:
            res = syscall(SYS_getrlimit, p0, p1);
            break;
        case PR_getrusage:
            res = syscall(SYS_getrusage, p0, p1);
            break;
        case PR_getpeername:
            res = syscall(SYS_getpeername, p0, p1, p2);
            break;
        case PR_gettid:
            res = syscall(SYS_gettid);
            break;
        case PR_gettimeofday:
            res = syscall(SYS_gettimeofday, p0, p1);
            break;
        case PR_getuid:
            res = syscall(SYS_getuid);
            break;
        case PR_getxattr:
            res = syscall(SYS_getxattr, p0, p1, p2, p3);
            break;
        case PR_ioctl:
            res = syscall(SYS_ioctl, p0, p1, p2, p3, p4, p5);
            break;
        case PR_kill:
            res = syscall(SYS_kill, p0, p1);
            break;
        case PR_lgetxattr:
            res = syscall(SYS_lgetxattr, p0, p1, p2, p3);
            break;
        case PR_lseek:
            res = syscall(SYS_lseek, p0, p1, p2);
            break;
        case PR_lstat64:
            res = syscall_x86_64_lstat64(p0, p1);
            break;
        case PR_madvise:
            res = syscall(SYS_madvise, p0, p1, p2);
            break;
        case PR_mprotect:
            res = syscall(SYS_mprotect, p0, p1, p2);
            break;
        case PR_nanosleep:
            res = syscall(SYS_nanosleep, p0, p1);
            break;
        case PR_personality:
            res = syscall(SYS_personality, p0);
            break;
        case PR_pipe:
            res = syscall(SYS_pipe, p0);
            break;
        case PR_poll:
            res = syscall(SYS_poll, p0, p1, p2);
            break;
        case PR_pread64:
            res = syscall(SYS_pread64, p0, p1, p2, p3);
            break;
        case PR_prlimit64:
            res = syscall(SYS_prlimit64, p0, p1, p2, p3);
            break;
        case PR_read:
            res = syscall(SYS_read, p0, p1, p2);
            break;
        case PR_readlink:
            res = syscall(SYS_readlink, p0, p1, p2);
            break;
        case PR_rename:
            res = syscall(SYS_rename, p0, p1);
            break;
        case PR_rt_sigprocmask:
            res = syscall(SYS_rt_sigprocmask, p0, p1, p2, p3);
            break;
        case PR_rt_sigsuspend:
            res = syscall(SYS_rt_sigsuspend, p0, p1);
            break;
        case PR_tgkill:
            res = syscall(SYS_tgkill, p0, p1, p2);
            break;
        case PR_select:
            res = syscall(SYS_select, p0, p1, p2, p3, p4);
            break;
        case PR_set_tid_address:
            res = syscall(SYS_set_tid_address, p0);
            break;
        case PR_setitimer:
            res = syscall(SYS_setitimer, p0, p1, p2);
            break;
        case PR_setpgid:
            res = syscall(SYS_setpgid, p0, p1);
            break;
        case PR_setreuid:
            res = syscall(SYS_setreuid, p0, p1);
            break;
        case PR_setxattr:
            res = syscall(SYS_setxattr, p0, p1, p2, p3, p4);
            break;
        case PR_socket:
            res = syscall(SYS_socket, p0, p1, p2);
            break;
        case PR_socketpair:
            res = syscall(SYS_socketpair, p0, p1, p2, p3);
            break;
        case PR_stat64:
            res = syscall_x86_64_stat64(p0, p1);
            break;
        case PR_statfs:
            res = syscall(SYS_statfs, p0, p1);
            break;
        case PR_sysinfo:
            res = syscall(SYS_sysinfo, p0);
            break;
        case PR_umask:
            res = syscall(SYS_umask, p0);
            break;
        case PR_unlink:
            res = syscall(SYS_unlink, p0);
            break;
        case PR_unlinkat:
            res = syscall(SYS_unlinkat, p0, p1, p2);
            break;
        case PR_utimensat:
            res = syscall(SYS_utimensat, p0, p1, p2, p3);
            break;
        case PR_vfork:
            /* implement with fork to avoid sync problem but semantic is not fully preserved ... */
            res = syscall(SYS_fork);
            break;
        case PR_write:
            res = syscall(SYS_write, p0, p1, p2);
            break;
        default:
            fatal("syscall_neutral_64: unsupported neutral syscall %d\n", no);
    }

    return res;
}
