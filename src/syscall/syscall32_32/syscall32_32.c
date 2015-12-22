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
#include <poll.h>
#include <signal.h>
#include <sched.h>
#include <mqueue.h>

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
        case PR_getppid:
            res = syscall(SYS_getppid);
            break;
        case PR_getpgrp:
            res = syscall(SYS_getpgrp);
            break;
        case PR_dup:
            res = syscall(SYS_dup, (int)p0);
            break;
        case PR_dup2:
            res = syscall(SYS_dup2, (int)p0, (int)p1);
            break;
        case PR_setpgid:
            res = syscall(SYS_setpgid, (pid_t)p0, (pid_t)p1);
            break;
        case PR_faccessat:
            res = syscall(SYS_faccessat, (int) p0, (const char *) g_2_h(p1), (int) p2, (int) p3);
            break;
        case PR_pipe:
            res = syscall(SYS_pipe, (int *) g_2_h(p0));
            break;
        case PR_tgkill:
            res = syscall(SYS_tgkill, (int) p0, (int) p1, (int) p2);
            break;
        case PR_gettid:
            res = syscall(SYS_gettid);
            break;
        case PR_wait4:
            res = syscall(SYS_wait4, (pid_t) p0, IS_NULL(p1, int), (int) p2, IS_NULL(p3, struct rusage));
            break;
        case PR_execve:
            res = execve_s3232(p0,p1,p2);
            break;
        case PR_setitimer:
            res = syscall(SYS_setitimer, (int) p0, (const struct itimerval *) g_2_h(p1), IS_NULL(p2, struct itimerval));
            break;
        case PR_nanosleep:
            res = syscall(SYS_nanosleep, (const struct timespec *) g_2_h(p0), IS_NULL(p1, struct timespec *));
            break;
        case PR_getrusage:
            res = syscall(SYS_getrusage, (int) p0, (struct rusage *) g_2_h(p1));
            break;
        case PR_utimensat:
            res = syscall(SYS_utimensat, (int) p0, IS_NULL(p1, const char), IS_NULL(p2, const struct timespec *), (int) p3);
            break;
        case PR_fstatat64:
            res = fstatat64_s3232(p0,p1,p2,p3);
            break;
        case PR_unlinkat:
            res = syscall(SYS_unlinkat, (int) p0, (const char *) g_2_h(p1), (int) p2);
            break;
        case PR_vfork:
            /* implement with fork to avoid sync problem but semantic is not fully preserved ... */
            res = syscall(SYS_fork);
            break;
        case PR_madvise:
            res = syscall(SYS_madvise, (void *) g_2_h(p0), (size_t) p1, (int) p2);
            break;
        case PR_clock_getres:
            res = syscall(SYS_clock_getres, (clockid_t) p0, IS_NULL(p1, struct timespec));
            break;
        case PR_fstatfs:
            /* FIXME: to be check */
            res = syscall(SYS_fstatfs, (int) p0, (struct statfs *) g_2_h(p1));
            break;
        case PR_getresuid32:
            res = syscall(SYS_getresuid, (uid_t *) g_2_h(p0), (uid_t *) g_2_h(p1), (uid_t *) g_2_h(p2));
            break;
        case PR_getresgid32:
            res = syscall(SYS_getresgid, (gid_t *) g_2_h(p0), (gid_t *) g_2_h(p1), (gid_t *) g_2_h(p2));
            break;
        case PR_fstatfs64:
            /* FIXME: to be check */
            res = syscall(SYS_fstatfs64, (int) p0, (struct statfs *) g_2_h(p1));
            break;
        case PR_getpeername:
            {
                unsigned long args[3];

                args[0] = (int) p0;
                args[1] = (unsigned long)(const struct sockaddr *) g_2_h(p1);
                args[2] = (socklen_t) p2;
                res = syscall(SYS_socketcall, 7 /*sys_getpeername*/, args);
            }
            break;
        case PR_poll:
            res = syscall(SYS_poll, (struct pollfd *) g_2_h(p0), (nfds_t) p1, (int) p2);
            break;
        case PR_recv:
            {
                unsigned long args[4];

                args[0] = (int) p0;
                args[1] = (unsigned long)(void *) g_2_h(p1);
                args[2] = (size_t) p2;
                args[3] = (int) p3;
                res = syscall(SYS_socketcall, 10 /*sys_recv*/, args);
            }
            break;
        case PR_shutdown:
            {
                unsigned long args[2];

                args[0] = (int) p0;
                args[1] = (int) p1;
                res = syscall(SYS_socketcall, 13 /*sys_shutdown*/, args);
            }
            break;
        case PR_eventfd2:
            res = syscall(SYS_eventfd2, (unsigned int) p0, (int) p1);
            break;
        case PR_pipe2:
            res = syscall(SYS_pipe2, (int *) g_2_h(p0), (int) p1);
            break;
        case PR_getdents:
            res = syscall(SYS_getdents, (unsigned int) p0, (struct linux_dirent *)g_2_h(p1), (unsigned int) p2);
            break;
        case PR_getsockname:
            {
                unsigned long args[3];

                args[0] = (int) p0;
                args[1] = (unsigned long)(struct sockaddr *) g_2_h(p1);
                args[2] = (unsigned long)(socklen_t *) g_2_h(p2);
                res = syscall(SYS_socketcall, 6 /*sys_getsockname*/, args);
            }
            break;
        case PR_statfs:
            /* FIXME: to be check */
            res = syscall(SYS_statfs, (const char *)g_2_h(p0), (struct statfs *) g_2_h(p1));
            break;
        case PR_rt_sigtimedwait:
            /* FIXME: to be check */
            res = syscall(SYS_rt_sigtimedwait, (sigset_t *) g_2_h(p0), IS_NULL(p1, siginfo_t), IS_NULL(p2, struct timespec), (size_t) p3);
            break;
        case PR_shmget:
            res = syscall(SYS_ipc, 23/*IPCOP_shmget*/, (key_t) p0, (size_t) p1, (int) p2);
            break;
        case PR_shmctl:
            res = syscall(SYS_ipc, 24/*IPCOP_shmctl*/, (int) p0, (int) p1, 0, IS_NULL(p2, struct shmid_ds), 0, 0);
            break;
        case PR_setreuid32:
            res = syscall(SYS_setreuid, (uid_t) p0, (uid_t) p1);
            break;
        case PR_socketpair:
            {
                unsigned long args[4];

                args[0] = (int) p0;
                args[1] = (int) p1;
                args[2] = (int) p2;
                args[3] = (unsigned long)(int *) g_2_h(p3);
                res = syscall(SYS_socketcall, 8/*sys_socketpair*/, args);
            }
            break;
        case PR_personality:
            res = syscall(SYS_personality, (unsigned long) p0);
            break;
        case PR_rt_sigsuspend:
            res = syscall(SYS_rt_sigsuspend, (const sigset_t *) g_2_h(p0), (size_t) p1);
            break;
        case PR_mkdir:
            res = syscall(SYS_mkdir, (const char *) g_2_h(p0), (mode_t) p1);
            break;
        case PR_fchmodat:
            res = syscall(SYS_fchmodat, (int) p0, (const char *) g_2_h(p1), (mode_t) p2, (int) p3);
            break;
        case PR_fchmod:
            res = syscall(SYS_fchmod, (int) p0, (mode_t) p1);
            break;
        case PR_times:
            res = syscall(SYS_times, (struct tms *)g_2_h(p0));
            break;
        case PR_setrlimit:
            res = syscall(SYS_setrlimit, (int) p0, (const struct rlimit *) g_2_h(p1));
            break;
        case PR_chown32:
            res = syscall(SYS_chown, (const char *) g_2_h(p0), (uid_t) p1, (gid_t) p2);
            break;
        case PR_pause:
            res = syscall(SYS_pause);
            break;
        case PR_getitimer:
            res = syscall(SYS_getitimer, (int) p0, (struct itimerval *) g_2_h(p1));
            break;
        case PR_getpgid:
            res = syscall(SYS_getpgid, (pid_t) p0);
            break;
        case PR_setfsuid:
            res = syscall(SYS_setfsuid, (uid_t) (int16_t)p0);
            break;
        case PR_setfsgid:
            res = syscall(SYS_setfsgid, (uid_t) (int16_t)p0);
            break;
        case PR_getsid:
            res = syscall(SYS_getsid, (pid_t) p0);
            break;
        case PR_fdatasync:
            res = syscall(SYS_fdatasync, (int) p0);
            break;
        case PR_mlock:
            res = syscall(SYS_mlock, (const void *) g_2_h(p0), (size_t) p1);
            break;
        case PR_sched_setparam:
            res = syscall(SYS_sched_setparam, (pid_t) p0, IS_NULL(p1, struct sched_param));
            break;
        case PR_sched_getparam:
            res = syscall(SYS_sched_getparam, (pid_t) p0, IS_NULL(p1, struct sched_param));
            break;
        case PR_sched_setscheduler:
            res = syscall(SYS_sched_setscheduler, (pid_t) p0, (int) p1, (const struct sched_param *) g_2_h(p2));
            break;
        case PR_sched_getscheduler:
            res = syscall(SYS_sched_getscheduler, (pid_t) p0);
            break;
        case PR_sched_yield:
            res = syscall(SYS_sched_yield);
            break;
        case PR_sched_get_priority_max:
            res = syscall(SYS_sched_get_priority_max, (int) p0);
            break;
        case PR_sched_get_priority_min:
            res = syscall(SYS_sched_get_priority_min, (int) p0);
            break;
        case PR_rt_sigpending:
            res = syscall(SYS_rt_sigpending, (sigset_t *) g_2_h(p0), (size_t) p1);
            break;
        case PR_capget:
            res = syscall(SYS_capget, (void *) g_2_h(p0), p1?(void *) g_2_h(p1):NULL);
            break;
        case PR_setgroups32:
            res = syscall(SYS_setgroups32, (int) p0, (gid_t *) g_2_h(p1));
            break;
        case PR_setuid32:
            res = syscall(SYS_setuid, (uid_t) p0);
            break;
        case PR_setgid32:
            res = syscall(SYS_setgid, (gid_t) p0);
            break;
        case PR_setfsuid32:
            res = syscall(SYS_setfsuid, (uid_t) p0);
            break;
        case PR_setfsgid32:
            res = syscall(SYS_setfsgid, (uid_t) p0);
            break;
        case PR_setuid:
            res = syscall(SYS_setuid, (uid_t) (int16_t)p0);
            break;
        case PR_getuid:
            res = syscall(SYS_getuid);
            break;
        case PR_epoll_create:
            res = syscall(SYS_epoll_create, (int) p0);
            break;
        case PR_futimesat:
            syscall(SYS_futimesat, (int) p0, IS_NULL(p1, const char), IS_NULL(p2, const struct timeval));
            break;
        case PR_timerfd_create:
            res = syscall(SYS_timerfd_create, (int) p0, (int) p1);
            break;
        case PR_signalfd4:
            res = syscall(SYS_signalfd4, (int) p0, (sigset_t *) g_2_h(p1), (size_t) p2, (int) p3);
            break;
        case PR_perf_event_open:
            res = syscall(SYS_perf_event_open, (void *) g_2_h(p0), (pid_t) p1, (int) p2, (int) p3, (unsigned long) p4);
            break;
        case PR_sync:
            res = syscall(SYS_sync);
            break;
        case PR_rmdir:
            res = syscall(SYS_rmdir, (const char *) g_2_h(p0));
            break;
        case PR_setgid:
            res = syscall(SYS_setgid, (gid_t) (int16_t)p0);
            break;
        case PR_getgid:
            res = syscall(SYS_getgid);
            break;
        case PR_geteuid:
            res = syscall(SYS_geteuid);
            break;
        case PR_getegid:
            res = syscall(SYS_getegid);
            break;
        case PR_setsid:
            res = syscall(SYS_setsid);
            break;
        case PR_getpriority:
            res = syscall(SYS_getpriority, (int) p0, (id_t) p1);
            break;
        case PR_setpriority:
            res = syscall(SYS_setpriority, (int) p0, (id_t) p1, (int) p2);
            break;
        case PR_epoll_ctl:
            res = syscall(SYS_epoll_ctl, (int) p0, (int) p1, (int) p2, IS_NULL(p3, struct epoll_event));
            break;
        case PR_vhangup:
            res = syscall(SYS_vhangup);
            break;
        case PR_bdflush:
            /* This one is deprecated since 2.6 and p1 is no more use */
            res = 0;
            break;
        case PR_msync:
            res = syscall(SYS_msync, (void *) g_2_h(p0), (size_t) p1, (int) p2);
            break;
        case PR_mknod:
            res = syscall(SYS_mknod, (const char *) g_2_h(p0), (mode_t) p1, (dev_t) p2);
            break;
        case PR_munlock:
            res = syscall(SYS_munlock, (void *) g_2_h(p0), (size_t) p1);
            break;
        case PR_prctl:
            res = prctl_s3232(p0, p1, p2, p3, p4);
            break;
        case PR_capset:
            res = syscall(SYS_capset, (void *) g_2_h(p0), p1?(void *) g_2_h(p1):NULL);
            break;
        case PR_setregid32:
            res = syscall(SYS_setregid32, (gid_t) p0, (gid_t) p1);
            break;
        case PR_mincore:
            res = syscall(SYS_mincore, (void *) g_2_h(p0), (size_t) p1, (char *) g_2_h(p2));
            break;
        case PR_tkill:
            res = syscall(SYS_tkill, (int) p0, (int) p1);
            break;
        case PR_sched_getaffinity:
            res = syscall(SYS_sched_getaffinity, (pid_t) p0, (size_t) p1, (cpu_set_t *) g_2_h(p2));
            break;
        case PR_remap_file_pages:
            res = syscall(SYS_remap_file_pages, (void *) g_2_h(p0), (size_t) p1, (int) p2, (size_t) p3, (int) p4);
            break;
        case PR_timer_create:
            res = syscall(SYS_timer_create, (clockid_t) p0, IS_NULL(p1, struct sigevent),
                                                            IS_NULL(p2, timer_t));
            break;
        case PR_clock_nanosleep:
            res = syscall(SYS_clock_nanosleep, (clockid_t) p0, (int) p1, (struct timespec *) g_2_h(p2),
                                               IS_NULL(p3, struct timespec));
            break;
        case PR_mq_unlink:
            res = syscall(SYS_mq_unlink, (char *) g_2_h(p0));
            break;
        case PR_mq_notify:
            res = syscall(SYS_mq_notify, (mqd_t) p0, IS_NULL(p1, struct sigevent));
            break;
        case PR_waitid:
            res = syscall(SYS_waitid, (idtype_t) p0, (id_t) p1, IS_NULL(p2, siginfo_t), (int) p3,
                                      IS_NULL(p4, struct rusage));
            break;
        case PR_bind:
            {
                unsigned long args[3];

                args[0] = (int) p0;
                args[1] = (int)(const struct sockaddr *) g_2_h(p1);
                args[2] = (socklen_t) p2;
                res = syscall(SYS_socketcall, 2/*sys_bind*/, args);
            }
            break;
        case PR_listen:
            {
                unsigned long args[2];

                args[0] = (int) p0;
                args[1] = (int) p1;
                res = syscall(SYS_socketcall, 4/*sys_listen*/, args);
            }
            break;
        case PR_accept:
            {
                unsigned long args[3];

                args[0] = (int) p0;
                args[1] = (int) IS_NULL(p1, struct sockaddr);
                args[2] = (int) IS_NULL(p2, socklen_t);
                res = syscall(SYS_socketcall, 5/*sys_accept*/, args);
            }
            break;
        case PR_setsockopt:
            {
                unsigned long args[5];

                args[0] = (int) p0;
                args[1] = (int) p1;
                args[2] = (int) p2;
                args[3] = (int) (void *) g_2_h(p3);
                args[4] = (int) (socklen_t) p4;
                res = syscall(SYS_socketcall, 14/*sys_setsockopt*/, args);
            }
            break;
        case PR_getsockopt:
            {
                unsigned long args[5];

                args[0] = (int) p0;
                args[1] = (int) p1;
                args[2] = (int) p2;
                args[3] = (int) IS_NULL(p3, void);
                args[4] = (int) IS_NULL(p4, socklen_t);
                res = syscall(SYS_socketcall, 15/*sys_getsockopt*/, args);
            }
            break;
        case PR_semget:
            res = syscall(SYS_ipc, 2/*IPCOP_semget*/, (key_t) p0, (int) p1, (int) p2);
            break;
        case PR_msgget:
            res = syscall(SYS_ipc, 13/*IPCOP_msgget*/, (key_t) p0, (int) p1);
            break;
        case PR_msgctl:
            res = syscall(SYS_ipc, 14/*IPCOP_msgctl*/, (key_t) p0, (int) p1, (struct msqid_ds *) g_2_h(p2));
            break;
        case PR_add_key:
            res = syscall(SYS_add_key, (char *) g_2_h(p0), IS_NULL(p1, char), IS_NULL(p2, void), (size_t) p3, p4);
            break;
        case PR_keyctl:
            res = keyctl_s3232(p0, p1, p2, p3, p4);
            break;
        case PR_inotify_init:
            res = syscall(SYS_inotify_init);
            break;
        case PR_mkdirat:
            res = syscall(SYS_mkdirat, (int) p0, (const char *) g_2_h(p1), (mode_t) p2);
            break;
        case PR_mknodat:
            res = syscall(SYS_mknodat, (int) p0, (char *) g_2_h(p1), (mode_t) p2, (dev_t) p3);
            break;
        case PR_fchownat:
            res = syscall(SYS_fchownat, (int) p0, (const char *) g_2_h(p1), (uid_t) p2, (gid_t) p3, (int) p4);
            break;
        case PR_symlinkat:
            res = syscall(SYS_symlinkat, (const char *) g_2_h(p0), (int) p1, (const char *) g_2_h(p2));
            break;
        case PR_pselect6:
            res = pselect6_s3232(p0,p1,p2,p3,p4,p5);
            break;
        case PR_ppoll:
            res = syscall(SYS_ppoll, (struct pollfd *) g_2_h(p0), p1, IS_NULL(p2, struct timespec),
                                                                      IS_NULL(p3, sigset_t), (size_t) p4);
            break;
        case PR_splice:
            res = syscall(SYS_splice, (int) p0, IS_NULL(p1, loff_t), (int) p2, IS_NULL(p3, loff_t), (size_t) p4, (unsigned int) p5);
            break;
        case PR_tee:
            res = syscall(SYS_tee, (int) p0, (int) p1, (size_t) p2, (unsigned int) p3);
            break;
        case PR_vmsplice:
            res = vmsplice_s3232(p0, p1, p2 , p3);
            break;
        case PR_eventfd:
            res = syscall(SYS_eventfd, (int)p0, (int)(p1));
            break;
        case PR_fallocate:
            res = syscall(SYS_fallocate, (int) p0, (int) p1, p3, p2, p5, p4);
            break;
        case PR_timerfd_settime:
            res = syscall(SYS_timerfd_settime, (int) p0, (int) p1, (struct itimerspec *) g_2_h(p2), IS_NULL(p3, struct itimerspec));
            break;
        case PR_timerfd_gettime:
            res = syscall(SYS_timerfd_gettime, (int) p0, (struct itimerspec *) g_2_h(p1));
            break;
        case PR_epoll_create1:
            res = syscall(SYS_epoll_create1, (int) p0);
            break;
        case PR_dup3:
            res = syscall(SYS_dup3, (int) p0, (int) p1, (int) p2);
            break;
        case PR_inotify_init1:
            res = syscall(SYS_inotify_init1, (int) p0);
            break;
        case PR_setreuid:
            res = syscall(SYS_setreuid, (uid_t) (int16_t)p0, (uid_t) (int16_t)p1);
            break;
        case PR_setregid:
            res = syscall(SYS_setregid, (gid_t) (int16_t)p0, (gid_t) (int16_t)p1);
            break;
        case PR_symlink:
            res = syscall(SYS_symlink, (const char *) g_2_h(p0), (const char *) g_2_h(p1));
            break;
        case PR_creat:
            res = syscall(SYS_creat, (char *) g_2_h(p0), (mode_t) p1);
            break;
        case PR_truncate:
            res = syscall(SYS_truncate, (char *) g_2_h(p0), (off_t) p1);
            break;
        case PR_ftruncate:
            res = syscall(SYS_ftruncate, (int) p0, (off_t) p1);
            break;
        case PR_link:
            res = syscall(SYS_link, (const char *) g_2_h(p0), (const char *) g_2_h(p1));
            break;
        case PR_flock:
            res = syscall(SYS_flock, (int) p0, (int) p1);
            break;
        case PR_sendfile:
            res = syscall(SYS_sendfile, (int) p0, (int) p1, IS_NULL(p2, off_t), (size_t) p3);
            break;
        case PR_getgroups32:
            res = syscall(SYS_getgroups32, (int) p0, (gid_t *) g_2_h(p1));
            break;
        case PR_timer_gettime:
            res = syscall(SYS_timer_gettime, (timer_t) p0, (struct itimerspec *) g_2_h(p1));
            break;
        case PR_timer_getoverrun:
            res = syscall(SYS_timer_getoverrun, p0);
            break;
        case PR_utimes:
            res = syscall(SYS_utimes, (const char *) g_2_h(p0), IS_NULL(p1, const struct timeval));
            break;
        case PR_mq_open:
            res = syscall(SYS_mq_open, (char *) g_2_h(p0), (int) p1, (mode_t) p2, IS_NULL(p3, struct mq_attr));
            break;
        case PR_send:
            {
                unsigned long args[4];

                args[0] = (int) p0;
                args[1] = (int) (const void *) g_2_h(p1);
                args[2] = (int) (size_t) p2;
                args[3] = (int) p3;
                res = syscall(SYS_socketcall, 9/*sys_send*/, args);
            }
            break;
        case PR_sendto:
            {
                unsigned long args[6];

                args[0] = (int) p0;
                args[1] = (int) (const void *) g_2_h(p1);
                args[2] = (int) (size_t) p2;
                args[3] = (int) p3;
                args[4] = (int) (const struct sockaddr *) g_2_h(p4);
                args[5] = (int) (socklen_t) p5;
                res = syscall(SYS_socketcall, 11/*sys_sendto*/, args);
            }
            break;
        case PR_recvfrom:
            {
                unsigned long args[6];

                args[0] = (int) p0;
                args[1] = (int) (void *) g_2_h(p1);
                args[2] = (int) (size_t) p2;
                args[3] = (int) p3;
                args[4] = (int) IS_NULL(p4, struct sockaddr);
                args[5] = (int) IS_NULL(p5, socklen_t);
                res = syscall(SYS_socketcall, 12/*sys_recvfrom*/, args);
            }
            break;
        case PR_msgsnd:
            res = syscall(SYS_ipc, 11/*IPCOP_msgsnd*/, (int) p0, (void *) g_2_h(p1), (size_t) p2, (int) p3);
            break;
        case PR_msgrcv:
            res = syscall(SYS_ipc, 12/*IPCOP_msgrcv*/, (int) p0, (void *) g_2_h(p1), (size_t) p2, (long) p3, (int) p4);
            break;
        case PR_inotify_add_watch:
            res = syscall(SYS_inotify_add_watch, (int) p0, (const char *) g_2_h(p1), (uint32_t) p2);
            break;
        case PR_linkat:
            res = syscall(SYS_linkat, (int) p0, (const char *) g_2_h(p1), (int) p2, (const char *) g_2_h(p3), (int) p4);
            break;
        case PR_readlinkat:
            res = syscall(SYS_readlinkat, (int) p0, (const char *) g_2_h(p1), (char *) g_2_h(p2), (size_t) p3);
            break;
        case PR_accept4:
            {
                unsigned long args[4];

                args[0] = (int) p0;
                args[1] = (int) IS_NULL(p1, struct sockaddr);
                args[2] = (int) IS_NULL(p2, socklen_t);
                args[3] = (int) p3;
                res = syscall(SYS_socketcall, 18/*sys_accept4*/, args);
            }
            break;
        case PR_chroot:
            res = syscall(SYS_chroot, (char *) g_2_h(p0));
            break;
        case PR_getgroups:
            res = syscall(SYS_getgroups, (int) p0, (gid_t *) g_2_h(p1));
            break;
        default:
            fatal("syscall_32_to_32: unsupported neutral syscall %d\n", no);
    }

    return res;
}
