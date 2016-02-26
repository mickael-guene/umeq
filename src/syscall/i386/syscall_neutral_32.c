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
#include "syscalls_neutral_types.h"
#include "runtime.h"
#include "target32.h"
#include "syscall_i386.h"

struct convertFlags {
    int neutralFlag;
    int x86Flag;
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

static int neutralToX86Flags(int neutral_flags)
{
    int res = 0;
    int i;

    for(i=0;i<sizeof(neutralToX86FlagsTable) / sizeof(struct convertFlags); i++) {
        if (neutral_flags & neutralToX86FlagsTable[i].neutralFlag)
            res |= neutralToX86FlagsTable[i].x86Flag;
    }

    return res;
}

static int x86ToNeutralFlags(int x86_flags)
{
    int res = 0;
    int i;

    for(i=0;i<sizeof(neutralToX86FlagsTable) / sizeof(struct convertFlags); i++) {
        if (x86_flags & neutralToX86FlagsTable[i].x86Flag)
            res |= neutralToX86FlagsTable[i].neutralFlag;
    }

    return res;
}

static int fcntl64_neutral(int fd, int cmd, uint32_t opt_p)
{
    int res = -EINVAL;

    switch(cmd) {
        case F_GETFL:/*3*/
            res = syscall(SYS_fcntl64, fd, cmd);
            res = x86ToNeutralFlags(res);
            break;
        case F_SETFL:/*4*/
            res = syscall(SYS_fcntl64, fd, cmd, neutralToX86Flags(opt_p));
            break;
        /* following are neutral approved !!!! */
        case F_DUPFD:/*0*/
        case F_GETFD:/*1*/
        case F_SETFD:/*2*/
        case F_GETLK:/*5*/
        case F_SETLK:/*6*/
        case F_SETLKW:/*7*/
        case F_SETOWN:/*8*/
        case F_GETOWN:/*9*/
        case 10:/*F_SETSIG*/
        case 11:/*F_GETSIG*/
        case 15://F_SETOWN_EX
        case F_GETOWN_EX:/*16*/
        case 1024:/*F_SETLEASE*/
        case 1025:/*F_GETLEASE:*/
        case F_DUPFD_CLOEXEC:/*1030*/
        case 1031:/*F_SETPIPE_SZ:*/
        case 1032:/*F_GETPIPE_SZ:*/
            res = syscall(SYS_fcntl64, fd,cmd, opt_p);
            break;
        case F_GETLK64:/*12*/
            {
                struct neutral_flock64_32 *lock_guest = (struct neutral_flock64_32 *) opt_p;
                struct flock64 lock;

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
            break;
        case F_SETLK64:/*13*/
            {
                struct neutral_flock64_32 *lock_guest = (struct neutral_flock64_32 *) opt_p;
                struct flock64 lock;

                lock.l_type = lock_guest->l_type;
                lock.l_whence = lock_guest->l_whence;
                lock.l_start = lock_guest->l_start;
                lock.l_len = lock_guest->l_len;
                lock.l_pid = lock_guest->l_pid;
                res = syscall(SYS_fcntl64, fd, cmd, &lock);
            }
            break;
        case F_SETLKW64:/*14*/
            {
                struct neutral_flock64_32 *lock_guest = (struct neutral_flock64_32 *) opt_p;
                struct flock64 lock;

                lock.l_type = lock_guest->l_type;
                lock.l_whence = lock_guest->l_whence;
                lock.l_start = lock_guest->l_start;
                lock.l_len = lock_guest->l_len;
                lock.l_pid = lock_guest->l_pid;
                res = syscall(SYS_fcntl64, fd, cmd, &lock);
            }
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

#include <sys/epoll.h>
static int epoll_ctl_neutral(uint32_t epfd_p, uint32_t op_p, uint32_t fd_p, uint32_t event_p)
{
    int res;
    int epfd = (int) epfd_p;
    int op = (int) op_p;
    int fd = (int) fd_p;
    struct neutral_epoll_event *event_guest = (struct neutral_epoll_event *) event_p;
    struct epoll_event event;

    if (event_p) {
        event.events = event_guest->events;
        event.data.u64 = event_guest->data;
    }
    res = syscall(SYS_epoll_ctl, epfd, op, fd, event_p?&event:NULL);

    return res;
}

static int epoll_wait_neutral(uint32_t epfd_p, uint32_t events_p, uint32_t maxevents_p, uint32_t timeout_p)
{
    int res;
    int epfd = (int) epfd_p;
    struct neutral_epoll_event *events_guest = (struct neutral_epoll_event *) events_p;
    int maxevents = (int) maxevents_p;
    int timeout = (int) timeout_p;
    struct epoll_event *events = (struct epoll_event *) alloca(maxevents * sizeof(struct epoll_event));
    int i;

    res = syscall(SYS_epoll_wait, epfd, events, maxevents, timeout);
    for(i = 0; i < maxevents; i++) {
        events_guest[i].events = events[i].events;
        events_guest[i].data = events[i].data.u64;
    }

    return res;
}

int syscall_neutral_32(Sysnum no, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5)
{
    int res = -ENOSYS;

    switch(no) {
        case PR__newselect:
            res = syscall(SYS__newselect, p0, p1, p2, p3, p4);
            break;
        case PR__llseek:
            res = syscall(SYS__llseek, p0, p1, p2, p3, p4);
            break;
        case PR_accept:
            {
                unsigned long args[3];

                args[0] = (int) p0;
                args[1] = (int) p1;
                args[2] = (int) p2;
                res = syscall(SYS_socketcall, 5/*sys_accept*/, args);
            }
            break;
        case PR_accept4:
            res = syscall(SYS_accept4, p0, p1, p2, p3);
            break;
        case PR_access:
            res = syscall(SYS_access, p0, p1);
            break;
        case PR_add_key:
            res = syscall(SYS_add_key, p0, p1, p2, p3, p4);
            break;
        case PR_bind:
            res= syscall(SYS_bind, p0, p1, p2);
            break;
        case PR_capget:
            res = syscall(SYS_capget, p0, p1);
            break;
        case PR_capset:
            res = syscall(SYS_capset, p0, p1);
            break;
        case PR_chdir:
            res = syscall(SYS_chdir, p0);
            break;
        case PR_chmod:
            res = syscall(SYS_chmod, p0, p1);
            break;
        case PR_chown32:
            res = syscall(SYS_chown32, p0, p1, p2);
            break;
        case PR_chroot:
            res = syscall(SYS_chroot, p0);
            break;
        case PR_clock_getres:
            res = syscall(SYS_clock_getres, p0, p1);
            break;
        case PR_clock_gettime:
            res = syscall(SYS_clock_gettime, p0, p1);
            break;
        case PR_clock_nanosleep:
            res = syscall(SYS_clock_nanosleep, p0, p1, p2, p3);
            break;
        case PR_clock_settime:
            res = syscall(SYS_clock_settime, p0, p1);
            break;
        case PR_close:
            res = syscall(SYS_close, p0);
            break;
        case PR_connect:
            res = syscall(SYS_connect, p0, p1, p2);
            break;
        case PR_creat:
            res = syscall(SYS_creat, p0, p1);
            break;
        case PR_dup:
            res = syscall(SYS_dup, p0);
            break;
        case PR_dup2:
            res = syscall(SYS_dup2, p0, p1);
            break;
        case PR_dup3:
            res = syscall(SYS_dup3, p0, p1, p2);
            break;
        case PR_epoll_ctl:
            res = epoll_ctl_neutral(p0, p1, p2, p3);
            break;
        case PR_epoll_create:
            res = syscall(SYS_epoll_create, p0);
            break;
        case PR_epoll_create1:
            res = syscall(SYS_epoll_create1, p0);
            break;
        case PR_epoll_wait:
            res = epoll_wait_neutral(p0 , p1, p2, p3);
            break;
        case PR_eventfd:
            res = syscall(SYS_eventfd, p0, p1);
            break;
        case PR_eventfd2:
            res = syscall(SYS_eventfd2, p0, p1);
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
        case PR_fadvise64_64:
            res = syscall(SYS_fadvise64_64, p0, p1, p2, p3, p4, p5);
            break;
        case PR_fallocate:
            res = syscall(SYS_fallocate, p0, p1, p2, p3, p4, p5);
            break;
        case PR_fchdir:
            res = syscall(SYS_fchdir, p0);
            break;
        case PR_fchmod:
            res = syscall(SYS_fchmod, p0, p1);
            break;
        case PR_fchmodat:
            res = syscall(SYS_fchmodat, p0, p1, p2);
            break;
        case PR_fchown32:
            res = syscall(SYS_fchown32, p0, p1, p2);
            break;
        case PR_fchownat:
            res = syscall(SYS_fchownat, p0, p1, p2, p3, p4);
            break;
        case PR_fcntl64:
            res = fcntl64_neutral(p0, p1, p2);
            break;
        case PR_fdatasync:
            res = syscall(SYS_fdatasync, p0);
            break;
        case PR_fgetxattr:
            res = syscall(SYS_fgetxattr, p0, p1, p2, p3);
            break;
        case PR_flistxattr:
            res = syscall(SYS_flistxattr, p0, p1, p2);
            break;
        case PR_flock:
            res = syscall(SYS_flock, p0, p1);
            break;
        case PR_fsetxattr:
            res = syscall(SYS_fsetxattr, p0, p1, p2, p3, p4);
            break;
        case PR_ftruncate:
            res = syscall(SYS_ftruncate, p0, p1);
            break;
        case PR_ftruncate64:
            res = syscall(SYS_ftruncate64, p0, p1, p2);
            break;
        case PR_futimesat:
            res = syscall(SYS_futimesat, p0, p1, p2);
            break;
        case PR_futex:
            res = syscall(SYS_futex, p0, p1, p2, p3, p4, p5);
            break;
        case PR_fstat64:
            res = syscall_i386_fstat64(p0, p1);
            break;
        case PR_fstatat64:
            res = syscall_i386_fstatat64(p0, p1, p2, p3);
            break;
        case PR_fstatfs:
            res = syscall(SYS_fstatfs, p0, p1);
            break;
        case PR_fstatfs64:
            /* due to padding the length of i386 is different but layout is the same */
            res = syscall(SYS_fstatfs64, p0, 84/*(size_t) p1*/, p2);
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
        case PR_getegid32:
            res = syscall(SYS_getegid32);
            break;
        case PR_geteuid:
            res = syscall(SYS_geteuid);
            break;
        case PR_geteuid32:
            res = syscall(SYS_geteuid32);
            break;
        case PR_getgid:
            res = syscall(SYS_getgid);
            break;
        case PR_getgid32:
            res = syscall(SYS_getgid32);
            break;
        case PR_getitimer:
            res = syscall(SYS_getitimer, p0, p1);
            break;
        case PR_getgroups:
            res = syscall(SYS_getgroups, p0, p1);
            break;
        case PR_getgroups32:
            res = syscall(SYS_getgroups32, p0, p1);
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
        case PR_getpriority:
            res = syscall(SYS_getpriority, p0, p1);
            break;
        case PR_getresgid32:
            res = syscall(SYS_getresgid32, p0, p1, p2);
            break;
        case PR_getresuid32:
            res = syscall(SYS_getresuid32, p0, p1, p2);
            break;
        case PR_getrlimit:
            res = syscall(SYS_getrlimit, p0, p1);
            break;
        case PR_getsid:
            res = syscall(SYS_getsid, p0);
            break;
        case PR_getsockname:
            res = syscall(SYS_getsockname, p0, p1, p2);
            break;
        case PR_getsockopt:
            res = syscall(SYS_getsockopt, p0, p1, p2, p3, p4);
            break;
        case PR_getrusage:
            res = syscall(SYS_getrusage, p0, p1);
            break;
        case PR_getpeername:
            res = syscall(SYS_getpeername, p0, p1, p2);
            break;
        case PR_getpgid:
            res = syscall(SYS_getpgid, p0);
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
        case PR_getuid32:
            res = syscall(SYS_getuid32);
            break;
        case PR_getxattr:
            res = syscall(SYS_getxattr, p0, p1, p2, p3);
            break;
        case PR_inotify_init:
            res = syscall(SYS_inotify_init);
            break;
        case PR_inotify_init1:
            res = syscall(SYS_inotify_init1, p0);
            break;
        case PR_inotify_add_watch:
            res = syscall(SYS_inotify_add_watch, p0, p1, p2);
            break;
        case PR_inotify_rm_watch:
            res = syscall(SYS_inotify_rm_watch, p0, p1);
            break;
        case PR_ioctl:
            res = syscall(SYS_ioctl, p0, p1, p2, p3, p4, p5);
            break;
        case PR_keyctl:
            res = syscall(SYS_keyctl, p0, p1, p2, p3, p4);
            break;
        case PR_kill:
            res = syscall(SYS_kill, p0, p1);
            break;
        case PR_lchown32:
            res = syscall(SYS_lchown32, p0, p1, p2);
            break;
        case PR_link:
            res = syscall(SYS_link, p0, p1);
            break;
        case PR_linkat:
            res = syscall(SYS_linkat, p0, p1, p2, p3, p4);
            break;
        case PR_listen:
            res = syscall(SYS_listen, p0, p1);
            break;
        case PR_lgetxattr:
            res = syscall(SYS_lgetxattr, p0, p1, p2, p3);
            break;
        case PR_lseek:
            res = syscall(SYS_lseek, p0, p1, p2);
            break;
        case PR_lstat64:
            res = syscall_i386_lstat64(p0, p1);
            break;
        case PR_madvise:
            res = syscall(SYS_madvise, p0, p1, p2);
            break;
        case PR_mincore:
            res = syscall(SYS_mincore, p0, p1, p2);
            break;
        case PR_mkdir:
            res = syscall(SYS_mkdir, p0, p1);
            break;
        case PR_mkdirat:
            res = syscall(SYS_mkdirat, p0, p1, p2);
            break;
        case PR_mknod:
            res = syscall(SYS_mknod, p0, p1, p2);
            break;
        case PR_mknodat:
            res = syscall(SYS_mknodat, p0, p1, p2, p3);
            break;
        case PR_mlock:
            res = syscall(SYS_mlock, p0, p1);
            break;
        case PR_mprotect:
            res = syscall(SYS_mprotect, p0, p1, p2);
            break;
        case PR_mq_getsetattr:
            res = syscall(SYS_mq_getsetattr, p0, p1, p2);
            break;
        case PR_mq_notify:
            res = syscall(SYS_mq_notify, p0, p1);
            break;
        case PR_mq_open:
            res = syscall(SYS_mq_open, p0, p1, p2, p3);
            break;
        case PR_mq_timedreceive:
            res = syscall(SYS_mq_timedreceive, p0, p1, p2, p3, p4);
            break;
        case PR_mq_timedsend:
            res = syscall(SYS_mq_timedsend, p0, p1, p2, p3, p4);
            break;
        case PR_mq_unlink:
            res = syscall(SYS_mq_unlink, p0);
            break;
        case PR_msgctl:
            res = syscall(SYS_ipc, 14/*IPCOP_msgctl*/, p0, p1, 0, p2);
            break;
        case PR_msgget:
            res = syscall(SYS_ipc, 13/*IPCOP_msgget*/, p0, p1);
            break;
        case PR_msgrcv:
            {
                struct ipc_kludge {
                    void *msgp;
                    long int msgtyp;
                } tmp;

                tmp.msgp = (void *) p1;
                tmp.msgtyp = (long) p3;
                res = syscall(SYS_ipc, 12/*IPCOP_msgrcv*/, (int) p0, (size_t) p2, (int) p4, &tmp);
            }
            break;
        case PR_msgsnd:
            res = syscall(SYS_ipc, 11/*IPCOP_msgsnd*/, p0, p2, p3, p1);
            break;
        case PR_msync:
            res = syscall(SYS_msync, p0, p1, p2);
            break;
        case PR_munlock:
            res = syscall(SYS_munlock, p0, p1);
            break;
        case PR_nanosleep:
            res = syscall(SYS_nanosleep, p0, p1);
            break;
        case PR_pause:
            res = syscall(SYS_pause);
            break;
        case PR_perf_event_open:
            res = syscall(SYS_perf_event_open, p0, p1, p2, p3, p4);
            break;
        case PR_personality:
            res = syscall(SYS_personality, p0);
            break;
        case PR_pipe:
            res = syscall(SYS_pipe, p0);
            break;
        case PR_pipe2:
            res = syscall(SYS_pipe2, p0, p1);
            break;
        case PR_poll:
            res = syscall(SYS_poll, p0, p1, p2);
            break;
        case PR_ppoll:
            res = syscall(SYS_ppoll, p0, p1, p2, p3, p4);
            break;
        case PR_prctl:
            res = syscall(SYS_prctl, p0, p1, p2, p3, p4);
            break;
        case PR_pread64:
            res = syscall(SYS_pread64, p0, p1, p2, p3, p4);
            break;
        case PR_prlimit64:
            res = syscall(SYS_prlimit64, p0, p1, p2, p3);
            break;
        case PR_pselect6:
            res = syscall(SYS_pselect6, p0, p1, p2, p3, p4, p5);
            break;
        case PR_pwrite64:
            res = syscall(SYS_pwrite64, p0, p1, p2, p3, p4);
            break;
        case PR_quotactl:
            res = syscall(SYS_quotactl, p0, p1, p2, p3);
            break;
        case PR_read:
            res = syscall(SYS_read, p0, p1, p2);
            break;
        case PR_readlink:
            res = syscall(SYS_readlink, p0, p1, p2);
            break;
        case PR_readlinkat:
            res = syscall(SYS_readlinkat, p0, p1, p2, p3);
            break;
        case PR_readv:
            res = syscall(SYS_readv, p0, p1, p2);
            break;
        case PR_recv:
            {
                unsigned long args[4];

                args[0] = (int) p0;
                args[1] = (unsigned long)p1;
                args[2] = (size_t) p2;
                args[3] = (int) p3;
                res = syscall(SYS_socketcall, 10 /*sys_recv*/, args);
            }
            break;
        case PR_recvfrom:
            res = syscall(SYS_recvfrom, p0, p1, p2, p3, p4, p5);
            break;
        case PR_recvmsg:
            res = syscall(SYS_recvmsg, p0, p1, p2);
            break;
        case PR_remap_file_pages:
            res = syscall(SYS_remap_file_pages, p0, p1, p2, p3, p4);
            break;
        case PR_rename:
            res = syscall(SYS_rename, p0, p1);
            break;
        case PR_rmdir:
            res = syscall(SYS_rmdir, p0);
            break;
        case PR_rt_sigpending:
            res = syscall(SYS_rt_sigpending, p0, p1);
            break;
        case PR_rt_sigprocmask:
            res = syscall(SYS_rt_sigprocmask, p0, p1, p2, p3);
            break;
        case PR_rt_sigsuspend:
            res = syscall(SYS_rt_sigsuspend, p0, p1);
            break;
        case PR_rt_sigtimedwait:
            res = syscall(SYS_rt_sigtimedwait, p0, p1, p2, p3);
            break;
        case PR_tgkill:
            res = syscall(SYS_tgkill, p0, p1, p2);
            break;
        case PR_sched_getaffinity:
            res = syscall(SYS_sched_getaffinity, p0, p1, p2);
            break;
        case PR_sched_getparam:
            res = syscall(SYS_sched_getparam, p0, p1);
            break;
        case PR_sched_getscheduler:
            res = syscall(SYS_sched_getscheduler, p0);
            break;
        case PR_sched_get_priority_max:
            res = syscall(SYS_sched_get_priority_max, p0);
            break;
        case PR_sched_get_priority_min:
            res = syscall(SYS_sched_get_priority_min, p0);
            break;
        case PR_sched_rr_get_interval:
            res = syscall(SYS_sched_rr_get_interval, p0, p1);
            break;
        case PR_sched_setaffinity:
            res = syscall(SYS_sched_setaffinity, p0, p1, p2);
            break;
        case PR_sched_setparam:
            res = syscall(SYS_sched_setparam, p0, p1);
            break;
        case PR_sched_setscheduler:
            res = syscall(SYS_sched_setscheduler, p0, p1, p2);
            break;
        case PR_sched_yield:
            res = syscall(SYS_sched_yield);
            break;
        case PR_semctl:
            res = syscall(SYS_ipc, 3/*IPCOP_semctl*/, (int) p0, (int) p1, (int) p2, &p3);
            break;
        case PR_semget:
            res = syscall(SYS_ipc, 2/*IPCOP_semget*/, (key_t) p0, (int) p1, (int) p2);
            break;
        case PR_semop:
            res = syscall(SYS_ipc, 1/*IPCOP_semop*/, (int) p0, (size_t) p2, 0, p1);
            break;
        case PR_send:
            {
                unsigned long args[4];

                args[0] = (int) p0;
                args[1] = (int) p1;
                args[2] = (int) p2;
                args[3] = (int) p3;
                res = syscall(SYS_socketcall, 9/*sys_send*/, args);
            }
            break;
        case PR_sendfile:
            res = syscall(SYS_sendfile, p0, p1, p2, p3);
            break;
        case PR_sendmsg:
            res = syscall(SYS_sendmsg, p0, p1, p2);
            break;
        case PR_sendto:
            res = syscall(SYS_sendto, p0, p1, p2, p3, p4, p5);
            break;
        case PR_sendfile64:
            res = syscall(SYS_sendfile64, p0, p1, p2, p3);
            break;
        case PR_set_tid_address:
            res = syscall(SYS_set_tid_address, p0);
            break;
        case PR_setfsgid:
            res = syscall(SYS_setfsgid, p0);
            break;
        case PR_setfsgid32:
            res = syscall(SYS_setfsgid32, p0);
            break;
        case PR_setfsuid:
            res = syscall(SYS_setfsuid, p0);
            break;
        case PR_setfsuid32:
            res = syscall(SYS_setfsuid32, p0);
            break;
        case PR_setgroups32:
            res = syscall(SYS_setgroups32, p0, p1);
            break;
        case PR_setgid:
            res = syscall(SYS_setgid, p0);
            break;
        case PR_setgid32:
            res = syscall(SYS_setgid32, p0);
            break;
        case PR_sethostname:
            res = syscall(SYS_sethostname, p0, p1);
            break;
        case PR_setitimer:
            res = syscall(SYS_setitimer, p0, p1, p2);
            break;
        case PR_setpgid:
            res = syscall(SYS_setpgid, p0, p1);
            break;
        case PR_setpriority:
            res = syscall(SYS_setpriority, p0, p1, p2);
            break;
        case PR_setresgid32:
            res = syscall(SYS_setresgid32, p0, p1, p2);
            break;
        case PR_setresuid32:
            res = syscall(SYS_setresuid32, p0, p1, p2);
            break;
        case PR_setreuid:
            res = syscall(SYS_setreuid, p0, p1);
            break;
        case PR_setreuid32:
            res = syscall(SYS_setreuid32, p0, p1);
            break;
        case PR_setregid:
            res = syscall(SYS_setregid, p0, p1);
            break;
        case PR_setregid32:
            res = syscall(SYS_setregid32, p0, p1);
            break;
        case PR_setrlimit:
            res = syscall(SYS_setrlimit, p0, p1);
            break;
        case PR_setsid:
            res = syscall(SYS_setsid);
            break;
        case PR_setsockopt:
            res = syscall(SYS_setsockopt, p0, p1, p2, p3, p4);
            break;
        case PR_setuid:
            res = syscall(SYS_setuid, p0);
            break;
        case PR_setuid32:
            res = syscall(SYS_setuid32, p0);
            break;
        case PR_setxattr:
            res = syscall(SYS_setxattr, p0, p1, p2, p3, p4);
            break;
        case PR_shmctl:
            res = syscall(SYS_ipc, 24/*IPCOP_shmctl*/, p0, p1, 0, p2, 0, 0);
            break;
        case PR_shmget:
            res = syscall(SYS_ipc, 23/*IPCOP_shmget*/, p0, p1, p2);
            break;
        case PR_shutdown:
            res = syscall(SYS_shutdown, p0, p1);
            break;
        case PR_signalfd4:
            res = syscall(SYS_signalfd4, p0, p1, p2, p3);
            break;
        case PR_socket:
            res = syscall(SYS_socket, p0, p1, p2);
            break;
        case PR_socketpair:
            res = syscall(SYS_socketpair, p0, p1, p2, p3);
            break;
        case PR_splice:
            res = syscall(SYS_splice, p0, p1, p2, p3, p4, p5);
            break;
        case PR_stat64:
            res = syscall_i386_stat64(p0, p1);
            break;
        case PR_statfs:
            res = syscall(SYS_statfs, p0, p1);
            break;
        case PR_statfs64:
            /* due to padding the length of i386 is different but layout is the same */
            res = syscall(SYS_statfs64, p0, 84/*(size_t) p1*/, p2);
            break;
        case PR_symlink:
            res = syscall(SYS_symlink, p0, p1);
            break;
        case PR_symlinkat:
            res = syscall(SYS_symlinkat, p0, p1, p2);
            break;
        case PR_sync:
            res = syscall(SYS_sync);
            break;
        case PR_sync_file_range:
            res = syscall(SYS_sync_file_range, p0, p1, p2, p3, p4, p5);
            break;
        case PR_sysinfo:
            res = syscall(SYS_sysinfo, p0);
            break;
        case PR_tee:
            res = syscall(SYS_tee, p0, p1, p2, p3);
            break;
        case PR_timer_create:
            res = syscall(SYS_timer_create, p0, p1, p2);
            break;
        case PR_timer_delete:
            res = syscall(SYS_timer_delete, p0);
            break;
        case PR_timer_getoverrun:
            res = syscall(SYS_timer_getoverrun, p0);
            break;
        case PR_timer_gettime:
            res = syscall(SYS_timer_gettime, p0, p1);
            break;
        case PR_timer_settime:
            res = syscall(SYS_timer_settime, p0, p1, p2, p3);
            break;
        case PR_timerfd_create:
            res = syscall(SYS_timerfd_create, p0, p1);
            break;
        case PR_timerfd_gettime:
            res = syscall(SYS_timerfd_gettime, p0, p1);
            break;
        case PR_timerfd_settime:
            res = syscall(SYS_timerfd_settime, p0, p1, p2, p3);
            break;
        case PR_times:
            res = syscall(SYS_times, p0);
            break;
        case PR_tkill:
            res = syscall(SYS_tkill, p0, p1);
            break;
        case PR_truncate:
            res = syscall(SYS_truncate, p0, p1);
            break;
        case PR_truncate64:
            res = syscall(SYS_truncate64, p0, p1, p2);
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
        case PR_utimes:
            res = syscall(SYS_utimes, p0, p1);
            break;
        case PR_vfork:
            /* implement with fork to avoid sync problem but semantic is not fully preserved ... */
            res = syscall(SYS_fork);
            break;
        case PR_vhangup:
            res = syscall(SYS_vhangup);
            break;
        case PR_vmsplice:
            res = syscall(SYS_vmsplice, p0, p1, p2, p3);
            break;
        case PR_waitid:
            res = syscall(SYS_waitid, p0, p1, p2, p3, p4);
            break;
        case PR_write:
            res = syscall(SYS_write, p0, p1, p2);
            break;
        case PR_writev:
            res = syscall(SYS_writev, p0, p1, p2);
            break;
        default:
            fatal("syscall_neutral_32: unsupported neutral syscall %d\n", no);
    }

    return res;
}
