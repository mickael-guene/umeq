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
#include "target64.h"
#include "syscall_x86_64.h"

#ifndef SYS_getrandom
 #define SYS_getrandom 318
#endif

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

static long neutralToX86Flags(long neutral_flags)
{
    long res = 0;
    int i;

    for(i=0;i<sizeof(neutralToX86FlagsTable) / sizeof(struct convertFlags); i++) {
        if (neutral_flags & neutralToX86FlagsTable[i].neutralFlag)
            res |= neutralToX86FlagsTable[i].x86Flag;
    }

    return res;
}

static long x86ToNeutralFlags(long x86_flags)
{
    long res = 0;
    int i;

    for(i=0;i<sizeof(neutralToX86FlagsTable) / sizeof(struct convertFlags); i++) {
        if (x86_flags & neutralToX86FlagsTable[i].x86Flag)
            res |= neutralToX86FlagsTable[i].neutralFlag;
    }

    return res;
}

static long fcntl_neutral(int fd, int cmd, uint64_t opt_p)
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
        case F_GETLK:/*5*/
        case F_SETLK:/*6*/
        case F_SETLKW:/*7*/
        case F_SETOWN:/*8*/
        case F_GETOWN:/*9*/
        case 10:/*F_SETSIG*/
        case 11:/*F_GETSIG*/
        case 12:/*F_GETLK64:*/
        case 13://F_SETLK64
        case 14://F_SETLKW64
        case 15://F_SETOWN_EX
        case F_GETOWN_EX:/*16*/
        case 1024:/*F_SETLEASE*/
        case 1025:/*F_GETLEASE:*/
        case F_DUPFD_CLOEXEC:/*1030*/
        case 1031:/*F_SETPIPE_SZ:*/
        case 1032:/*F_GETPIPE_SZ:*/
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

#include <sys/epoll.h>
static long epoll_ctl_neutral(uint64_t epfd_p, uint64_t op_p, uint64_t fd_p, uint64_t event_p)
{
    long res;
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

static long epoll_wait_neutral(uint64_t epfd_p, uint64_t events_p, uint64_t maxevents_p, uint64_t timeout_p)
{
    long res;
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

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
static long semctl_neutral(uint64_t semid_p, uint64_t semnum_p, uint64_t cmd_p, uint64_t arg0_p)
{
    long res;
    int cmd = (int) cmd_p;

    switch(cmd) {
        case IPC_SET:
            {
                struct neutral_semid64_ds_64 *buf_neutral = (struct neutral_semid64_ds_64 *) arg0_p;
                struct semid_ds buf;

                buf.sem_perm.__key = buf_neutral->sem_perm.key;
                buf.sem_perm.uid = buf_neutral->sem_perm.uid;
                buf.sem_perm.gid = buf_neutral->sem_perm.gid;
                buf.sem_perm.cuid = buf_neutral->sem_perm.cuid;
                buf.sem_perm.cgid = buf_neutral->sem_perm.cgid;
                buf.sem_perm.mode = buf_neutral->sem_perm.mode;
                buf.sem_perm.__seq = buf_neutral->sem_perm.seq;
                buf.sem_otime = buf_neutral->sem_otime;
                buf.sem_ctime = buf_neutral->sem_ctime;
                buf.sem_nsems = buf_neutral->sem_nsems;
                res = syscall(SYS_semctl, semid_p, semnum_p, cmd_p, &buf);
                buf_neutral->sem_perm.key = buf.sem_perm.__key;
                buf_neutral->sem_perm.uid = buf.sem_perm.uid;
                buf_neutral->sem_perm.gid = buf.sem_perm.gid;
                buf_neutral->sem_perm.cuid = buf.sem_perm.cuid;
                buf_neutral->sem_perm.cgid = buf.sem_perm.cgid;
                buf_neutral->sem_perm.mode = buf.sem_perm.mode;
                buf_neutral->sem_perm.seq = buf.sem_perm.__seq;
                buf_neutral->sem_otime = buf.sem_otime;
                buf_neutral->sem_ctime = buf.sem_ctime;
                buf_neutral->sem_nsems = buf.sem_nsems;
            }
            break;
        case SEM_STAT:
        case IPC_STAT:
            {
                struct neutral_semid64_ds_64 *buf_neutral = (struct neutral_semid64_ds_64 *) arg0_p;
                struct semid_ds buf;

                res = syscall(SYS_semctl, semid_p, semnum_p, cmd_p, &buf);
                buf_neutral->sem_perm.key = buf.sem_perm.__key;
                buf_neutral->sem_perm.uid = buf.sem_perm.uid;
                buf_neutral->sem_perm.gid = buf.sem_perm.gid;
                buf_neutral->sem_perm.cuid = buf.sem_perm.cuid;
                buf_neutral->sem_perm.cgid = buf.sem_perm.cgid;
                buf_neutral->sem_perm.mode = buf.sem_perm.mode;
                buf_neutral->sem_perm.seq = buf.sem_perm.__seq;
                buf_neutral->sem_otime = buf.sem_otime;
                buf_neutral->sem_ctime = buf.sem_ctime;
                buf_neutral->sem_nsems = buf.sem_nsems;
            }
            break;
        default:
            res = syscall(SYS_semctl, semid_p, semnum_p, cmd_p, arg0_p);
    }

    return res;
}

static long epoll_pwait_neutral(uint64_t epfd_p, uint64_t events_p, uint64_t maxevents_p, uint64_t timeout_p, uint64_t sigmask_p, uint64_t sigmask_size_p)
{
    long res;
    int epfd = (int) epfd_p;
    struct neutral_epoll_event *events_guest = (struct neutral_epoll_event *) events_p;
    int maxevents = (int) maxevents_p;
    int timeout = (int) timeout_p;
    sigset_t *sigmask = (sigset_t *) sigmask_p;
    int sigmask_size = (int) sigmask_size_p;
    struct epoll_event *events = (struct epoll_event *) alloca(maxevents * sizeof(struct epoll_event));
    int i;

    res = syscall(SYS_epoll_pwait, epfd, events, maxevents, timeout, sigmask, sigmask_size);
    for(i = 0; i < maxevents; i++) {
        events_guest[i].events = events[i].events;
        events_guest[i].data = events[i].data.u64;
    }

    return res;
}

long syscall_neutral_64(Sysnum no, uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5)
{
    long res = -ENOSYS;

    switch(no) {
        case PR_accept:
            res = syscall(SYS_accept, p0, p1, p2);
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
        case PR_chown:
            res = syscall(SYS_chown, p0, p1, p2);
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
        case PR_epoll_pwait:
            res = epoll_pwait_neutral(p0, p1, p2, p3, p4, p5);
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
        case PR_fadvise64:
            res = syscall(SYS_fadvise64, p0, p1, p2, p3);
            break;
        case PR_fallocate:
            res = syscall(SYS_fallocate, p0, p1, p2, p3);
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
        case PR_fchown:
            res = syscall(SYS_fchown, p0, p1, p2);
            break;
        case PR_fchownat:
            res = syscall(SYS_fchownat, p0, p1, p2, p3, p4);
            break;
        case PR_fcntl:
            res = fcntl_neutral(p0, p1, p2);
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
        case PR_futex:
            res = syscall(SYS_futex, p0, p1, p2, p3, p4, p5);
            break;
        case PR_fstat:
            res = syscall_x86_64_fstat(p0, p1);
            break;
        case PR_fstat64:
            res = syscall_x86_64_fstat64(p0, p1);
            break;
        case PR_fstatat64:
            res = syscall_x86_64_fstatat64(p0, p1, p2, p3);
            break;
        case PR_fstatfs:
            res = syscall(SYS_fstatfs, p0, p1);
            break;
        case PR_fsync:
            res = syscall(SYS_fsync, p0);
            break;
        case PR_futimesat:
            res = syscall(SYS_futimesat, p0, p1, p2);
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
        case PR_getitimer:
            res = syscall(SYS_getitimer, p0, p1);
            break;
        case PR_getgroups:
            res = syscall(SYS_getgroups, p0, p1);
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
        case PR_getresgid:
            res = syscall(SYS_getresgid, p0, p1, p2);
            break;
        case PR_getresuid:
            res = syscall(SYS_getresuid, p0, p1, p2);
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
        case PR_getrandom:
            res = syscall(SYS_getrandom, p0, p1, p2);
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
            res = syscall(SYS_lchown, p0, p1, p2);
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
        case PR_llistxattr:
            res = syscall(SYS_llistxattr, p0, p1, p2);
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
            res = syscall(SYS_msgctl, p0, p1, p2);
            break;
        case PR_msgget:
            res = syscall(SYS_msgget, p0, p1);
            break;
        case PR_msgsnd:
            res = syscall(SYS_msgsnd, p0, p1, p2, p3);
            break;
        case PR_msgrcv:
            res = syscall(SYS_msgrcv, p0, p1, p2, p3, p4);
            break;
        case PR_msync:
            res = syscall(SYS_msync, p0, p1, p2);
            break;
        case PR_munlock:
            res = syscall(SYS_munlock, p0, p1);
            break;
        case PR_name_to_handle_at:
            res = syscall(SYS_name_to_handle_at, p0, p1, p2, p3, p4);
            break;
        case PR_nanosleep:
            res = syscall(SYS_nanosleep, p0, p1);
            break;
        case PR_newfstatat:
            res = syscall_x86_64_newfstatat(p0, p1, p2, p3);
            break;
        case PR_open:
            res = syscall(SYS_open, p0, neutralToX86Flags(p1), p2);
            break;
        case PR_openat:
            res = syscall(SYS_openat, p0, p1, neutralToX86Flags(p2), p3);
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
            res = syscall(SYS_pread64, p0, p1, p2, p3);
            break;
        case PR_prlimit64:
            res = syscall(SYS_prlimit64, p0, p1, p2, p3);
            break;
        case PR_process_vm_readv:
            res = syscall(SYS_process_vm_readv, p0, p1, p2, p3, p4, p5);
            break;
        case PR_process_vm_writev:
            res = syscall(SYS_process_vm_writev, p0, p1, p2, p3, p4, p5);
            break;
        case PR_pselect6:
            res = syscall(SYS_pselect6, p0, p1, p2, p3, p4, p5);
            break;
        case PR_pwrite64:
            res = syscall(SYS_pwrite64, p0, p1, p2, p3);
            break;
        case PR_quotactl:
            res = syscall(SYS_quotactl, p0, p1, p2, p3);
            break;
        case PR_read:
            res = syscall(SYS_read, p0, p1, p2);
            break;
        case PR_readahead:
            res = syscall(SYS_readahead, p0, p1, p2);
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
            res = syscall(SYS_recvfrom, p0, p1, p2, p3, 0, 0);
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
        case PR_removexattr:
            res = syscall(SYS_removexattr, p0, p1);
            break;
        case PR_rename:
            res = syscall(SYS_rename, p0, p1);
            break;
        case PR_renameat:
            res = syscall(SYS_renameat, p0, p1, p2, p3);
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
        case PR_rt_sigqueueinfo:
            res = syscall(SYS_rt_sigqueueinfo, p0, p1, p2);
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
            res = semctl_neutral(p0, p1, p2, p3);
            break;
        case PR_semget:
            res = syscall(SYS_semget, p0, p1, p2);
            break;
        case PR_semop:
            res = syscall(SYS_semop, p0, p1, p2);
            break;
        case PR_send:
            res = syscall(SYS_sendto, p0, p1, p2, p3, 0, 0);
            break;
        case PR_sendto:
            res = syscall(SYS_sendto, p0, p1, p2, p3, p4, p5);
            break;
        case PR_sendfile:
            res = syscall(SYS_sendfile, p0, p1, p2, p3);
            break;
        case PR_sendmmsg:
            res = syscall(SYS_sendmmsg, p0, p1, p2, p3);
            break;
        case PR_sendmsg:
            res = syscall(SYS_sendmsg, p0, p1, p2);
            break;
        case PR_select:
            res = syscall(SYS_select, p0, p1, p2, p3, p4);
            break;
        case PR_set_tid_address:
            res = syscall(SYS_set_tid_address, p0);
            break;
        case PR_setfsgid:
            res = syscall(SYS_setfsgid, p0);
            break;
        case PR_setfsuid:
            res = syscall(SYS_setfsuid, p0);
            break;
        case PR_setgroups:
            res = syscall(SYS_setgroups, p0, p1);
            break;
        case PR_setgid:
            res = syscall(SYS_setgid, p0);
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
        case PR_setresgid:
            res = syscall(SYS_setresgid, p0, p1, p2);
            break;
        case PR_setresgid32:
            res = syscall(SYS_setresgid, p0, p1, p2);
            break;
        case PR_setresuid:
            res = syscall(SYS_setresuid, p0, p1, p2);
            break;
        case PR_setresuid32:
            res = syscall(SYS_setresuid, p0, p1, p2);
            break;
        case PR_setreuid:
            res = syscall(SYS_setreuid, p0, p1);
            break;
        case PR_setregid:
            res = syscall(SYS_setregid, p0, p1);
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
        case PR_setxattr:
            res = syscall(SYS_setxattr, p0, p1, p2, p3, p4);
            break;
        case PR_shmctl:
            res = syscall(SYS_shmctl, p0, p1, p2);
            break;
        case PR_shmget:
            res = syscall(SYS_shmget, p0, p1, p2);
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
            res = syscall_x86_64_stat64(p0, p1);
            break;
        case PR_statfs:
            res = syscall(SYS_statfs, p0, p1);
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
            res = syscall(SYS_sync_file_range, p0, p1, p2, p3);
            break;
        case PR_syncfs:
            res = syscall(SYS_syncfs, p0);
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
        case PR_umask:
            res = syscall(SYS_umask, p0);
            break;
        case PR_uname:
            res = syscall(SYS_uname, p0);
            break;
        case PR_unlink:
            res = syscall(SYS_unlink, p0);
            break;
        case PR_unlinkat:
            res = syscall(SYS_unlinkat, p0, p1, p2);
            break;
        case PR_unshare:
            res = syscall(SYS_unshare, p0);
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
            fatal("syscall_neutral_64: unsupported neutral syscall %d\n", no);
    }

    return res;
}
