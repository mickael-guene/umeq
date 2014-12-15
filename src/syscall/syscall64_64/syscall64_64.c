#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <errno.h>
#include <stdint.h>
#include <sched.h>
#include <signal.h>
#include <mqueue.h>

#include "syscall64_64.h"
#include "syscall64_64_private.h"
#include "runtime.h"

#define IS_NULL(px,type) ((px)?(type *)g_2_h((px)):NULL)

long syscall64_64(Sysnum no, uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3, uint64_t p4, uint64_t p5)
{
    long res = ENOSYS;

    switch(no) {
        case PR_write:
            res = syscall(SYS_write, (int)p0, (void *)g_2_h(p1), (size_t)p2);
            break;
        case PR_readlinkat:
            res = syscall(SYS_readlinkat, (int) p0, (const char *) g_2_h(p1), (char *) g_2_h(p2), (size_t) p3);
            break;
        case PR_faccessat:
            res = syscall(SYS_faccessat, (int) p0, (const char *) g_2_h(p1), (int) p2, (int) p3);
            break;
        case PR_exit_group:
            res = syscall(SYS_exit_group, (int) p0);
            break;
        case PR_close:
            res = syscall(SYS_close, (int) p0);
            break;
        case PR_mmap:
            res = syscall(SYS_mmap, (void *) g_2_h(p0), (size_t) p1, (int) p2, (int) p3, (int) p4, (off_t) p5);
            break;
        case PR_read:
            res = syscall(SYS_read, (int) p0, (void *) g_2_h(p1), (size_t) p2);
            break;
        case PR_mprotect:
            res = syscall(SYS_mprotect, (void *) g_2_h(p0), (size_t) p1, (int) p2);
            break;
        case PR_munmap:
            res = syscall(SYS_munmap, (void *) g_2_h(p0), (size_t) p1);
            break;
        case PR_set_tid_address:
            res = syscall(SYS_set_tid_address, (int *) g_2_h(p0));
            break;
        case PR_set_robust_list:
            /* FIXME: implement this correctly */
            res = -EPERM;
            break;
        case PR_rt_sigprocmask:
            res = syscall(SYS_rt_sigprocmask, (int)p0, IS_NULL(p1, const sigset_t),
                                              IS_NULL(p2, sigset_t), (size_t) p3);
            break;
        case PR_getrlimit:
            res = syscall(SYS_getrlimit, (int) p0, (struct rlimit *) g_2_h(p1));
            break;
        case PR_statfs:
            res = syscall(SYS_statfs, (const char *) g_2_h(p0), (struct statfs *) g_2_h(p1));
            break;
        case PR_ioctl:
            /* FIXME: need specific version to offset param according to ioctl */
            res = syscall(SYS_ioctl, (int) p0, (unsigned long) p1, (char *) g_2_h(p2));
            break;
        case PR_getdents64:
            res = syscall(SYS_getdents64, (unsigned int) p0, (struct linux_dirent *) g_2_h(p1), (unsigned int) p2);
            break;
        case PR_lgetxattr:
            res = syscall(SYS_lgetxattr, (const char *) g_2_h(p0), (const char *) g_2_h(p1),
                                         (void *) g_2_h(p2), (size_t) p3);
            break;
        case PR_getxattr:
            res = syscall(SYS_getxattr, (const char *) g_2_h(p0), (const char *) g_2_h(p1),
                                        (void *) g_2_h(p2), (size_t) p3);
            break;
        case PR_socket:
            res = syscall(SYS_socket, (int) p0, (int) p1, (int) p2);
            break;
        case PR_connect:
            res = syscall(SYS_connect, (int) p0, (const struct sockaddr *) g_2_h(p1), (socklen_t) p2);
            break;
        case PR_lseek:
            res = syscall(SYS_lseek, (int) p0, (off_t) p1, (int) p2);
            break;
        case PR_clock_gettime:
            res = syscall(SYS_clock_gettime, (clockid_t) p0, (struct timespec *) g_2_h(p1));
            break;
        case PR_geteuid:
            res = syscall(SYS_geteuid);
            break;
        case PR_getuid:
            res = syscall(SYS_getuid);
            break;
        case PR_getegid:
            res = syscall(SYS_getegid);
            break;
        case PR_getgid:
            res = syscall(SYS_getgid);
            break;
        case PR_getgroups:
            res = syscall(SYS_getgroups, (int) p0, (gid_t *) g_2_h(p1));
            break;
        case PR_getpid:
            res = syscall(SYS_getpid);
            break;
        case PR_sync:
            res = syscall(SYS_sync);
            break;
        case PR_dup3:
            res = syscall(SYS_dup3, (int) p0, (int) p1, (int) p2);
            break;
        case PR_utimensat:
            res = syscall(SYS_utimensat, (int) p0, (const char *) g_2_h(p1),
                                         (const struct timespec *) g_2_h(p2), (int) p3);
            break;
        case PR_setrlimit:
            res = syscall(SYS_setrlimit, (int) p0, (const struct rlimit *) g_2_h(p1));
            break;
        case PR_fadvise64:
            res = syscall(SYS_fadvise64, (int) p0, (off_t) p1, (off_t) p2, (int) p3);
            break;
        case PR_gettimeofday:
            res = syscall(SYS_gettimeofday, (struct timeval *) g_2_h(p0), (struct timezone *) g_2_h(p1));
            break;
        case PR_getppid:
            res = syscall(SYS_getppid);
            break;
        case PR_getpgid:
            res = syscall(SYS_getpgid, (pid_t) p0);
            break;
        case PR_dup:
            res = syscall(SYS_dup, (int)p0);
            break;
        case PR_fcntl:
            res = fcntl_s6464(p0,p1,p2);
            break;
        case PR_setpgid:
            res = syscall(SYS_setpgid, (pid_t)p0, (pid_t)p1);
            break;
        case PR_pipe2:
            res = syscall(SYS_pipe2, (int *) g_2_h(p0), (int) p1);
            break;
        case PR_execve:
            res = execve_s6464(p0,p1,p2);
            break;
        case PR_wait4:
            res = syscall(SYS_wait4, (pid_t) p0, (int *) g_2_h(p1), (int) p2, (struct rusage *) g_2_h(p3));
            break;
        case PR_getrusage:
            res = syscall(SYS_getrusage, (int) p0, (struct rusage *) g_2_h(p1));
            break;
        case PR_unlinkat:
            res = syscall(SYS_unlinkat, (int) p0, (char *) g_2_h(p1), (int) p2);
            break;
        case PR_umask:
            res = syscall(SYS_umask, (mode_t) p0);
            break;
        case PR_fchmodat:
            res = syscall(SYS_fchmodat, (int) p0, (char *) g_2_h(p1), (mode_t) p2, (int) p3);
            break;
        case PR_chdir:
            res = syscall(SYS_chdir, (const char *) g_2_h(p0));
            break;
        case PR_pread64:
            res = syscall(SYS_pread64, (int) p0, (void *) g_2_h(p1), (size_t) p2, (off_t) p3);
            break;
        case PR_nanosleep:
            res = syscall(SYS_nanosleep, (struct timespec *) p0, IS_NULL(p1, struct timespec));
            break;
        case PR_futex:
            res = futex_s6464(p0,p1,p2,p3,p4,p5);
            break;
        case PR_clock_getres:
            res = syscall(SYS_clock_getres, (clockid_t) p0, IS_NULL(p1, struct timespec));
            break;
        case PR_ppoll:
            res = syscall(SYS_ppoll, (struct pollfd *) g_2_h(p0), p1, IS_NULL(p2, struct timespec),
                                                                      IS_NULL(p3, sigset_t), (size_t) p4);
            break;
        case PR_sendmmsg:
            res = syscall(SYS_sendmmsg, (int) p0, (struct mmsghdr *) g_2_h(p1), (unsigned int) p2, (unsigned int) p3);
            break;
        case PR_recvfrom:
            res = syscall(SYS_recvfrom, (int) p0, (void *) g_2_h(p1), (size_t) p2, (int) p3,
                                        IS_NULL(p4, struct sockaddr), IS_NULL(p5, socklen_t));
            break;
        case PR_getcwd:
            res = syscall(SYS_getcwd, (char *) g_2_h(p0), (size_t) p1);
            break;
        case PR_pselect6:
            res = pselect6_s6464(p0,p1,p2,p3,p4,p5);
            break;
        case PR_mkdirat:
            res = syscall(SYS_mkdirat, (int) p0, (char *) g_2_h(p1), (mode_t) p2);
            break;
        case PR_renameat:
            res = syscall(SYS_renameat, (int) p0, (char *) g_2_h(p1), (int) p2, (char *) g_2_h(p3));
            break;
        case PR_fsetxattr:
            res = syscall(SYS_fsetxattr, (int) p0, (char *) g_2_h(p1), (void *) g_2_h(p2), (size_t) p3, (int) p4);
            break;
        case PR_sched_getaffinity:
            res = syscall(SYS_sched_getaffinity, (pid_t) p0, (size_t) p1, (cpu_set_t *) g_2_h(p2));
            break;
        case PR_kill:
            res = syscall(SYS_kill, (pid_t) p0, (int) p1);
            break;
        case PR_fgetxattr:
            res = syscall(SYS_fgetxattr, (int) p0, (char *) g_2_h(p1), (void *) g_2_h(p2), (size_t) p3);
            break;
        case PR_getpeername:
            res = syscall(SYS_getpeername, (int) p0, (struct sockaddr *) g_2_h(p1), (socklen_t *) g_2_h(p2));
            break;
        case PR_fchownat:
            res = syscall(SYS_fchownat, (int) p0, (char *) g_2_h(p1), (uid_t) p2, (gid_t) p3, (int) p4);
            break;
        case PR_fchdir:
            res = syscall(SYS_fchdir, (int) p0);
            break;
        case PR_symlinkat:
            res = syscall(SYS_symlinkat, (char *) g_2_h(p0), (int) p1, (char *) g_2_h(p2));
            break;
        case PR_linkat:
            res = syscall(SYS_linkat, (int) p0, (char *) g_2_h(p1), (int) p2, (char *) g_2_h(p3), (int) p4);
            break;
        case PR_madvise:
            res = syscall(SYS_madvise, (void *) g_2_h(p0), (size_t) p1, (int) p2);
            break;
        case PR_tgkill:
            res = syscall(SYS_tgkill, (int) p0, (int) p1, (int) p2);
            break;
        case PR_fchmod:
            res = syscall(SYS_fchmod, (int) p0, (mode_t) p1);
            break;
        case PR_setitimer:
            res = syscall(SYS_setitimer, (int) p0, IS_NULL(p1, struct itimerval), IS_NULL(p2, struct itimerval));
            break;
        case PR_timer_create:
            res = syscall(SYS_timer_create, (clockid_t) p0, IS_NULL(p1, struct sigevent),
                                                            IS_NULL(p2, timer_t));
            break;
        case PR_getpriority:
            res = syscall(SYS_getpriority, (int) p0, (id_t) p1);
            break;
        case PR_setsid:
            res = syscall(SYS_setsid);
            break;
        case PR_inotify_init1:
            res = syscall(SYS_inotify_init1, (int) p0);
            break;
        case PR_mknodat:
            res = syscall(SYS_mknodat, (int) p0, (char *) g_2_h(p1), (mode_t) p2, (dev_t) p3);
            break;
        case PR_fstatfs:
            res = syscall(SYS_fstatfs, (int) p0, (struct statfs *) g_2_h(p1));
            break;
        case PR_ftruncate:
            res = syscall(SYS_ftruncate, (int) p0, (off_t) p1);
            break;
        case PR_fchown:
            res = syscall(SYS_fchown, (int) p0, (uid_t) p1, (gid_t) p2);
            break;
        case PR_setxattr:
            res = syscall(SYS_setxattr, (char *) g_2_h(p0), (char *) g_2_h(p1), (void *) g_2_h(p2), (size_t) p3, (int) p4);
            break;
        case PR_fdatasync:
            res = syscall(SYS_fdatasync, (int) p0);
            break;
        case PR_timer_settime:
            res = syscall(SYS_timer_settime, (timer_t) p0, (int) p1, (struct itimerspec *) g_2_h(p2),
                                             IS_NULL(p3, struct itimerspec));
            break;
        case PR_rt_sigsuspend:
            res = syscall(SYS_rt_sigsuspend, (sigset_t *) g_2_h(p0), (size_t) p1);
            break;
        case PR_setpriority:
            res = syscall(SYS_setpriority, (int) p0, (id_t) p1, (int) p2);
            break;
        case PR_inotify_add_watch:
            res = syscall(SYS_inotify_add_watch, (int) p0, (char *) g_2_h(p1), (uint32_t) p2);
            break;
        case PR_fsync:
            res = syscall(SYS_fsync, (int) p0);
            break;
        case PR_prctl:
            res = syscall(SYS_prctl, (int) p0, (unsigned long) p1, (unsigned long) p2, (unsigned long) p3, (unsigned long) p4);
            break;
        case PR_writev:
            res = syscall(SYS_writev, (int) p0, (struct iovec *) g_2_h(p1), (int) p2);
            break;
        case PR_bind:
            res = syscall(SYS_bind, (int) p0, (struct sockaddr *) g_2_h(p1), (socklen_t) p2);
            break;
        case PR_getsockname:
            res = syscall(SYS_getsockname, (int) p0, (struct sockaddr *) g_2_h(p1), (socklen_t *) g_2_h(p2));
            break;
        case PR_msync:
            res = syscall(SYS_msync, (void *) g_2_h(p0), (size_t) p1, (int) p2);
            break;
        case PR_sendto:
            res = syscall(SYS_sendto, (int) p0, (void *) g_2_h(p1), (size_t) p2, (int) p3,
                                      (struct sockaddr *) g_2_h(p4), (socklen_t) p5);
            break;
        case PR_recvmsg:
            res = syscall(SYS_recvmsg, (int) p0, (struct msghdr *) g_2_h(p1), (int) p2);
            break;
        case PR_getsockopt:
            res = syscall(SYS_getsockopt, (int) p0, (int) p1, (int) p2,
                                          IS_NULL(p3, void), IS_NULL(p4, void));
            break;
        case PR_flock:
            res = syscall(SYS_flock, (int) p0, (int) p1);
            break;
        case PR_fallocate:
            res = syscall(SYS_fallocate, (int) p0, (int) p1, (off_t) p2, (off_t) p3);
            break;
        case PR_sync_file_range:
            res = syscall(SYS_sync_file_range, (int) p0, (off64_t) p1, (off64_t) p2, (unsigned int) p3);
            break;
        case PR_semget:
            res = syscall(SYS_semget, (key_t) p0, (int) p1, (int) p2);
            break;
        case PR_mremap:
            res = syscall(SYS_mremap, (void *) g_2_h(p0), (size_t) p1, (size_t) p2, (int) p3, (void *) g_2_h(p4));
            break;
        case PR_semctl:
            res = semctl_s6464(p0,p1,p2,p3);
            break;
        case PR_clock_nanosleep:
            res = syscall(SYS_clock_nanosleep, (clockid_t) p0, (int) p1, (struct timespec *) g_2_h(p2),
                                               IS_NULL(p3, struct timespec));
            break;
        case PR_gettid:
            res = syscall(SYS_gettid);
            break;
        case PR_socketpair:
            res = syscall(SYS_socketpair, (int) p0, (int) p1, (int) p2,(int *) g_2_h(p3));
            break;
        case PR_personality:
            res = syscall(SYS_personality, (unsigned long) p0);
            break;
        case PR_truncate:
            res = syscall(SYS_truncate, (char *) g_2_h(p0), (off_t) p1);
            break;
        case PR_shmget:
            res = syscall(SYS_shmget, (key_t) p0, (size_t) p1, (int) p2);
            break;
        case PR_listen:
            res = syscall(SYS_listen, (int) p0, (int) p1);
            break;
        case PR_setresuid:
            res = syscall(SYS_setresuid, (uid_t) p0, (uid_t) p1, (uid_t) p2);
            break;
        case PR_rt_sigpending:
            res = syscall(SYS_rt_sigpending, (sigset_t *) g_2_h(p0), (size_t) p1);
            break;
        case PR_times:
            res = syscall(SYS_times, (struct tms *) g_2_h(p0));
            break;
        case PR_shmctl:
            res = syscall(SYS_shmctl, (int) p0, (int) p1, (struct shmid_ds *) g_2_h(p2));
            break;
        case PR_accept:
            res = syscall(SYS_accept, (int) p0, IS_NULL(p1, struct sockaddr), IS_NULL(p2, socklen_t));
            break;
        case PR_msgget:
            res = syscall(SYS_msgget, (key_t) p0, (int) p1);
            break;
        case PR_setsockopt:
            res = syscall(SYS_setsockopt, (int) p0, (int) p1, (int) p2, IS_NULL(p3, void), (socklen_t) p4);
            break;
        case PR_getitimer:
            res = syscall(SYS_getitimer, (int) p0, (struct itimerval *) g_2_h(p1));
            break;
        case PR_shmat:
            /* FIXME: return value is a pointer */
            res = syscall(SYS_shmat, (int) p0, (void *) g_2_h(p1), (int) p2);
            break;
        case PR_shutdown:
            res = syscall(SYS_shutdown, (int) p0, (int) p1);
            break;
        case PR_getresuid:
            res = syscall(SYS_getresuid, (uid_t *) g_2_h(p0), (uid_t *) g_2_h(p1), (uid_t *) g_2_h(p2));
            break;
        case PR_getresgid:
            res = syscall(SYS_getresgid, (gid_t *) g_2_h(p0), (gid_t *) g_2_h(p1), (gid_t *) g_2_h(p2));
            break;
        case PR_sendmsg:
            /* FIXME: will not work with offset since msg struct contain pointers */
            res = syscall(SYS_sendmsg, (int) p0, (struct msghdr *) g_2_h(p1), (int) p2);
            break;
        case PR_eventfd2:
            res = syscall(SYS_eventfd2, (int) p0, (int) p1);
            break;
        case PR_rt_sigtimedwait:
            /* FIXME: is siginfo has same structure for arm64 ? */
            res = syscall(SYS_rt_sigtimedwait, (sigset_t *) g_2_h(p0), IS_NULL(p1, siginfo_t),
                                               IS_NULL(p2, struct timespec), (size_t) p3);
            break;
        case PR_shmdt:
            res = syscall(SYS_shmdt, (void *) g_2_h(p0));
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
        case PR_setuid:
            res = syscall(SYS_setuid, (uid_t) p0);
            break;
        case PR_tkill:
            res = syscall(SYS_tkill, (int) p0, (int) p1);
            break;
        case PR_setreuid:
            res = syscall(SYS_setreuid, (uid_t) p0, (uid_t) p1);
            break;
        case PR_process_vm_readv:
            /* FIXME: offset support not easy to do. what if we return not implemented ? */
            res = syscall(SYS_process_vm_readv, (pid_t) p0, (struct iovec *) p1, (unsigned long) p2,
                                                (struct iovec *) p3, (unsigned long) p4, (unsigned long) p5);
            break;
        case PR_semop:
            res = syscall(SYS_semop, (int) p0, (struct sembuf *) g_2_h(p1), (size_t) p2);
            break;
        case PR_mlock:
            res = syscall(SYS_mlock, (void *) g_2_h(p0), (size_t) p1);
            break;
        case PR_munlock:
            res = syscall(SYS_munlock, (void *) g_2_h(p0), (size_t) p1);
            break;
        case PR_msgsnd:
            res = syscall(SYS_msgsnd, (int) p0, (void *) g_2_h(p1), (size_t) p2, (int) p3);
            break;
        case PR_msgrcv:
            res = syscall(SYS_msgrcv, (int) p0, (void *) g_2_h(p1), (size_t) p2, (long) p3, (int) p4);
            break;
        case PR_msgctl:
            res = syscall(SYS_msgctl, (int) p0, (int) p1, (struct msqid_ds *) g_2_h(p2));
            break;
        case PR_accept4:
            res = syscall(SYS_accept4, (int) p0, IS_NULL(p1, struct sockaddr),
                                       IS_NULL(p2, socklen_t), (int) p3);
            break;
        case PR_add_key:
            res = syscall(SYS_add_key, (char *) g_2_h(p0), (char *) g_2_h(p1), (void *) g_2_h(p2),
                                       (size_t) p3, (uint64_t/*key_serial_t*/) p4);
            break;
        case PR_setresgid:
            res = syscall(SYS_setresgid, (gid_t) p0, (gid_t) p1, (gid_t) p2);
            break;
        case PR_capget:
            res = syscall(SYS_capget, (uint64_t/*cap_user_header_t*/) p0, IS_NULL(p1, void));
            break;
        case PR_epoll_ctl:
            res = syscall(SYS_epoll_ctl, (int) p0, (int) p1, (int) p2, (struct epoll_event *) g_2_h(p3));
            break;
        case PR_epoll_create1:
            res = syscall(SYS_epoll_create1, (int) p0);
            break;
        case PR_setgid:
            res = syscall(SYS_setgid, (gid_t) p0);
            break;
        case PR_setgroups:
            res = syscall(SYS_setgroups, (size_t) p0, (gid_t *) g_2_h(p1));
            break;
        case PR_getsid:
            res = syscall(SYS_getsid, (pid_t) p0);
            break;
        case PR_inotify_rm_watch:
            res = syscall(SYS_inotify_rm_watch, (int) p0, (int) p1);
            break;
        case PR_fanotify_init:
            res = syscall(SYS_fanotify_init, (unsigned int) p0, (unsigned int) p1);
            break;
        case PR_keyctl:
            /* FIXME: check according to params */
            res = syscall(SYS_keyctl, (int) p0, p1,p2,p3,p4,p5);
            break;
        case PR_mq_unlink:
            res = syscall(SYS_mq_unlink, (char *) g_2_h(p0));
            break;
        case PR_mq_notify:
            res = syscall(SYS_mq_notify, (mqd_t) p0, IS_NULL(p1, struct sigevent));
            break;
        case PR_mincore:
            res = syscall(SYS_mincore, (void *) g_2_h(p0), (size_t) p1, (char *) g_2_h(p2));
            break;
        case PR_pwrite64:
            res = syscall(SYS_pwrite64, (int) p0, (void *) g_2_h(p1), (size_t) p2, (off_t) p3);
            break;
        case PR_readv:
            res = syscall(SYS_readv, (int) p0, (struct iovec *) g_2_h(p1), (int) p2);
            break;
        case PR_remap_file_pages:
            res = syscall(SYS_remap_file_pages, (void *) g_2_h(p0), (size_t) p1, (int) p2, (size_t) p3, (int) p4);
            break;
        case PR_rt_sigqueueinfo:
            res = syscall(SYS_rt_sigqueueinfo, (pid_t) p0, (int) p1, (siginfo_t *) g_2_h(p2));
            break;
        case PR_sched_getparam:
            res = syscall(SYS_sched_getparam, (pid_t) p0, (struct sched_param *) g_2_h(p1));
            break;
        case PR_sched_setscheduler:
            res = syscall(SYS_sched_setscheduler, (pid_t) p0, (int) p1, (struct sched_param *) g_2_h(p2));
            break;
        case PR_sched_setparam:
            res = syscall(SYS_sched_setparam, (pid_t) p0, (struct sched_param *) g_2_h(p1));
            break;
        case PR_sched_getscheduler:
            res = syscall(SYS_sched_getscheduler, (pid_t) p0);
            break;
        case PR_sendfile:
            res = syscall(SYS_sendfile, (int) p0, (int) p1, (off_t *) g_2_h(p2), (size_t) p3);
            break;
        case PR_setfsgid:
            res = syscall(SYS_setfsgid, (uid_t) p0);
            break;
        case PR_setfsuid:
            res = syscall(SYS_setfsuid, (uid_t) p0);
            break;
        case PR_setregid:
            res = syscall(SYS_setregid, (gid_t) p0, (gid_t) p1);
            break;
        case PR_sysinfo:
            res = syscall(SYS_sysinfo, (struct sysinfo *) g_2_h(p0));
            break;
        case PR_timer_getoverrun:
            res = syscall(SYS_timer_getoverrun, (timer_t) p0);
            break;
        case PR_timer_gettime:
            res = syscall(SYS_timer_gettime, (timer_t) p0, (struct itimerspec *) g_2_h(p1));
            break;
        case PR_unshare:
            res = syscall(SYS_unshare, (int) p0);
            break;
        case PR_waitid:
            res = syscall(SYS_waitid, (idtype_t) p0, (id_t) p1, IS_NULL(p2, siginfo_t), (int) p3,
                                      IS_NULL(p4, struct rusage));
            break;
        case PR_perf_event_open:
            res = syscall(SYS_perf_event_open, (struct perf_event_attr *) g_2_h(p0), (pid_t) p1, (int) p2,
                                                (int) p3, (unsigned long) p4);
            break;
        case PR_capset:
            res = syscall(SYS_capset, (uint64_t/*cap_user_header_t*/) p0, IS_NULL(p1, void));
            break;
        case PR_chroot:
            res = syscall(SYS_chroot, (char *) g_2_h(p0));
            break;
        case PR_mq_open:
            res = syscall(SYS_mq_open, (char *) g_2_h(p0), (int) p1, (mode_t) p2, IS_NULL(p3, struct mq_attr));
            break;
        case PR_mq_timedsend:
            res = syscall(SYS_mq_timedsend, (mqd_t) p0, (char *) g_2_h(p1), (size_t) p2, (unsigned int) p3,
                                                        IS_NULL(p4, struct timespec));
            break;
        case PR_signalfd4:
            res = syscall(SYS_signalfd4, (int) p0, (sigset_t *) p1, (size_t) p2, (int) p3);
            break;
        case PR_splice:
            res = syscall(SYS_splice, (int) p0, (loff_t *) g_2_h(p1), (int) p2, (loff_t *) g_2_h(p3), (size_t) p4, (unsigned int) p5);
            break;
        case PR_tee:
            res = syscall(SYS_tee, (int) p0, (int) p1, (size_t) p2, (unsigned int) p3);
            break;
        case PR_timerfd_create:
            res = syscall(SYS_timerfd_create, (int) p0, (int) p1);
            break;
        case PR_vmsplice:
            res = syscall(SYS_vmsplice, (int) p0, (struct iovec *) g_2_h(p1), (unsigned long) p2, (unsigned int) p3);
            break;
        case PR_mq_timedreceive:
            res = syscall(SYS_mq_timedreceive, (mqd_t) p0, (char *) g_2_h(p1), (size_t) p2, IS_NULL(p3, unsigned int),
                                                IS_NULL(p4, struct timespec));
            break;
        case PR_timerfd_settime:
            res = syscall(SYS_timerfd_settime, (int) p0, (int) p1, (struct itimerspec *) g_2_h(p2), IS_NULL(p3, struct itimerspec));
            break;
        case PR_timerfd_gettime:
            res = syscall(SYS_timerfd_gettime, (int) p0, (struct itimerspec *) g_2_h(p1));
            break;
        case PR_name_to_handle_at:
            res = syscall(SYS_name_to_handle_at, (int) p0, ( char *) g_2_h(p1), (struct file_handle *) g_2_h(p2),
                                                 (int *) g_2_h(p3), (int) p4);
            break;
        case PR_sched_setaffinity:
            res = syscall(SYS_sched_setaffinity, (pid_t) p0, (size_t) p1, (unsigned long *) g_2_h(p2));
            break;
        case PR_mq_getsetattr:
            res = syscall(SYS_mq_getsetattr, (mqd_t) p0, IS_NULL(p1, struct mq_attr),
                                                         IS_NULL(p2, struct mq_attr));
            break;
        case PR_flistxattr:
            res = syscall(SYS_flistxattr, (int) p0, IS_NULL(p1, char), (size_t) p2);
            break;
        default:
            fatal("syscall64_64: unsupported neutral syscall %d\n", no);
    }

    return res;
}
