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
#include "syscalls_neutral_types.h"
#include "runtime.h"
#include "target64.h"
#include "umeq.h"
#include "adapters_private.h"

#define IS_NULL(px) ((uint64_t)((px)?g_2_h((px)):NULL))

static long execve_neutral(uint64_t filename_p, uint64_t argv_p, uint64_t envp_p)
{
    long res;
    const char *filename = (const char *) g_2_h(filename_p);
    uint64_t * argv_guest = (uint64_t *) g_2_h(argv_p);
    uint64_t * envp_guest = (uint64_t *) g_2_h(envp_p);
    char *ptr[8192];
    char **argv;
    char **envp;
    int index = 0;

    argv = &ptr[index];
    /* handle -execve mode */
    /* code adapted from https://github.com/resin-io/qemu/commit/6b9e5be0fbc07ae3d6525bbd57c60da58d33b840 and
       https://git.kernel.org/cgit/linux/kernel/git/torvalds/linux.git/tree/fs/binfmt_script.c */
    if (is_umeq_call_in_execve) {
        char *i_arg = NULL;
        char *i_name = NULL;

        /* test for shebang and file exists */
        res = parse_shebang(filename, &i_name, &i_arg);
        if (res)
            return res;

        /* insert umeq */
        ptr[index++] = umeq_filename;
        ptr[index++] = "-execve";
        ptr[index++] = "-0";
        if (i_name) {
            ptr[index++] = i_name;
            ptr[index++] = i_name;
            if (i_arg)
                ptr[index++] = i_arg;
        } else {
            ptr[index++] = (char *) g_2_h(*argv_guest);
        }
        argv_guest = (uint64_t *) ((long)argv_guest + 8);
        ptr[index++] = (char *) filename;
    }

    /* Manual say 'On Linux, argv and envp can be specified as NULL' */
    /* In that case we give array of length 1 with only one null element */
    if (argv_p) {
        while(*argv_guest != 0) {
            ptr[index++] = (char *) g_2_h(*argv_guest);
            argv_guest = (uint64_t *) ((long)argv_guest + 8);
        }
    }
    ptr[index++] = NULL;
    envp = &ptr[index];
    /* add internal umeq environment variable if process may be ptraced */
    if (maybe_ptraced)
        ptr[index++] = "__UMEQ_INTERNAL_MAYBE_PTRACED__=1";
    if (envp_p) {
        while(*envp_guest != 0) {
            ptr[index++] = (char *) g_2_h(*envp_guest);
            envp_guest = (uint64_t *) ((long)envp_guest + 8);
        }
    }
    ptr[index++] = NULL;
    /* sanity check in case we overflow => see fixme */
    if (index >= sizeof(ptr) / sizeof(char *))
        assert(0);

    res = syscall_neutral_64(PR_execve, (uint64_t) (is_umeq_call_in_execve?umeq_filename:filename), (uint64_t) argv,  (uint64_t) envp, 0, 0, 0);

    return res;
}

static long fcntl_neutral(uint64_t fd_p, uint64_t cmd_p, uint64_t opt_p)
{
    long res = -EINVAL;
    int fd = fd_p;
    int cmd = cmd_p;

    switch(cmd) {
        case NEUTRAL_F_DUPFD:/*0*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, (int) opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_GETFD:/*1*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, 0, 0, 0, 0);
            break;
        case NEUTRAL_F_SETFD:/*2*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, (int) opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_GETFL:/*3*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_SETFL:/*4*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_GETLK:/*5*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, (uint64_t) g_2_h(opt_p), 0, 0, 0);
            break;
        case NEUTRAL_F_SETLK:/*6*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, (uint64_t) g_2_h(opt_p), 0, 0, 0);
            break;
        case NEUTRAL_F_SETLKW:/*7*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, (uint64_t) g_2_h(opt_p), 0, 0, 0);
            break;
        case NEUTRAL_F_SETOWN:/*8*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, (int) opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_SETSIG:/*10*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, (int) opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_GETSIG:/*11*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, 0, 0, 0, 0);
            break;
        case NEUTRAL_F_SETOWN_EX:/*15*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, (uint64_t) g_2_h(opt_p), 0, 0, 0);
            break;
        case NEUTRAL_F_GETOWN_EX:/*16*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, (uint64_t) g_2_h(opt_p), 0, 0, 0);
            break;
        case NEUTRAL_F_SETLEASE:/*1024*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, (int) opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_GETLEASE:/*1025*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, 0, 0, 0, 0);
            break;
        case NEUTRAL_F_DUPFD_CLOEXEC:/*1030*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, (int) opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_SETPIPE_SZ:/*1031*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, (int) opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_GETPIPE_SZ:/*1032*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, 0, 0, 0, 0);
            break;
        default:
            /* invalid cmd use by ltp testsuite */
            if (cmd == 99999)
                res = -EINVAL;
            else
                fatal("unsupported fnctl command %d\n", cmd);
    }

    return res;
}

static long futex_neutral(uint64_t uaddr_p, uint64_t op_p, uint64_t val_p, uint64_t timeout_p, uint64_t uaddr2_p, uint64_t val3_p)
{
    long res;
    int *uaddr = (int *) g_2_h(uaddr_p);
    int op = (int) op_p;
    int val = (int) val_p;
    int *uaddr2 = (int *) g_2_h(uaddr2_p);
    int val3 = (int) val3_p;
    int cmd = op & NEUTRAL_FUTEX_CMD_MASK;
    /* default value is a timout parameter */
    uint64_t syscall_timeout = (uint64_t) (timeout_p?g_2_h(timeout_p):NULL);

    /* fixup syscall_timeout in case it's not really a timeout structure */
    if (cmd == NEUTRAL_FUTEX_REQUEUE || cmd == NEUTRAL_FUTEX_CMP_REQUEUE ||
        cmd == NEUTRAL_FUTEX_CMP_REQUEUE_PI || cmd == NEUTRAL_FUTEX_WAKE_OP) {
        syscall_timeout = timeout_p;
    } else if (cmd == NEUTRAL_FUTEX_WAKE) {
        /* in this case timeout argument is not use and can take any value */
        syscall_timeout = timeout_p;
    }

    res = syscall_neutral_64(PR_futex, (uint64_t) uaddr, op, val, syscall_timeout, (uint64_t) uaddr2, val3);

    return res;
}

struct data_64_internal {
    const sigset_t *ss;
    size_t ss_len;
};

static long pselect6_neutral(uint64_t nfds_p, uint64_t readfds_p, uint64_t writefds_p,
                             uint64_t exceptfds_p, uint64_t timeout_p, uint64_t data_p)
{
    long res;
    int nfds = (int) nfds_p;
    neutral_fd_set *readfds = (neutral_fd_set *) g_2_h(readfds_p);
    neutral_fd_set *writefds = (neutral_fd_set *) g_2_h(writefds_p);
    neutral_fd_set *exceptfds = (neutral_fd_set *) g_2_h(exceptfds_p);
    struct neutral_timespec_64 *timeout = (struct neutral_timespec_64 *) g_2_h(timeout_p);
    struct data_64_internal *data_guest = (struct data_64_internal *) g_2_h(data_p);
    struct data_64_internal data;

    if (data_p) {
        data.ss = (const sigset_t *) data_guest->ss?g_2_h((uint64_t)data_guest->ss):NULL;
        data.ss_len = data_guest->ss_len;
    }

    res = syscall_neutral_64(PR_pselect6, nfds, (uint64_t) (readfds_p?readfds:NULL), (uint64_t) (writefds_p?writefds:NULL),
                             (uint64_t) (exceptfds_p?exceptfds:NULL), (uint64_t) (timeout_p?timeout:NULL), (uint64_t) (data_p?&data:NULL));

    return res;
}

static long semctl_neutral(uint64_t semid_p, uint64_t semnum_p, uint64_t cmd_p, uint64_t arg_p)
{
    void *arg = ((cmd_p & ~0x100) == NEUTRAL_SETVAL) ? (void *)arg_p : g_2_h(arg_p);

    /* improve robustness */
    if (cmd_p == NEUTRAL_IPC_SET || cmd_p == NEUTRAL_SEM_STAT || cmd_p == NEUTRAL_IPC_STAT)
        if (arg_p == 0 || arg_p == 0xffffffff)
            return -EFAULT;

    return syscall_neutral_64(PR_semctl, semid_p, semnum_p, cmd_p, (uint64_t) arg, 0, 0);
}

long syscall_adapter_guest64(Sysnum no, uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5)
{
    long res = -ENOSYS;

    switch(no) {
        case PR_accept:
            res = syscall_neutral_64(PR_accept, (int) p0, IS_NULL(p1), IS_NULL(p2), p3, p4, p5);
            break;
        case PR_accept4:
            res = syscall_neutral_64(PR_accept4, (int) p0, IS_NULL(p1), IS_NULL(p2), (int) p3, p4, p5);
            break;
        case PR_add_key:
            res = syscall_neutral_64(PR_add_key, (uint64_t) g_2_h(p0), IS_NULL(p1), IS_NULL(p2), (size_t) p3, p4, p5);
            break;
        case PR_bind:
            res= syscall_neutral_64(PR_bind, (int) p0, (uint64_t) g_2_h(p1), (socklen_t) p2, p3, p4, p5);
            break;
        case PR_capget:
            res = syscall_neutral_64(PR_capget, (uint64_t) g_2_h(p0), IS_NULL(p1), p2, p3, p4, p5);
            break;
        case PR_capset:
            res = syscall_neutral_64(PR_capset, (uint64_t) g_2_h(p0), IS_NULL(p1), p2, p3, p4, p5);
            break;
        case PR_close:
            res = syscall_neutral_64(PR_close, (int)p0, p1, p2, p3, p4, p5);
            break;
        case PR_chdir:
            res = syscall_neutral_64(PR_chdir, (uint64_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_chroot:
            res = syscall_neutral_64(PR_chroot, (uint64_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_clock_getres:
            res = syscall_neutral_64(PR_clock_getres, (clockid_t) p0, IS_NULL(p1), p2, p3, p4, p5);
            break;
        case PR_clock_gettime:
            res = syscall_neutral_64(PR_clock_gettime, (clockid_t) p0, (uint64_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_clock_nanosleep:
            res = syscall_neutral_64(PR_clock_nanosleep, (clockid_t) p0, (int) p1, (uint64_t) g_2_h(p2),
                                                         IS_NULL(p3), p4, p5);
            break;
        case PR_connect:
            res = syscall_neutral_64(PR_connect, (int)p0, (uint64_t)g_2_h(p1), (socklen_t)p2, p3, p4, p5);
            break;
        case PR_dup:
            res = syscall_neutral_64(PR_dup, (int)p0, p1, p2, p3, p4, p5);
            break;
        case PR_dup2:
            res = syscall_neutral_64(PR_dup2, (int)p0, (int)p1, p2, p3, p4, p5);
            break;
        case PR_dup3:
            res = syscall_neutral_64(PR_dup3, (int)p0, (int)p1, (int)p2, p3, p4, p5);
            break;
        case PR_epoll_create1:
            res = syscall_neutral_64(PR_epoll_create1, (int) p0, p1, p2, p3, p4, p5);
            break;
        case PR_epoll_ctl:
            res = syscall_neutral_64(PR_epoll_ctl, p0, p1, p2, (uint64_t) IS_NULL(p3), p4, p5);
            break;
        case PR_epoll_pwait:
            res = syscall_neutral_64(PR_epoll_pwait, (int) p0, (uint64_t) g_2_h(p1), (int) p2, (int) p3, IS_NULL(p4), (int) p5);
            break;
        case PR_eventfd2:
            res = syscall_neutral_64(PR_eventfd2, (unsigned int) p0, (int) p1, p2, p3, p4, p5);
            break;
        case PR_execve:
            res = execve_neutral(p0,p1,p2);
            break;
        case PR_exit_group:
            res = syscall_neutral_64(PR_exit_group, (int)p0, p1, p2, p3, p4, p5);
            break;
        case PR_faccessat:
            res = syscall_neutral_64(PR_faccessat, (int)p0, (uint64_t)g_2_h(p1), (int)p2, p3, p4, p5);
            break;
        case PR_fadvise64:
            res = syscall_neutral_64(PR_fadvise64, (int) p0, (off_t) p1, (off_t) p2, (int) p3, p4, p5);
            break;
        case PR_fallocate:
            res = syscall_neutral_64(PR_fallocate, (int) p0, (int) p1, (off_t) p2, (off_t) p3, p4, p5);
            break;
        case PR_fanotify_init:
            res = syscall_neutral_64(PR_fanotify_init, (unsigned int) p0, (unsigned int) p1, p2, p3, p4, p5);
            break;
        case PR_fchdir:
            res = syscall_neutral_64(PR_fchdir, (int)p0, p1, p2, p3, p4, p5);
            break;
        case PR_fchmod:
            res = syscall_neutral_64(PR_fchmod, (unsigned int)p0, p1, p2, p3, p4, p5);
            break;
        case PR_fchmodat:
            res = syscall_neutral_64(PR_fchmodat, (int)p0, (uint64_t) g_2_h(p1), (mode_t)p2, p3, p4, p5);
            break;
        case PR_fchown:
            res = syscall_neutral_64(PR_fchown, (unsigned int)p0, (uid_t)p1, (gid_t)p2, p3, p4, p5);
            break;
        case PR_fchownat:
            res = syscall_neutral_64(PR_fchownat, (int) p0, (uint64_t) g_2_h(p1), (uid_t) p2, (gid_t) p3, (int) p4, p5);
            break;
        case PR_fcntl:
            res = fcntl_neutral(p0,p1,p2);
            break;
        case PR_fdatasync:
            res = syscall_neutral_64(PR_fdatasync, (unsigned int)p0, p1, p2, p3, p4, p5);
            break;
        case PR_fgetxattr:
            res = syscall_neutral_64(PR_fgetxattr, (int) p0, (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), (size_t) p3, p4, p5);
            break;
        case PR_flistxattr:
            res = syscall_neutral_64(PR_flistxattr, (int) p0, (uint64_t) g_2_h(p1), (size_t) p2, p3, p4, p5);
            break;
        case PR_flock:
            res = syscall_neutral_64(PR_flock, (int) p0, (int) p1, p2, p3, p4, p5);
            break;
        case PR_fsetxattr:
            res = syscall_neutral_64(PR_fsetxattr, (int) p0, (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), (size_t) p3, (int) p4, p5);
            break;
        case PR_fstat:
            res = syscall_neutral_64(PR_fstat, (int) p0, (uint64_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_fstatfs:
            res = syscall_neutral_64(PR_fstatfs, (int) p0, (uint64_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_fsync:
            res = syscall_neutral_64(PR_fsync, (int)p0, p1, p2, p3, p4, p5);
            break;
        case PR_ftruncate:
            res = syscall_neutral_64(PR_ftruncate, (int) p0, (off_t) p1, p2, p3, p4, p5);
            break;
        case PR_futex:
            res = futex_neutral(p0,p1,p2,p3,p4,p5);
            break;
        case PR_getcpu:
            res = syscall_neutral_64(PR_getcpu, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), p3, p4, p5);
                break;
        case PR_getcwd:
            res = syscall_neutral_64(PR_getcwd, (uint64_t)g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_getdents64:
            res = syscall_neutral_64(PR_getdents64, (int) p0, (uint64_t)g_2_h(p1), (unsigned int) p2, p3, p4, p5);
            break;
        case PR_getegid:
            res = syscall_neutral_64(PR_getegid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_geteuid:
            res = syscall_neutral_64(PR_geteuid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getgid:
            res = syscall_neutral_64(PR_getgid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getgroups:
            res = syscall_neutral_64(PR_getgroups, (int) p0, (uint64_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_getitimer:
            res = syscall_neutral_64(PR_getitimer, (int) p0, (uint64_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_getpeername:
            res = syscall_neutral_64(PR_getpeername, (int)p0, (uint64_t)g_2_h(p1), (uint64_t)g_2_h(p2), p3, p4, p5);
            break;
        case PR_getpgid:
            res = syscall_neutral_64(PR_getpgid, (pid_t)p0, p1, p2, p3, p4, p5);
            break;
        case PR_getpid:
            res = syscall_neutral_64(PR_getpid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getppid:
            res = syscall_neutral_64(PR_getppid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getpriority:
            res = syscall_neutral_64(PR_getpriority, (int) p0, (id_t) p1, p2, p3, p4, p5);
            break;
        case PR_getresgid:
            res = syscall_neutral_64(PR_getresgid, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_getresuid:
            res = syscall_neutral_64(PR_getresuid, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_getrlimit:
            res = syscall_neutral_64(PR_getrlimit, (int) p0, (uint64_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_getrusage:
            res = syscall_neutral_64(PR_getrusage, (int) p0, (uint64_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_getsid:
            res = syscall_neutral_64(PR_getsid, (pid_t) p0, p1, p2, p3, p4, p5);
            break;
        case PR_getsockname:
            res = syscall_neutral_64(PR_getsockname, p0, (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_getsockopt:
            res = syscall_neutral_64(PR_getsockopt, p0, p1, p2, IS_NULL(p3), IS_NULL(p4), p5);
            break;
        case PR_gettid:
            res = syscall_neutral_64(PR_gettid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_gettimeofday:
            res = syscall_neutral_64(PR_gettimeofday, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_getuid:
            res = syscall_neutral_64(PR_getuid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getxattr:
            res = syscall_neutral_64(PR_getxattr, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), (size_t) p3, p4, p5);
            break;
        case PR_inotify_add_watch:
            res = syscall_neutral_64(PR_inotify_add_watch, (int) p0, (uint64_t) g_2_h(p1), (uint32_t) p2, p3, p4, p5);
            break;
        case PR_inotify_init1:
            res = syscall_neutral_64(PR_inotify_init1, (int) p0, p1, p2, p3, p4, p5);
            break;
        case PR_inotify_rm_watch:
            res = syscall_neutral_64(PR_inotify_rm_watch, (int) p0, (int) p1, p2, p3, p4, p5);
            break;
        case PR_ioctl:
            /* FIXME: need specific version to offset param according to ioctl */
            res = syscall_neutral_64(PR_ioctl, (int) p0, (unsigned long) p1, (uint64_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_keyctl:
            /* FIXME: check according to params */
            res = syscall_neutral_64(PR_keyctl, (int) p0, p1,p2,p3,p4,p5);
            break;
        case PR_kill:
            res = syscall_neutral_64(PR_kill, (pid_t)p0, (int)p1, p2, p3, p4, p5);
            break;
        case PR_lgetxattr:
            res = syscall_neutral_64(PR_lgetxattr, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), (size_t) p3, p4, p5);
            break;
        case PR_linkat:
            res = syscall_neutral_64(PR_linkat, (int) p0, (uint64_t) g_2_h(p1), (int) p2, (uint64_t) g_2_h(p3), (int) p4, p5);
            break;
        case PR_listen:
            res = syscall_neutral_64(PR_listen, p0, p1, p2, p3, p4, p5);
            break;
        case PR_llistxattr:
            res = syscall_neutral_64(PR_llistxattr, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), (size_t) p2, p3, p4, p5);
            break;
        case PR_lseek:
            res = syscall_neutral_64(PR_lseek, (int)p0, (off_t)p1, (int)p2, p3, p4, p5);
            break;
        case PR_madvise:
            res = syscall_neutral_64(PR_madvise, (uint64_t) g_2_h(p0), (size_t) p1, (int) p2, p3, p4, p5);
            break;
        case PR_mincore:
            res = syscall_neutral_64(PR_mincore, (uint64_t) g_2_h(p0), (size_t) p1, (uint64_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_mkdirat:
            res = syscall_neutral_64(PR_mkdirat, (int) p0, (uint64_t) g_2_h(p1), (mode_t) p2, p3, p4, p5);
            break;
        case PR_mknodat:
            res = syscall_neutral_64(PR_mknodat, (int) p0, (uint64_t) g_2_h(p1), (mode_t) p2, (dev_t) p3, p4, p5);
            break;
        case PR_mlock:
            res = syscall_neutral_64(PR_mlock, (uint64_t) g_2_h(p0), (size_t) p1, p2, p3, p4, p5);
            break;
        case PR_mount:
            res = syscall_neutral_64(PR_mount, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), (unsigned long) p3, (uint64_t) g_2_h(p4), p5);
            break;
        case PR_mprotect:
            res = syscall_neutral_64(PR_mprotect, (uint64_t) g_2_h(p0), (size_t) p1, (int) p2, p3, p4, p5);
            break;
        case PR_mq_getsetattr:
            res = syscall_neutral_64(PR_mq_getsetattr, (neutral_mqd_t) p0, IS_NULL(p1), IS_NULL(p2), p3, p4, p5);
            break;
        case PR_mq_notify:
            res = syscall_neutral_64(PR_mq_notify, (neutral_mqd_t) p0, IS_NULL(p1), p2, p3, p4, p5);
            break;
        case PR_mq_open:
            res = syscall_neutral_64(PR_mq_open, (uint64_t) g_2_h(p0), (int) p1, (mode_t) p2, IS_NULL(p3), p4, p5);
            break;
        case PR_mq_timedreceive:
            res = syscall_neutral_64(PR_mq_timedreceive, (neutral_mqd_t) p0, (uint64_t) g_2_h(p1), (size_t) p2, IS_NULL(p3), IS_NULL(p4), p5);
            break;
        case PR_mq_timedsend:
            res = syscall_neutral_64(PR_mq_timedsend, (neutral_mqd_t) p0, (uint64_t) g_2_h(p1), (size_t) p2, (unsigned int) p3, IS_NULL(p4), p5);
            break;
        case PR_mq_unlink:
            res = syscall_neutral_64(PR_mq_unlink, (uint64_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_msgctl:
            res = syscall_neutral_64(PR_msgctl, (int) p0, (int) p1, (uint64_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_msgget:
            res = syscall_neutral_64(PR_msgget, (key_t) p0, (int) p1, p2, p3, p4, p5);
            break;
        case PR_msgrcv:
            res = syscall_neutral_64(PR_msgrcv, (int) p0, (uint64_t) g_2_h(p1), (size_t) p2, (long) p3, (int) p4, p5);
            break;
        case PR_msgsnd:
            res = syscall_neutral_64(PR_msgsnd, (int) p0, (uint64_t) g_2_h(p1), (size_t) p2, (int) p3, p4, p5);
            break;
        case PR_msync:
            res = syscall_neutral_64(PR_msync, (uint64_t) g_2_h(p0), (size_t) p1, (int) p2, p3, p4, p5);
            break;
        case PR_munlock:
            res = syscall_neutral_64(PR_munlock, (uint64_t) g_2_h(p0), (size_t) p1, p2, p3, p4, p5);
            break;
        case PR_name_to_handle_at:
            res = syscall_neutral_64(PR_name_to_handle_at, (int) p0, (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2),
                                                           (uint64_t) g_2_h(p3), (int) p4, p5);
            break;
        case PR_nanosleep:
            res = syscall_neutral_64(PR_nanosleep, (uint64_t) p0, IS_NULL(p1), p2, p3, p4, p5);
            break;
        case PR_newfstatat:
            res = syscall_neutral_64(PR_newfstatat, (int) p0, (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), (int) p3, p4, p5);
            break;
        case PR_openat:
            res = syscall_neutral_64(PR_openat, (int) p0, (uint64_t) g_2_h(p1), (int) p2, (mode_t) p3, p4, p5);
            break;
        case PR_perf_event_open:
            res = syscall_neutral_64(PR_perf_event_open, (uint64_t) g_2_h(p0), (pid_t) p1, (int) p2, (int) p3, (unsigned long) p4, p5);
            break;
        case PR_personality:
            res = syscall_neutral_64(PR_personality, (unsigned int)p0, p1, p2, p3, p4, p5);
            break;
        case PR_pipe2:
            res = syscall_neutral_64(PR_pipe2, (uint64_t) g_2_h(p0), (int) p1, p2, p3, p4, p5);
            break;
        case PR_ppoll:
            res = syscall_neutral_64(PR_ppoll, (uint64_t) g_2_h(p0), p1, IS_NULL(p2), IS_NULL(p3), (size_t) p4, p5);
            break;
        case PR_prctl:
            res = syscall_neutral_64(PR_prctl, (int) p0, (unsigned long) p1, (unsigned long) p2, (unsigned long) p3, (unsigned long) p4, p5);
            break;
        case PR_pread64:
            res = syscall_neutral_64(PR_pread64, (int) p0, (uint64_t) g_2_h(p1), (size_t) p2, (off_t) p3, p4, p5);
            break;
        case PR_prlimit64:
            res = syscall_neutral_64(PR_prlimit64, p0, p1, IS_NULL(p2), IS_NULL(p3), p4, p5);
            break;
        case PR_process_vm_readv:
            /* FIXME: offset support not easy to do. what if we return not implemented ? */
            res = syscall_neutral_64(PR_process_vm_readv, (pid_t) p0, (uint64_t) g_2_h(p1), (unsigned long) p2,
                                     (uint64_t) g_2_h(p3), (unsigned long) p4, (unsigned long) p5);
            break;
        case PR_process_vm_writev:
            /* FIXME: offset support not easy to do. what if we return not implemented ? */
            res = syscall_neutral_64(PR_process_vm_readv, (pid_t) p0, (uint64_t) g_2_h(p1), (unsigned long) p2,
                                     (uint64_t) g_2_h(p3), (unsigned long) p4, (unsigned long) p5);
            break;
        case PR_pselect6:
            res = pselect6_neutral(p0,p1,p2,p3,p4,p5);
            break;
        case PR_pwrite64:
            res = syscall_neutral_64(PR_pwrite64, (int) p0, (uint64_t) g_2_h(p1), (size_t) p2, (off_t) p3, p4, p5);
            break;
        case PR_quotactl:
            res = syscall_neutral_64(PR_quotactl, (int) p0, IS_NULL(p1), (int) p2, (uint64_t) g_2_h(p3), p4, p5);
            break;
        case PR_read:
            res = syscall_neutral_64(PR_read, (int)p0, (uint64_t) g_2_h(p1), (size_t) p2, p3, p4, p5);
            break;
        case PR_readahead:
            res = syscall_neutral_64(PR_readahead, (int) p0, (off64_t) p1, (size_t) p2, p3, p4, p5);
            break;
        case PR_readlinkat:
            res = syscall_neutral_64(PR_readlinkat, (int) p0, (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), (size_t) p3, p4, p5);
            break;
        case PR_readv:
            /* FIXME: need a neutral fonction since iov_base in struct iovec need translation */
            res = syscall_neutral_64(PR_readv, (int) p0, (uint64_t) g_2_h(p1), (int) p2, p3, p4, p5);
            break;
        case PR_recvfrom:
            res = syscall_neutral_64(PR_recvfrom, (int) p0, (uint64_t) g_2_h(p1), (size_t) p2, (int) p3, IS_NULL(p4), IS_NULL(p5));
            break;
        case PR_recvmsg:
            res = syscall_neutral_64(PR_recvmsg, (int) p0, (uint64_t) g_2_h(p1), (int) p2, p3, p4, p5);
            break;
        case PR_remap_file_pages:
            res = syscall_neutral_64(PR_remap_file_pages, (uint64_t) g_2_h(p0), (size_t) p1, (int) p2, (size_t) p3, (int) p4, p5);
            break;
        case PR_removexattr:
            res = syscall_neutral_64(PR_removexattr, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_renameat:
            res = syscall_neutral_64(PR_renameat, (int) p0, (uint64_t) g_2_h(p1), (int) p2, (uint64_t) g_2_h(p3), p4, p5);
            break;
        case PR_renameat2:
            res = syscall_neutral_64(PR_renameat2, (int) p0, (uint64_t) g_2_h(p1), (int) p2, (uint64_t) g_2_h(p3), p4, p5);
            break;
        case PR_rt_sigpending:
            res = syscall_neutral_64(PR_rt_sigpending, (uint64_t) g_2_h(p0), (size_t) p1, p2, p3, p4, p5);
            break;
        case PR_rt_sigprocmask:
            res = syscall_neutral_64(PR_rt_sigprocmask, (int)p0, IS_NULL(p1), IS_NULL(p2), (size_t) p3, p4, p5);
            break;
        case PR_rt_sigqueueinfo:
            res = syscall_neutral_64(PR_rt_sigqueueinfo, (pid_t) p0, (int) p1, (uint64_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_rt_sigsuspend:
            res = syscall_neutral_64(PR_rt_sigsuspend, (uint64_t) g_2_h(p0), (size_t) p1, p2, p3, p4, p5);
            break;
        case PR_rt_sigtimedwait:
            res = syscall_neutral_64(PR_rt_sigtimedwait, (uint64_t) g_2_h(p0), IS_NULL(p1), IS_NULL(p2), (size_t) p3, p4, p5);
            break;
        case PR_sched_get_priority_max:
            res = syscall_neutral_64(PR_sched_get_priority_max, (int) p0, p1, p2, p3, p4, p5);
            break;
        case PR_sched_get_priority_min:
            res = syscall_neutral_64(PR_sched_get_priority_min, (int) p0, p1, p2, p3, p4, p5);
            break;
        case PR_sched_getaffinity:
            res = syscall_neutral_64(PR_sched_getaffinity, (pid_t) p0, (size_t) p1, (uint64_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_sched_getparam:
            res = syscall_neutral_64(PR_sched_getparam, (pid_t) p0, IS_NULL(p1), p2, p3, p4, p5);
            break;
        case PR_sched_getscheduler:
            res = syscall_neutral_64(PR_sched_getscheduler, (pid_t) p0, p1, p2, p3, p4, p5);
            break;
        case PR_sched_setaffinity:
            res = syscall_neutral_64(PR_sched_setaffinity, (pid_t) p0, (size_t) p1, (uint64_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_sched_setparam:
            res = syscall_neutral_64(PR_sched_setparam, (pid_t) p0, IS_NULL(p1), p2, p3, p4, p5);
            break;
        case PR_sched_setscheduler:
            res = syscall_neutral_64(PR_sched_setscheduler, (pid_t) p0, (int) p1, (uint64_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_sched_yield:
            res = syscall_neutral_64(PR_sched_yield, p0, p1, p2, p3, p4, p5);
            break;
        case PR_semctl:
            res = semctl_neutral(p0,p1,p2,p3);
            break;
        case PR_semget:
            res = syscall_neutral_64(PR_semget, (key_t) p0, (int) p1, (int) p2, p3, p4, p5);
            break;
        case PR_semop:
            res = syscall_neutral_64(PR_semop, (int) p0, (uint64_t) g_2_h(p1), (size_t) p2, p3, p4, p5);
            break;
        case PR_sendfile:
            res = syscall_neutral_64(PR_sendfile, (int) p0, (int) p1, (uint64_t) g_2_h(p2), (size_t) p3, p4, p5);
            break;
        case PR_sendmmsg:
            res = syscall_neutral_64(PR_sendmmsg, (int) p0, (uint64_t) g_2_h(p1), (unsigned int) p2, (unsigned int) p3, p4, p5);
            break;
        case PR_sendmsg:
            /* FIXME: will not work with offset since msg struct contain pointers */
            res = syscall_neutral_64(PR_sendmsg, (int) p0, (uint64_t) g_2_h(p1), (int) p2, p3, p4, p5);
            break;
        case PR_sendto:
            res = syscall_neutral_64(PR_sendto, (int) p0, (uint64_t) g_2_h(p1), (size_t) p2, (unsigned int) p3, (uint64_t) g_2_h(p4), (socklen_t) p5);
            break;
        case PR_set_robust_list:
            /* FIXME: implement this correctly */
            res = -EPERM;
            break;
        case PR_set_tid_address:
            res = syscall_neutral_64(PR_set_tid_address, (uint64_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_setfsgid:
            res = syscall_neutral_64(PR_setfsgid, (gid_t) p0, p1, p2, p3, p4, p5);
            break;
        case PR_setfsuid:
            res = syscall_neutral_64(PR_setfsuid, (uid_t) p0, p1, p2, p3, p4, p5);
            break;
        case PR_setgid:
            res = syscall_neutral_64(PR_setgid, (gid_t) p0, p1, p2, p3, p4, p5);
            break;
        case PR_setgroups:
            res = syscall_neutral_64(PR_setgroups, (int) p0, (uint64_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_sethostname:
            res = syscall_neutral_64(PR_sethostname, (uint64_t) g_2_h(p0), (size_t) p1, p2, p3, p4, p5);
            break;
        case PR_setitimer:
            res = syscall_neutral_64(PR_setitimer, (int) p0, IS_NULL(p1), IS_NULL(p2), p3, p4, p5);
            break;
        case PR_setpgid:
            res = syscall_neutral_64(PR_setpgid, (pid_t)p0, (pid_t)p1, p2, p3, p4, p5);
            break;
        case PR_setpriority:
            res = syscall_neutral_64(PR_setpriority, (int) p0, (id_t) p1, (int) p2, p3, p4, p5);
            break;
        case PR_setregid:
            res = syscall_neutral_64(PR_setregid, (gid_t) p0, (gid_t) (int)p1, p2, p3, p4, p5);
            break;
        case PR_setresgid:
            res = syscall_neutral_64(PR_setresgid, (gid_t) p0, (gid_t) p1, (gid_t) p2, p3, p4, p5);
            break;
        case PR_setresuid:
            res = syscall_neutral_64(PR_setresuid, (uid_t) p0, (uid_t) p1, (uid_t) p2, p3, p4, p5);
            break;
        case PR_setreuid:
            res = syscall_neutral_64(PR_setreuid, (uid_t) p0, (uid_t) p1, p2, p3, p4, p5);
            break;
        case PR_setrlimit:
            res = syscall_neutral_64(PR_setrlimit, (int) p0, (uint64_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_setsid:
            res = syscall_neutral_64(PR_setsid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_setsockopt:
            res = syscall_neutral_64(PR_setsockopt, p0, p1, p2, (uint64_t) g_2_h(p3), p4, p5);
            break;
        case PR_setuid:
            res = syscall_neutral_64(PR_setuid, (uid_t) (int16_t)p0, p1, p2, p3, p4, p5);
            break;
        case PR_setxattr:
            res = syscall_neutral_64(PR_setxattr, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), (size_t) p3, (int) p4, p5);
            break;
        case PR_shmctl:
            res = syscall_neutral_64(PR_shmctl, (int) p0, (int) p1, (uint64_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_shmget:
            res = syscall_neutral_64(PR_shmget, (key_t) p0, (size_t) p1, (int) p2, p3 ,p4, p5);
            break;
        case PR_shutdown:
            res = syscall_neutral_64(PR_shutdown, (int) p0, (int) p1, p2, p3 ,p4, p5);
            break;
        case PR_signalfd4:
            res = syscall_neutral_64(PR_signalfd4, (int) p0, (uint64_t) g_2_h(p1), (size_t) p2, (int) p3, p4, p5);
            break;
        case PR_socket:
            res = syscall_neutral_64(PR_socket, (int)p0, (int)p1, (int)p2, p3 ,p4, p5);
            break;
        case PR_socketpair:
            res = syscall_neutral_64(PR_socketpair, (int)p0, (int)p1, (int)p2, (uint64_t) g_2_h(p3), p4, p5);
            break;
        case PR_splice:
            res = syscall_neutral_64(PR_splice, (int) p0, IS_NULL(p1), (int) p2, IS_NULL(p3), (size_t) p4, (unsigned int) p5);
            break;
        case PR_statfs:
            res = syscall_neutral_64(PR_statfs, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_symlinkat:
            res = syscall_neutral_64(PR_symlinkat, (uint64_t) g_2_h(p0), (int) p1, (uint64_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_sync:
            res = syscall_neutral_64(PR_sync, p0, p1, p2, p3, p4, p5);
            break;
        case PR_sync_file_range:
            res = syscall_neutral_64(PR_sync_file_range, (int) p0, (off64_t) p1, (off64_t) p2, (unsigned int) p3, p4, p5);
            break;
        case PR_syncfs:
            res = syscall_neutral_64(PR_syncfs, (int) p0, p1, p2, p3, p4, p5);
            break;
        case PR_sysinfo:
            res = syscall_neutral_64(PR_sysinfo, (uint64_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_tee:
            res = syscall_neutral_64(PR_tee, (int) p0, (int) p1, (size_t) p2, (unsigned int) p3, p4, p5);
            break;
        case PR_tgkill:
            res = syscall_neutral_64(PR_tgkill, (int)p0, (int)p1, (int)p2, p3, p4, p5);
            break;
        case PR_timer_create:
            res = syscall_neutral_64(PR_timer_create, (clockid_t) p0, IS_NULL(p1), IS_NULL(p2), p3, p4, p5);
            break;
        case PR_timer_getoverrun:
            res = syscall_neutral_64(PR_timer_getoverrun, p0, p1, p2, p3 ,p4, p5);
            break;
        case PR_timer_gettime:
            res = syscall_neutral_64(PR_timer_gettime, (uint64_t)(timer_t) p0, (uint64_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_timer_settime:
            res = syscall_neutral_64(PR_timer_settime, (uint64_t)(timer_t) p0, (int) p1, (uint64_t) g_2_h(p2), IS_NULL(p3), p4, p5);
            break;
        case PR_timerfd_create:
            res = syscall_neutral_64(PR_timerfd_create, (int) p0, (int) p1, p2, p3 ,p4, p5);
            break;
        case PR_timerfd_gettime:
            res = syscall_neutral_64(PR_timerfd_gettime, (int) p0, (uint64_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_timerfd_settime:
            res = syscall_neutral_64(PR_timerfd_settime, (int) p0, (int) p1, (uint64_t) g_2_h(p2), IS_NULL(p3), p4, p5);
            break;
        case PR_times:
            res = syscall_neutral_64(PR_times, (uint64_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_tkill:
            res = syscall_neutral_64(PR_tkill, (int) p0, (int) p1, p2, p3 ,p4, p5);
            break;
        case PR_truncate:
            res = syscall_neutral_64(PR_truncate, (uint64_t) g_2_h(p0), (off_t) p1, p2, p3 ,p4, p5);
            break;
        case PR_umask:
            res = syscall_neutral_64(PR_umask, (mode_t) p0, p1, p2, p3 ,p4, p5);
            break;
        case PR_umount:
            res = syscall_neutral_64(PR_umount, (uint64_t) g_2_h(p0), p1, p2, p3 ,p4, p5);
            break;
        case PR_umount2:
            res = syscall_neutral_64(PR_umount2, (uint64_t) g_2_h(p0), p1, p2, p3 ,p4, p5);
            break;
        case PR_uname:
            res = syscall_neutral_64(PR_uname, (uint64_t) g_2_h(p0), p1, p2, p3 ,p4, p5);
            break;
        case PR_unlinkat:
            res = syscall_neutral_64(PR_unlinkat, (int)p0, (uint64_t) g_2_h(p1), (int)p2, p3, p4, p5);
            break;
        case PR_unshare:
            res = syscall_neutral_64(PR_unshare, (int) p0, p1, p2, p3, p4, p5);
            break;
        case PR_utimensat:
            res = syscall_neutral_64(PR_utimensat, (int) p0, (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), (int) p3, p4, p5);
            break;
        case PR_vmsplice:
            /* FIXME: translate iov_base of iovec */
            res = syscall_neutral_64(PR_vmsplice, (int) p0, (uint64_t) g_2_h(p1), (unsigned long) p2, (unsigned int) p3, p4, p5);
            break;
        case PR_waitid:
            res = syscall_neutral_64(PR_waitid, (int) p0, (id_t) p1, IS_NULL(p2), (int) p3, IS_NULL(p4), p5);
            break;
        case PR_write:
            res = syscall_neutral_64(PR_write, (int)p0, (uint64_t)g_2_h(p1), (size_t)p2, p3, p4, p5);
            break;
        case PR_writev:
            /* FIXME: translate iov_base of iovec */
            res = syscall_neutral_64(PR_writev, (int) p0, (uint64_t) g_2_h(p1), (int) p2, p3, p4, p5);
            break;
        case PR_getrandom:
            res = syscall_neutral_64(PR_getrandom, (uint64_t) g_2_h(p0), (size_t) p1, (unsigned int) p2, p3, p4, p5);
            break;
        default:
            fatal("syscall_adapter_guest64: unsupported neutral syscall %d\n", no);
    }

    return res;
}
