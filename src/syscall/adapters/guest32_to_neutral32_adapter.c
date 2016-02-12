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
#include "umeq.h"

#define IS_NULL(px) ((uint32_t)((px)?g_2_h((px)):NULL))

#include <linux/futex.h>
#include <sys/time.h>
static int futex_neutral(uint32_t uaddr_p, uint32_t op_p, uint32_t val_p, uint32_t timeout_p, uint32_t uaddr2_p, uint32_t val3_p)
{
    int res;
    int *uaddr = (int *) g_2_h(uaddr_p);
    int op = (int) op_p;
    int val = (int) val_p;
    const struct timespec *timeout_guest = (struct timespec *) g_2_h(timeout_p);
    int *uaddr2 = (int *) g_2_h(uaddr2_p);
    int val3 = (int) val3_p;
    struct timespec timeout;
    int cmd = op & FUTEX_CMD_MASK;
    long syscall_timeout = (long) (timeout_p?&timeout:NULL);

    /* fixup syscall_timeout in case it's not really a timeout structure */
    if (cmd == FUTEX_REQUEUE || cmd == FUTEX_CMP_REQUEUE ||
        cmd == FUTEX_CMP_REQUEUE_PI || cmd == FUTEX_WAKE_OP) {
        syscall_timeout = timeout_p;
    } else if (cmd == FUTEX_WAKE) {
        /* in this case timeout argument is not use and can take any value */
        syscall_timeout = timeout_p;
    } else if (timeout_p) {
        timeout.tv_sec = timeout_guest->tv_sec;
        timeout.tv_nsec = timeout_guest->tv_nsec;
    }

    res = syscall_neutral_32(PR_futex, (uint32_t)uaddr, op, val, syscall_timeout, (uint32_t)uaddr2, val3);

    return res;
}

#include <fcntl.h>
static int fcnt64_neutral(uint32_t fd_p, uint32_t cmd_p, uint32_t opt_p)
{
    int res = -EINVAL;
    int fd = fd_p;
    int cmd = cmd_p;

    switch(cmd) {
        case F_DUPFD:/*0*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, opt_p, 0, 0, 0);
            break;
        case F_GETFD:/*1*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, opt_p, 0, 0, 0);
            break;
        case F_SETFD:/*2*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, opt_p, 0, 0, 0);
            break;
        case F_GETFL:/*3*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, opt_p, 0, 0, 0);
            break;
        case F_SETFL:/*4*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, opt_p, 0, 0, 0);
            break;
        case F_SETLKW:/*7*/
            res = syscall_neutral_32(PR_fcntl64, fd, cmd, (uint32_t)g_2_h(opt_p), 0, 0, 0);
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

/* FIXME: use alloca to allocate space for ptr */
/* FIXME: work only under proot */
static int execve_neutral(uint32_t filename_p,uint32_t argv_p,uint32_t envp_p)
{
    int res;
    const char *filename = (const char *) g_2_h(filename_p);
    uint32_t * argv_guest = (uint32_t *) g_2_h(argv_p);
    uint32_t * envp_guest = (uint32_t *) g_2_h(envp_p);
    char *ptr[4096];
    char **argv;
    char **envp;
    int index = 0;

    /* FIXME: do we really need to support this ? */
    /* Manual say 'On Linux, argv and envp can be specified as NULL' */
    assert(argv_p != 0);
    assert(envp_p != 0);

    argv = &ptr[index];
    while(*argv_guest != 0) {
        ptr[index++] = (char *) g_2_h(*argv_guest);
        argv_guest = (uint32_t *) ((long)argv_guest + 4);
    }
    ptr[index++] = NULL;
    envp = &ptr[index];
    /* add internal umeq environment variable if process may be ptraced */
    if (maybe_ptraced)
        ptr[index++] = "__UMEQ_INTERNAL_MAYBE_PTRACED__=1";
    while(*envp_guest != 0) {
        ptr[index++] = (char *) g_2_h(*envp_guest);
        envp_guest = (uint32_t *) ((long)envp_guest + 4);
    }
    ptr[index++] = NULL;
    /* sanity check in case we overflow => see fixme */
    if (index >= sizeof(ptr) / sizeof(char *))
        assert(0);

    res = syscall_neutral_32(PR_execve, (uint32_t)filename, (uint32_t)argv, (uint32_t)envp, 0, 0, 0);

    return res;
}

int syscall_adapter_guest32(Sysnum no, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5)
{
    int res = -ENOSYS;

    switch(no) {
        case PR__newselect:
            res = syscall_neutral_32(PR__newselect, p0, IS_NULL(p1), IS_NULL(p2), IS_NULL(p3), IS_NULL(p4), p5);
            break;
        case PR__llseek:
            res = syscall_neutral_32(PR__llseek, p0, p1, p2, (uint32_t) g_2_h(p3), p4, p5);
            break;
        case PR_access:
            res = syscall_neutral_32(PR_access, (uint32_t)g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_chdir:
            res = syscall_neutral_32(PR_chdir, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_chmod:
            res = syscall_neutral_32(PR_chmod, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_clock_gettime:
            res = syscall_neutral_32(PR_clock_gettime, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_close:
            res = syscall_neutral_32(PR_close, p0, p1, p2, p3, p4, p5);
            break;
        case PR_connect:
            res = syscall_neutral_32(PR_connect, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_dup:
            res = syscall_neutral_32(PR_dup, p0, p1, p2, p3, p4, p5);
            break;
        case PR_dup2:
            res = syscall_neutral_32(PR_dup2, p0, p1, p2, p3, p4, p5);
            break;
        case PR_execve:
            res = execve_neutral(p0, p1, p2);
            break;
        case PR_exit_group:
            res = syscall_neutral_32(PR_exit_group, p0, p1, p2, p3, p4, p5);
            break;
        case PR_faccessat:
            res = syscall_neutral_32(PR_faccessat, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_fadvise64_64:
            res = syscall_neutral_32(PR_fadvise64_64, p0, p1, p2, p3, p4, p5);
            break;
        case PR_fchdir:
            res = syscall_neutral_32(PR_fchdir, p0, p1, p2, p3, p4, p5);
            break;
        case PR_fchmod:
            res = syscall_neutral_32(PR_fchmod, p0, p1, p2, p3, p4, p5);
            break;
        case PR_fchmodat:
            res = syscall_neutral_32(PR_fchmodat, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_fchown32:
            res = syscall_neutral_32(PR_fchown32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_fcntl64:
            res = fcnt64_neutral(p0, p1, p2);
            break;
        case PR_futex:
            res = futex_neutral(p0, p1, p2, p3, p4, p5);
            break;
        case PR_fstat64:
            res = syscall_neutral_32(PR_fstat64, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_fstatat64:
            res = syscall_neutral_32(PR_fstatat64, p0, (uint32_t)g_2_h(p1), (uint32_t)g_2_h(p2), p3, p4, p5);
            break;
        case PR_fsync:
            res = syscall_neutral_32(PR_fsync, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getcwd:
            res = syscall_neutral_32(PR_getcwd, (uint32_t)g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_getdents:
            res = syscall_neutral_32(PR_getdents, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_getdents64:
            res = syscall_neutral_32(PR_getdents64, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_getegid32:
            res = syscall_neutral_32(PR_getegid32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_geteuid32:
            res = syscall_neutral_32(PR_geteuid32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getgid32:
            res = syscall_neutral_32(PR_getgid32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getrusage:
            res = syscall_neutral_32(PR_getrusage, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_getpeername:
            res = syscall_neutral_32(PR_getpeername, p0, (uint32_t)g_2_h(p1), (uint32_t)g_2_h(p2), p3, p4, p5);
            break;
        case PR_getpgrp:
            res = syscall_neutral_32(PR_getpgrp, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getpid:
            res = syscall_neutral_32(PR_getpid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getppid:
            res = syscall_neutral_32(PR_getppid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getrlimit:
            res = syscall_neutral_32(PR_getrlimit, p0, p1, p2, p3, p4, p5);
            break;
        case PR_gettid:
            res = syscall_neutral_32(PR_gettid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_gettimeofday:
            res = syscall_neutral_32(PR_gettimeofday, IS_NULL(p0), IS_NULL(p1), p2, p3, p4, p5);
            break;
        case PR_getuid32:
            res = syscall_neutral_32(PR_getuid32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getxattr:
            res = syscall_neutral_32(PR_getxattr, (uint32_t) g_2_h(p0), (uint32_t) g_2_h(p1), (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_ioctl:
            /* FIXME: need specific version to offset param according to ioctl */
            res = syscall_neutral_32(PR_ioctl, p0, p1, (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_kill:
            res = syscall_neutral_32(PR_kill, p0, p1, p2, p3, p4, p5);
            break;
        case PR_lgetxattr:
            res = syscall_neutral_32(PR_lgetxattr, (uint32_t) g_2_h(p0), (uint32_t) g_2_h(p1), (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_lseek:
            res = syscall_neutral_32(PR_lseek, p0, p1, p2, p3, p4, p5);
            break;
        case PR_lstat64:
            res = syscall_neutral_32(PR_lstat64, (uint32_t) g_2_h(p0), (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_madvise:
            res = syscall_neutral_32(PR_madvise, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_mkdir:
            res = syscall_neutral_32(PR_mkdir, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_mprotect:
            res = syscall_neutral_32(PR_mprotect, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_nanosleep:
            res = syscall_neutral_32(PR_nanosleep, (uint32_t) g_2_h(p0), (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_personality:
            res = syscall_neutral_32(PR_personality, p0, p1, p2, p3, p4, p5);
            break;
        case PR_pipe:
            res = syscall_neutral_32(PR_pipe, (uint32_t)g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_poll:
            res = syscall_neutral_32(PR_poll, (uint32_t)g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_pread64:
            res = syscall_neutral_32(PR_pread64, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_prlimit64:
            res = syscall_neutral_32(PR_prlimit64, p0, p1, IS_NULL(p2), IS_NULL(p3), p4, p5);
            break;
        case PR_read:
            res = syscall_neutral_32(PR_read, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_readlink:
            res = syscall_neutral_32(PR_readlink, (uint32_t)g_2_h(p0), (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_rename:
            res = syscall_neutral_32(PR_rename, (uint32_t)g_2_h(p0), (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_rmdir:
            res = syscall_neutral_32(PR_rmdir, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_rt_sigprocmask:
            res = syscall_neutral_32(PR_rt_sigprocmask, p0, IS_NULL(p1), IS_NULL(p2), p3, p4, p5);
            break;
        case PR_rt_sigsuspend:
            res = syscall_neutral_32(PR_rt_sigsuspend, (uint32_t)g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_tgkill:
            res = syscall_neutral_32(PR_tgkill, p0, p1, p2, p3, p4, p5);
            break;
        case PR_set_robust_list:
            /* FIXME: implement this correctly */
            res = -EPERM;
            break;
        case PR_set_tid_address:
            res = syscall_neutral_32(PR_set_tid_address, (uint32_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_setitimer:
            res = syscall_neutral_32(PR_setitimer, p0, (uint32_t) g_2_h(p1), (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_setpgid:
            res = syscall_neutral_32(PR_setpgid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_setreuid32:
            res = syscall_neutral_32(PR_setreuid32, p0, p1, p2, p3, p4, p5);
            break;
        case PR_setxattr:
            res = syscall_neutral_32(PR_setxattr, (uint32_t) g_2_h(p0), (uint32_t) g_2_h(p1), (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_socket:
            res = syscall_neutral_32(PR_socket, p0, p1, p2, p3, p4, p5);
            break;
        case PR_socketpair:
            res = syscall_neutral_32(PR_socketpair, p0, p1, p2, (uint32_t) g_2_h(p3), p4, p5);
            break;
        case PR_stat64:
            res = syscall_neutral_32(PR_stat64, (uint32_t) g_2_h(p0), (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_statfs64:
            res = syscall_neutral_32(PR_statfs64, (uint32_t) g_2_h(p0), p1, (uint32_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_sysinfo:
            res = syscall_neutral_32(PR_sysinfo, (uint32_t) g_2_h(p0), p1, p2, p3 ,p4, p5);
            break;
        case PR_times:
            res = syscall_neutral_32(PR_times, (uint32_t) g_2_h(p0), p1, p2, p3 ,p4, p5);
            break;
        case PR_umask:
            res = syscall_neutral_32(PR_umask, p0, p1, p2, p3 ,p4, p5);
            break;
        case PR_unlink:
            res = syscall_neutral_32(PR_unlink, (uint32_t) g_2_h(p0), p1, p2, p3 ,p4, p5);
            break;
        case PR_unlinkat:
            res = syscall_neutral_32(PR_unlinkat, p0, (uint32_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_utimensat:
            res = syscall_neutral_32(PR_utimensat, p0, IS_NULL(p1), IS_NULL(p2), p3, p4, p5);
            break;
        case PR_vfork:
            res = syscall_neutral_32(PR_vfork, p0, p1, p2, p3, p4, p5);
            break;
        case PR_write:
            res = syscall_neutral_32(PR_write, p0, (uint32_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        default:
            fatal("syscall_adapter_guest32: unsupported neutral syscall %d\n", no);
    }

    return res;
}
