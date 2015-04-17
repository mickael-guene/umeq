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
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <errno.h>
#include <poll.h>
#include <sched.h>

#include "sysnum.h"
#include "syscall32_64_types.h"
#include "syscall32_64_private.h"
#include "syscall32_64.h"
#include "runtime.h"
#include "cache.h"

#define IS_NULL(px,type) ((px)?(type *)g_2_h((px)):NULL)

int syscall32_64(Sysnum no, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5)
{
    int res = ENOSYS;

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
        case PR_exit_group:
            res = syscall(SYS_exit_group, (int)p0);
            break;
        case PR_write:
            res = syscall(SYS_write, (int)p0, (void *)g_2_h(p1), (size_t)p2);
            break;
        case PR_readlink:
            res = syscall(SYS_readlink, (const char *)g_2_h(p0), (char *)g_2_h(p1), (size_t)p2);
            break;
        case PR_fstat64:
            res = fstat64_s3264(p0, p1);
            break;
        case PR_mprotect:
            res = syscall(SYS_mprotect, (void *) g_2_h(p0), (size_t) p1, (int) p2);
            break;
        case PR_fcntl64:
            res = fnctl64_s3264(p0,p1,p2);
            break;
        case PR_ioctl:
            /* FIXME: need specific version to offset param according to ioctl */
            res = syscall(SYS_ioctl, (int) p0, (unsigned long) p1, (char *) g_2_h(p2));
            break;
        case PR_getdents64:
            res = getdents64_s3264(p0,p1,p2);
            break;
        case PR_lstat64:
            res = lstat64_s3264(p0,p1);
            break;
        case PR_socket:
            res = syscall(SYS_socket, (int)p0, (int)(p1), (int)p2);
            break;
        case PR_connect:
            res = syscall(SYS_connect, (int)p0, (const struct sockaddr *) g_2_h(p1), (int)p2);
            break;
        case PR__llseek:
            res = llseek_s3264(p0,p1,p2,p3,p4);
            break;
        case PR_eventfd:
            res = syscall(SYS_eventfd, (int)p0, (int)(p1));
            break;
        case PR_clock_gettime:
            res = clock_gettime_s3264(p0, p1);
            break;
        case PR_stat64:
            res = stat64_s3264(p0,p1);
            break;
        case PR_rt_sigprocmask:
            res = syscall(SYS_rt_sigprocmask, (int)p0, p1?(const sigset_t *) g_2_h(p1):NULL,
                                              p2?(sigset_t *) g_2_h(p2):NULL, (size_t) p3);
            break;
        case PR_getuid32:
            res = syscall(SYS_getuid);
            break;
        case PR_getgid32:
            res = syscall(SYS_getgid);
            break;
        case PR_geteuid32:
            res = syscall(SYS_geteuid);
            break;
        case PR_getegid32:
            res = syscall(SYS_getegid);
            break;
        case PR_gettimeofday:
            res = gettimeofday_s3264(p0,p1);
            break;
        case PR_getpid:
            res = syscall(SYS_getpid);
            break;
        case PR_getppid:
            res = syscall(SYS_getppid);
            break;
        case PR_getpgrp:
            res = syscall(SYS_getpgrp);
            break;
        case PR_dup:
            res = syscall(SYS_dup, (int)p0);
            break;
        case PR_ugetrlimit:
            res = ugetrlimit_s3264(p0,p1);
            break;
        case PR_dup2:
            res = syscall(SYS_dup2, (int)p0, (int)p1);
            break;
        case PR_setpgid:
            res = syscall(SYS_setpgid, (pid_t)p0, (pid_t)p1);
            break;
        case PR_pipe:
            res = syscall(SYS_pipe, (int *) g_2_h(p0));
            break;
        case PR_wait4:
            res = wait4_s3264(p0,p1,p2,p3);
            break;
        case PR_execve:
            res = execve_s3264(p0,p1,p2);
            break;
        case PR_chdir:
            res = syscall(SYS_chdir, (const char *) g_2_h(p0));
            break;
        case PR_prlimit64:
            res = prlimit64_s3264(p0,p1,p2,p3);
            break;
        case PR_lseek:
            res = syscall(SYS_lseek, (int)p0, (off_t)p1, (int)p2);
            break;
        case PR_getcwd:
            res = syscall(SYS_getcwd, (char *) g_2_h(p0), (size_t) p1);
            break;
        case PR_writev:
            res = writev_s3264(p0,p1,p2);
            break;
        case PR_set_tid_address:
            res = syscall(SYS_set_tid_address, (int *) g_2_h(p0));
            break;
        case PR_set_robust_list:
            /* FIXME: implement this correctly */
            res = -EPERM;
            break;
        case PR_futex:
            res = futex_s3264(p0,p1,p2,p3,p4,p5);
            break;
        case PR_statfs64:
            res = statfs64_s3264(p0,p1,p2);
            break;
        case PR_lgetxattr:
            res = syscall(SYS_lgetxattr, (const char *) g_2_h(p0), (const char *) g_2_h(p1), (void *) g_2_h(p2), (size_t) p3);
            break;
        case PR_faccessat:
            res = syscall(SYS_faccessat, (int) p0, (const char *) g_2_h(p1), (int) p2, (int) p3);
            break;
        case PR_fsync:
            res = syscall(SYS_fsync, (int) p0);
            break;
        case PR_getpeername:
            res = syscall(SYS_getpeername, (int) p0, (struct sockaddr *) g_2_h(p1), (socklen_t *) g_2_h(p2));
            break;
        case PR_vfork:
            /* implement with fork to avoid sync problem but semantic is not fully preserved ... */
            res = syscall(SYS_fork);
            break;
        case PR_getrusage:
            res = getrusage_s3264(p0, p1);
            break;
        case PR_unlink:
            res = syscall(SYS_unlink, (const char *) g_2_h(p0));
            break;
        case PR_umask:
            res = syscall(SYS_umask, (mode_t) p0);
            break;
        case PR_chmod:
            res = syscall(SYS_chmod, (const char *) g_2_h(p0), (mode_t) p1);
            break;
        case PR_fstatat64:
            res = fstatat64_s3264(p0,p1,p2,p3);
            break;
        case PR_unlinkat:
            res = syscall(SYS_unlinkat, (int) p0, (const char *) g_2_h(p1), (int) p2);
            break;
        case PR_socketpair:
            res = syscall(SYS_socketpair, (int) p0, (int) p1, (int) p2, (int *) g_2_h(p3));
            break;
        case PR_poll:
            res = syscall(SYS_poll, (struct pollfd *) g_2_h(p0), (nfds_t) p1, (int) p2);
            break;
        case PR_kill:
            res = syscall(SYS_kill, (pid_t) p0, (int) p1);
            break;
        case PR_personality:
            res = syscall(SYS_personality, (unsigned long) p0);
            break;
        case PR_rt_sigsuspend:
            res = syscall(SYS_rt_sigsuspend, (const sigset_t *) g_2_h(p0), (size_t) p1);
            break;
        case PR_madvise:
            res = syscall(SYS_madvise, (void *) g_2_h(p0), (size_t) p1, (int) p2);
            break;
        case PR_setreuid32:
            res = syscall(SYS_setreuid, (uid_t) p0, (uid_t) p1);
            break;
        case PR_rename:
            res = syscall(SYS_rename, (const char *) g_2_h(p0), (const char *) g_2_h(p1));
            break;
        case PR__newselect:
            res = newselect_s3264(p0,p1,p2,p3,p4);
            break;
        case PR_bind:
            res= syscall(SYS_bind, (int) p0, (const struct sockaddr *) g_2_h(p1), (socklen_t) p2);
            break;
        case PR_getsockname:
            res = syscall(SYS_getsockname, (int) p0, (const struct sockaddr *) g_2_h(p1), (socklen_t *) g_2_h(p2));
            break;
        case PR_sendto:
            res = syscall(SYS_sendto, (int) p0, (const void *) g_2_h(p1), (size_t) p2, (int) p3, (const struct sockaddr *) g_2_h(p4), (socklen_t) p5);
            break;
        case PR_send:
            res = syscall(SYS_sendto, (int) p0, (const void *) g_2_h(p1), (size_t) p2, (int) p3, 0, 0);
            break;
        case PR_recvmsg:
            res = recvmsg_s3264(p0,p1,p2);
            break;
        case PR_recvfrom:
            res = syscall(SYS_recvfrom, (int) p0, (void *) g_2_h(p1), (size_t) p2, (int) p3,
                                        p4?(struct sockaddr *) g_2_h(p4):NULL, p5?(socklen_t *) g_2_h(p5):NULL);
            break;
        case PR_getsockopt:
            res = syscall(SYS_getsockopt, (int) p0, (int) p1, (int) p2,
                                          p3?(void *) g_2_h(p3):NULL, p4?(socklen_t *) g_2_h(p4):NULL);
            break;
        case PR_utimes:
            res = utimes_s3264(p0,p1);
            break;
        case PR_fchmod:
            res = syscall(SYS_fchmod, (int) p0, (mode_t) p1);
            break;
        case PR_msync:
            res = syscall(SYS_msync, (void *) g_2_h(p0), (size_t) p1, (int) p2);
            break;
        case PR_flock:
            res = syscall(SYS_flock, (int) p0, (int) p1);
            break;
        case PR_chown32:
            res = syscall(SYS_chown, (const char *) g_2_h(p0), (uid_t) p1, (gid_t) p2);
            break;
        case PR_statfs:
            res = statfs_s3264(p0, p1);
            break;
        case PR_pselect6:
            res = pselect6_s3264(p0,p1,p2,p3,p4,p5);
            break;
        case PR_setsid:
            res = syscall(SYS_setsid);
            break;
        case PR_rmdir:
            res = syscall(SYS_rmdir, (const char *) g_2_h(p0));
            break;
        case PR_mkdir:
            res = syscall(SYS_mkdir, (const char *) g_2_h(p0), (mode_t) p1);
            break;
        case PR_mkdirat:
            res = syscall(SYS_mkdirat, (int) p0, (const char *) g_2_h(p1), (mode_t) p2);
            break;
        case PR_utimensat:
            res = utimensat_s3264(p0,p1,p2,p3);
            break;
        case PR_fchown32:
            res = syscall(SYS_fchown, (int) p0, (uid_t) p1, (gid_t) p2);
            break;
        case PR_fchownat:
            res = syscall(SYS_fchownat, (int) p0, (const char *) g_2_h(p1), (uid_t) p2, (gid_t) p3, (int) p4);
            break;
        case PR_link:
            res = syscall(SYS_link, (const char *) g_2_h(p0), (const char *) g_2_h(p1));
            break;
        case PR_symlink:
            res = syscall(SYS_symlink, (const char *) g_2_h(p0), (const char *) g_2_h(p1));
            break;
        case PR_lchown32:
            res = syscall(SYS_lchown, (const char *) g_2_h(p0), (uid_t) p1, (gid_t) p2);
            break;
        case PR_nanosleep:
            res = nanosleep_s3264(p0,p1);
            break;
        case PR_sysinfo:
            res = sysinfo_s3264(p0);
            break;
        case PR_fchdir:
            res = syscall(SYS_fchdir, (int) p0);
            break;
        case PR_clock_getres:
            res = clock_getres_s3264(p0,p1);
            break;
        case PR_recv:
            res = syscall(SYS_recvfrom, (int) p0, (void *) g_2_h(p1), (size_t) p2, (int) p3, 0, 0);
            break;
        case PR_fchmodat:
            res = syscall(SYS_fchmodat, (int) p0, (const char *) g_2_h(p1), (mode_t) p2, (int) p3);
            break;
        case PR_fsetxattr:
            res = syscall(SYS_fsetxattr, (int) p0, (const char *) g_2_h(p1), (const void *) g_2_h(p2), (size_t) p3, (int) p4);
            break;
        case PR_getgroups32:
            res = syscall(SYS_getgroups, (int) p0, (gid_t *) g_2_h(p1));
            break;
        case PR_sched_getaffinity:
            res = syscall(SYS_sched_getaffinity, (pid_t) p0, (size_t) p1, (cpu_set_t *) g_2_h(p2));
            break;
        case PR_getdents:
            res = getdents_s3264(p0,p1,p2);
            break;
        case PR_setitimer:
            res = setitimer_s3264(p0,p1,p2);
            break;
        case PR_fdatasync:
            res = syscall(SYS_fdatasync, (int) p0);
            break;
        case PR_mknod:
            res = syscall(SYS_mknod, (const char *) g_2_h(p0), (mode_t) p1, (dev_t) p2);
            break;
        case PR_timer_create:
            res = timer_create_s3264(p0, p1, p2);
            break;
        case PR_fstatfs64:
            res = fstatfs64_s3264(p0,p1,p2);
            break;
        case PR_inotify_init:
            res = syscall(SYS_inotify_init);
            break;
        case PR_linkat:
            res = syscall(SYS_linkat, (int) p0, (const char *) g_2_h(p1), (int) p2, (const char *) g_2_h(p3), (int) p4);
            break;
        case PR_getpriority:
            res = syscall(SYS_getpriority, (int) p0, (id_t) p1);
            break;
        case PR_timer_settime:
            res = timer_settime_s3264(p0,p1,p2,p3);
            break;
        case PR_inotify_add_watch:
            res = syscall(SYS_inotify_add_watch, (int) p0, (const char *) g_2_h(p1), (uint32_t) p2);
            break;
        case PR_setpriority:
            res = syscall(SYS_setpriority, (int) p0, (id_t) p1, (int) p2);
            break;
        case PR_prctl:
            res = syscall(SYS_prctl, (int) p0, (unsigned long) p1, (unsigned long) p2, (unsigned long) p3, (unsigned long) p4);
            break;
        case PR_tgkill:
            res = syscall(SYS_tgkill, (int) p0, (int) p1, (int) p2);
            break;
        case PR_fstatfs:
            res = fstatfs_s3264(p0,p1);
            break;
        case PR_getresuid32:
            res = syscall(SYS_getresuid, (uid_t *) g_2_h(p0), (uid_t *) g_2_h(p1), (uid_t *) g_2_h(p2));
            break;
        case PR_getresgid32:
            res = syscall(SYS_getresgid, (gid_t *) g_2_h(p0), (gid_t *) g_2_h(p1), (gid_t *) g_2_h(p2));
            break;
        case PR_sendmsg:
            res = sendmsg_s3264(p0,p1,p2);
            break;
        case PR_gettid:
            res = syscall(SYS_gettid);
            break;
        case PR_tkill:
            res = syscall(SYS_tkill, (int) p0, (int) p1);
            break;
        case PR_shutdown:
            res = syscall(SYS_shutdown, (int) p0, (int) p1);
            break;
        case PR_rt_sigtimedwait:
            res = rt_sigtimedwait_s3264(p0,p1,p2);
            break;
        case PR_eventfd2:
            res = syscall(SYS_eventfd2, (unsigned int) p0, (int) p1);
            break;
        case PR_pipe2:
            res = syscall(SYS_pipe2, (int *) g_2_h(p0), (int) p1);
            break;
        case PR_shmget:
            res = syscall(SYS_shmget, (key_t) p0, (size_t) p1, (int) p2);
            break;
        case PR_shmat:
            res = shmat_s3264(p0,p1,p2);
            break;
        case PR_shmctl:
            res = shmctl_s3264(p0,p1,p2);
            break;
        case PR_shmdt:
            res = syscall(SYS_shmdt, (void *) g_2_h(p0));
            break;
        case PR_sched_yield:
            res = syscall(SYS_sched_yield);
            break;
        case PR_inotify_init1:
            res = syscall(SYS_inotify_init1, (int) p0);
            break;
        case PR_sched_get_priority_max:
            res = syscall(SYS_sched_get_priority_max, (int) p0);
            break;
        case PR_sched_get_priority_min:
            res = syscall(SYS_sched_get_priority_min, (int) p0);
            break;
        case PR_sched_setscheduler:
            res = syscall(SYS_sched_setscheduler, (pid_t) p0, (int) p1, (const struct sched_param *) g_2_h(p2));
            break;
        case PR_semget:
            res = syscall(SYS_semget, (key_t) p0, (int) p1, (int) p2);
            break;
        case PR_semctl:
            res = semctl_s3264(p0, p1, p2, p3, p4, p5);
            break;
        case PR_semop:
            res = semop_s3264(p0,p1,p2);
            break;
        case PR_mlock:
            res = syscall(SYS_mlock, (const void *) g_2_h(p0), (size_t) p1);
            break;
        case PR_clock_nanosleep:
            res = clock_nanosleep_s3264(p0,p1,p2,p3);
            break;
        case PR_inotify_rm_watch:
            res = syscall(SYS_inotify_rm_watch, (int) p0, (int) p1);
            break;
        case PR_fgetxattr:
            res = syscall(SYS_fgetxattr, (int) p0, (const char *) g_2_h(p1), (void *) g_2_h(p2), (size_t) p3);
            break;
        case PR_times:
            res = times_s3264(p0);
            break;
        case PR_symlinkat:
            res = syscall(SYS_symlinkat, (const char *) g_2_h(p0), (int) p1, (const char *) g_2_h(p2));
            break;
        case PR_readlinkat:
            res = syscall(SYS_readlinkat, (int) p0, (const char *) g_2_h(p1), (char *) g_2_h(p2), (size_t) p3);
            break;
        case PR_flistxattr:
            res = syscall(SYS_flistxattr, (int) p0, (char *) g_2_h(p1), (size_t) p2);
            break;
        case PR_epoll_create:
            res = syscall(SYS_epoll_create, (int) p0);
            break;
        case PR_epoll_ctl:
            res = epoll_ctl_s3264(p0, p1, p2, p3);
            break;
        case PR_epoll_wait:
            res = epoll_wait_s3264(p0 , p1, p2, p3);
            break;
        case PR_setsockopt:
            res = syscall(SYS_setsockopt, (int) p0, (int) p1, (int) p2, (void *) g_2_h(p3), (socklen_t) p4);
            break;
        case PR_setresgid32:
            res = syscall(SYS_setresgid, (gid_t) p0, (gid_t) p1, (gid_t) p2);
            break;
        case PR_setregid32:
            res = syscall(SYS_setregid, (gid_t) p0, (gid_t) p1);
            break;
        case PR_setresuid32:
            res = syscall(SYS_setresuid, (uid_t) p0, (uid_t) p1, (uid_t) p2);
            break;
        case PR_getxattr:
            res = syscall(SYS_getxattr, (char *) g_2_h(p0), (char *) g_2_h(p1), (void *) g_2_h(p2), (size_t) p3);
            break;
        case PR_dup3:
            res = syscall(SYS_dup3, (int) p0, (int) p1, (int) p2);
            break;
        case PR_sendmmsg:
            res = sendmmsg_s3264(p0, p1, p2, p3);
            break;
        case PR_fallocate:
            res = syscall(SYS_fallocate, (int) p0, (int) p1, (off_t) p2, (off_t) p3);
            break;
        case PR_capget:
            res = syscall(SYS_capget, (void *) g_2_h(p0), p1?(void *) g_2_h(p1):NULL);
            break;
        case PR_truncate64:
            res = syscall(SYS_truncate, (char *) g_2_h(p0), (loff_t) p1);
            break;
        case PR_listen:
            res = syscall(SYS_listen, (int) p0, (int) p1);
            break;
        case PR_accept:
            res = syscall(SYS_accept, (int) p0, p1?(struct sockaddr *)g_2_h(p1):NULL, p2?(socklen_t *)g_2_h(p2):NULL);
            break;
        case PR_msgget:
            res = syscall(SYS_msgget, (key_t) p0, (int) p1);
            break;
        case PR_pwrite64:
            res = syscall(SYS_pwrite64, (int) p0, (void *) g_2_h(p1), (size_t) p2, (off_t) p3);
            break;
        case PR_getitimer:
            res = getitimer_s3264(p0, p1);
            break;
        case PR_getpgid:
            res = syscall(SYS_getpgid, (pid_t) p0);
            break;
        case PR_rt_sigpending:
            res = syscall(SYS_rt_sigpending, (sigset_t *) g_2_h(p0), (size_t) p1);
            break;
        case PR_msgsnd:
            res = msgsnd_s3264(p0, p1, p2, p3);
            break;
        case PR_msgrcv:
            res = msgrcv_s3264(p0, p1, p2, p3, p4);
            break;
        case PR_msgctl:
            res = msgctl_s3264(p0, p1, p2);
            break;
        case PR_sched_setaffinity:
            res = syscall(SYS_sched_setaffinity, (pid_t) p0, (size_t) p1, (cpu_set_t *) g_2_h(p2));
            break;
        case PR_setgid32:
            res = syscall(SYS_setgid, (gid_t) p0);
            break;
        case PR_setuid32:
            res = syscall(SYS_setuid, (uid_t) p0);
            break;
        case PR_stat:
            res = stat_s3264(p0, p1);
            break;
        case PR_fstat:
            res = fstat_s3264(p0, p1);
            break;
        case PR_fcntl:
            res = fnctl_s3264(p0,p1,p2);
            break;
        case PR_sched_getparam:
            res = syscall(SYS_sched_getparam, (pid_t) p0, (struct sched_param *) g_2_h(p1));
            break;
        case PR_sched_getscheduler:
            res = syscall(SYS_sched_getscheduler, (pid_t) p0);
            break;
        case PR_sched_rr_get_interval:
            res = sched_rr_get_interval_s3264(p0, p1);
            break;
        case PR_timer_gettime:
            res = timer_gettime_s3264(p0, p1);
            break;
        case PR_timer_getoverrun:
            /* see comment in timer_s3264 */
            res = syscall(SYS_timer_getoverrun, p0);
            break;
        case PR_timer_delete:
            /* see comment in timer_s3264 */
            res = syscall(SYS_timer_delete, p0);
            break;
        case PR_clock_settime:
            res = clock_settime_s3264(p0, p1);
            break;
        case PR_mq_open:
            res = mq_open_s3264(p0, p1, p2, p3);
            break;
        case PR_mq_unlink:
            res = syscall(SYS_mq_unlink, (char *) g_2_h(p0));
            break;
        case PR_sched_setparam:
            res = syscall(SYS_sched_setparam, (pid_t) p0, (struct sched_param *) g_2_h(p1));
            break;
        case PR_mq_timedsend:
            res = mq_timedsend_s3264(p0, p1, p2, p3, p4);
            break;
        case PR_mq_timedreceive:
            res = mq_timedreceive_s3264(p0, p1, p2, p3, p4);
            break;
        case PR_mq_notify:
            res = mq_notify_s3264(p0, p1);
            break;
        case PR_mq_getsetattr:
            res = mq_getsetattr_s3264(p0, p1, p2);
            break;
        case PR_setrlimit:
            res = setrlimit_s3264(p0, p1);
            break;
        case PR_ftruncate:
            res = syscall(SYS_ftruncate, (int) p0, (off_t) p1);
            break;
        case PR_getsid:
            res = syscall(SYS_getsid, (pid_t) p0);
            break;
        case PR_munlock:
            res = syscall(SYS_munlock, (void *) g_2_h(p0), (size_t) p1);
            break;
        case PR_capset:
            res = syscall(SYS_capset, (void *) g_2_h(p0), p1?(void *) g_2_h(p1):NULL);
            break;
        case PR_setgroups32:
            res = syscall(SYS_setgroups, (int) p0, (gid_t *) g_2_h(p1));
            break;
        case PR_getuid:
            res = syscall(SYS_getuid);
            break;
        case PR_pause:
            res = syscall(SYS_pause);
            break;
        default:
            fatal("syscall_32_to_64: unsupported neutral syscall %d\n", no);
    }

    return res;
}
