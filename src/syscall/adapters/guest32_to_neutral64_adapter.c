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
#include <string.h>

#include "syscalls_neutral_types.h"
#include "syscalls_neutral.h"
#include "runtime.h"
#include "target32.h"
#include "umeq.h"

#define IS_NULL(px) ((uint64_t)((px)?g_2_h((px)):NULL))

static int futex_neutral(uint32_t uaddr_p, uint32_t op_p, uint32_t val_p, uint32_t timeout_p, uint32_t uaddr2_p, uint32_t val3_p)
{
    long res;
    int *uaddr = (int *) g_2_h(uaddr_p);
    int op = (int) op_p;
    int val = (int) val_p;
    const struct neutral_timespec_32 *timeout_guest = (struct neutral_timespec_32 *) g_2_h(timeout_p);
    int *uaddr2 = (int *) g_2_h(uaddr2_p);
    int val3 = (int) val3_p;
    struct neutral_timespec_64 timeout;
    int cmd = op & NEUTRAL_FUTEX_CMD_MASK;
    long syscall_timeout = (long) (timeout_p?&timeout:NULL);

    /* fixup syscall_timeout in case it's not really a timeout structure */
    if (cmd == NEUTRAL_FUTEX_REQUEUE || cmd == NEUTRAL_FUTEX_CMP_REQUEUE ||
        cmd == NEUTRAL_FUTEX_CMP_REQUEUE_PI || cmd == NEUTRAL_FUTEX_WAKE_OP) {
        syscall_timeout = timeout_p;
    } else if (cmd == NEUTRAL_FUTEX_WAKE) {
        /* in this case timeout argument is not use and can take any value */
        syscall_timeout = timeout_p;
    } else if (timeout_p) {
        timeout.tv_sec = timeout_guest->tv_sec;
        timeout.tv_nsec = timeout_guest->tv_nsec;
    }

    res = syscall_neutral_64(PR_futex, (uint64_t)uaddr, op, val, syscall_timeout, (uint64_t)uaddr2, val3);

    return res;
}

static  int getrlimit_neutral(uint32_t resource_p, uint32_t rlim_p)
{
    long res;
    int ressource = (int) resource_p;
    struct neutral_rlimit_32 *rlim_guest = (struct neutral_rlimit_32 *) g_2_h(rlim_p);
    struct neutral_rlimit_64 rlim;

    if (rlim_p == 0 || rlim_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall_neutral_64(PR_getrlimit, ressource, (uint64_t)&rlim, 0, 0, 0, 0);
        rlim_guest->rlim_cur = rlim.rlim_cur;
        rlim_guest->rlim_max = rlim.rlim_max;
    }

    return res;
}

static int statfs64_neutral(uint32_t path_p, uint32_t sz, uint32_t buf_p)
{
    const char *path = (const char *) g_2_h(path_p);
    struct neutral_statfs64_32 *buf_guest = (struct neutral_statfs64_32 *) g_2_h(buf_p);
    struct neutral_statfs_64 buf;
    long res;

    if (buf_p == 0 || buf_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall_neutral_64(PR_statfs, (uint64_t)path, (uint64_t)&buf, 0, 0, 0, 0);

        buf_guest->f_type = buf.f_type;
        buf_guest->f_bsize = buf.f_bsize;
        buf_guest->f_blocks = buf.f_blocks;
        buf_guest->f_bfree = buf.f_bfree;
        buf_guest->f_bavail = buf.f_bavail;
        buf_guest->f_files = buf.f_files;
        buf_guest->f_ffree = buf.f_ffree;
        buf_guest->f_fsid.val[0] = 0;
        buf_guest->f_fsid.val[1] = 0;
        buf_guest->f_namelen = buf.f_namelen;
        buf_guest->f_frsize = buf.f_frsize;
        buf_guest->f_flags = buf.f_flags;
    }

    return res;
}

static int fcnt64_neutral(uint32_t fd_p, uint32_t cmd_p, uint32_t opt_p)
{
    long res = -EINVAL;
    int fd = fd_p;
    int cmd = cmd_p;

    switch(cmd) {
        case NEUTRAL_F_DUPFD:/*0*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, (int)opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_GETFD:/*1*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_SETFD:/*2*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, (int)opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_GETFL:/*3*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_SETFL:/*4*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_GETLK:/*5*/
            {
                struct neutral_flock_32 *lock_guest = (struct neutral_flock_32 *) g_2_h(opt_p);
                struct neutral_flock_64 lock;

                if (opt_p == 0 || opt_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    lock.l_type = lock_guest->l_type;
                    lock.l_whence = lock_guest->l_whence;
                    lock.l_start = lock_guest->l_start;
                    lock.l_len = lock_guest->l_len;
                    lock.l_pid = lock_guest->l_pid;
                    res = syscall_neutral_64(PR_fcntl, fd, cmd, (uint64_t)&lock, 0, 0, 0);
                    lock_guest->l_type = lock.l_type;
                    lock_guest->l_whence = lock.l_whence;
                    lock_guest->l_start = lock.l_start;
                    lock_guest->l_len = lock.l_len;
                    lock_guest->l_pid = lock.l_pid;
                }
            }
            break;
        case NEUTRAL_F_SETLK:/*6*/
            /* Fallthrough */
        case NEUTRAL_F_SETLKW:/*7*/
            {
                struct neutral_flock_32 *lock_guest = (struct neutral_flock_32 *) g_2_h(opt_p);
                struct neutral_flock_64 lock;

                if (opt_p == 0 || opt_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    lock.l_type = lock_guest->l_type;
                    lock.l_whence = lock_guest->l_whence;
                    lock.l_start = lock_guest->l_start;
                    lock.l_len = lock_guest->l_len;
                    lock.l_pid = lock_guest->l_pid;
                    res = syscall_neutral_64(PR_fcntl, fd, cmd, (uint64_t)&lock, 0, 0, 0);
                }
            }
            break;
        case NEUTRAL_F_SETOWN:/*8*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, (int) opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_GETOWN:/*9*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, 0, 0, 0, 0);
            break;
        case NEUTRAL_F_SETSIG:/*10*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, (int) opt_p, 0, 0, 0);
            break;
        case NEUTRAL_F_GETSIG:/*11*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, 0, 0, 0, 0);
            break;
        case NEUTRAL_F_GETLK64:/*12*/
            {
                struct neutral_flock64_32 *lock_guest = (struct neutral_flock64_32 *) g_2_h(opt_p);
                struct neutral_flock_64 lock;

                if (opt_p == 0 || opt_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    lock.l_type = lock_guest->l_type;
                    lock.l_whence = lock_guest->l_whence;
                    lock.l_start = lock_guest->l_start;
                    lock.l_len = lock_guest->l_len;
                    lock.l_pid = lock_guest->l_pid;
                    res = syscall_neutral_64(PR_fcntl, fd, NEUTRAL_F_GETLK, (uint64_t)&lock, 0, 0, 0);
                    lock_guest->l_type = lock.l_type;
                    lock_guest->l_whence = lock.l_whence;
                    lock_guest->l_start = lock.l_start;
                    lock_guest->l_len = lock.l_len;
                    lock_guest->l_pid = lock.l_pid;
                }
            }
            break;
        case NEUTRAL_F_SETLK64:/*13*/
            {
                struct neutral_flock64_32 *lock_guest = (struct neutral_flock64_32 *) g_2_h(opt_p);
                struct neutral_flock_64 lock;

                if (opt_p == 0 || opt_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    lock.l_type = lock_guest->l_type;
                    lock.l_whence = lock_guest->l_whence;
                    lock.l_start = lock_guest->l_start;
                    lock.l_len = lock_guest->l_len;
                    lock.l_pid = lock_guest->l_pid;
                    res = syscall_neutral_64(PR_fcntl, fd, NEUTRAL_F_SETLK, (uint64_t)&lock, 0, 0, 0);
                }
            }
            break;
        case NEUTRAL_F_SETLKW64:/*14*/
            {
                struct neutral_flock64_32 *lock_guest = (struct neutral_flock64_32 *) g_2_h(opt_p);
                struct neutral_flock_64 lock;

                if (opt_p == 0 || opt_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    lock.l_type = lock_guest->l_type;
                    lock.l_whence = lock_guest->l_whence;
                    lock.l_start = lock_guest->l_start;
                    lock.l_len = lock_guest->l_len;
                    lock.l_pid = lock_guest->l_pid;
                    res = syscall_neutral_64(PR_fcntl, fd, NEUTRAL_F_SETLKW, (uint64_t)&lock, 0, 0, 0);
                }
            }
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
                fatal("unsupported fnctl64 command %d\n", cmd);
    }

    return res;
}

static int llseek_neutral(uint32_t fd_p, uint32_t offset_high_p, uint32_t offset_low_p, uint32_t result_p, uint32_t whence_p)
{
    int fd = (int) fd_p;
    uint32_t offset_high = offset_high_p;
    uint32_t offset_low = offset_low_p;
    int64_t *result = (int64_t *) g_2_h(result_p);
    unsigned int whence = (unsigned int) whence_p;
    int64_t offset = ((int64_t)offset_high << 32) | offset_low;
    long lseek_res;

    lseek_res = syscall_neutral_64(PR_lseek, fd, offset, whence, 0, 0, 0);
    *result = lseek_res;

    return lseek_res<0?lseek_res:0;
}

static int clock_gettime_neutral(uint32_t clk_id_p, uint32_t tp_p)
{
    long res;
    clockid_t clk_id = (clockid_t) clk_id_p;
    struct neutral_timespec_32 *tp_guest = (struct neutral_timespec_32 *) g_2_h(tp_p);
    struct neutral_timespec_64 tp;

    res = syscall_neutral_64(PR_clock_gettime, clk_id, (uint64_t)&tp, 0, 0, 0, 0);

    tp_guest->tv_sec = tp.tv_sec;
    tp_guest->tv_nsec = tp.tv_nsec;

    return res;
}

static int gettimeofday_neutral(uint32_t tv_p, uint32_t tz_p)
{
    long res;
    struct neutral_timeval_32 *tv_guest = (struct neutral_timeval_32 *) g_2_h(tv_p);
    struct neutral_timezone *tz = (struct neutral_timezone *) g_2_h(tz_p);
    struct neutral_timeval_64 tv;

    if (tv_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall_neutral_64(PR_gettimeofday, (uint64_t)(tv_p?&tv:NULL), (uint64_t)(tz_p?tz:NULL), 0, 0 ,0, 0);

        if (tv_p) {
            tv_guest->tv_sec = tv.tv_sec;
            tv_guest->tv_usec = tv.tv_usec;
        }
    }

    return res;
}

/* FIXME: use alloca to allocate space for ptr */
/* FIXME: work only under proot */
static int execve_neutral(uint32_t filename_p,uint32_t argv_p,uint32_t envp_p)
{
    long res;
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

    res = syscall_neutral_64(PR_execve, (uint64_t)filename, (uint64_t)argv, (uint64_t)envp, 0, 0, 0);

    return res;
}

static int utimensat_neutral(uint32_t dirfd_p, uint32_t pathname_p, uint32_t times_p, uint32_t flags_p)
{
    long res;
    int dirfd = (int) dirfd_p;
    char * pathname = (char *) g_2_h(pathname_p);
    struct neutral_timespec_32 *times_guest = (struct neutral_timespec_32 *) g_2_h(times_p);
    int flags = (int) flags_p;
    struct neutral_timespec_64 times[2];

    if (times_p) {
        times[0].tv_sec = times_guest[0].tv_sec;
        times[0].tv_nsec = times_guest[0].tv_nsec;
        times[1].tv_sec = times_guest[1].tv_sec;
        times[1].tv_nsec = times_guest[1].tv_nsec;
    }

    res = syscall_neutral_64(PR_utimensat, dirfd, (uint64_t)(pathname_p?pathname:NULL), (uint64_t)(times_p?times:NULL), flags, 0, 0);

    return res;
}

static int sysinfo_neutral(uint32_t info_p)
{
    struct neutral_sysinfo_32 *sysinfo_guest = (struct neutral_sysinfo_32 *) g_2_h(info_p);
    struct neutral_sysinfo_64 sysinfo;
    long res;

    if (info_p == 0 || info_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall_neutral_64(PR_sysinfo,(uint64_t) &sysinfo, 0, 0, 0, 0, 0);
        sysinfo_guest->uptime = sysinfo.uptime;
        sysinfo_guest->loads[0] = sysinfo.loads[0];
        sysinfo_guest->loads[1] = sysinfo.loads[1];
        sysinfo_guest->loads[2] = sysinfo.loads[2];
        sysinfo_guest->totalram = sysinfo.totalram;
        sysinfo_guest->freeram = sysinfo.freeram;
        sysinfo_guest->sharedram = sysinfo.sharedram;
        sysinfo_guest->bufferram = sysinfo.bufferram;
        sysinfo_guest->totalswap = sysinfo.totalswap;
        sysinfo_guest->freeswap = sysinfo.freeswap;
        sysinfo_guest->procs = sysinfo.procs;
        sysinfo_guest->totalhigh = sysinfo.totalhigh;
        sysinfo_guest->freehigh = sysinfo.freehigh;
        sysinfo_guest->mem_unit = sysinfo.mem_unit;
    }

    return res;
}

static int newselect_neutral(uint32_t nfds_p, uint32_t readfds_p, uint32_t writefds_p, uint32_t exceptfds_p, uint32_t timeout_p)
{
    long res;
    int nfds = (int) nfds_p;
    neutral_fd_set *readfds = (neutral_fd_set *) g_2_h(readfds_p);
    neutral_fd_set *writefds = (neutral_fd_set *) g_2_h(writefds_p);
    neutral_fd_set *exceptfds = (neutral_fd_set *) g_2_h(exceptfds_p);
    struct neutral_timeval_32 *timeout_guest = (struct neutral_timeval_32 *) g_2_h(timeout_p);
    struct neutral_timeval_64 timeout;

    if (timeout_p) {
        timeout.tv_sec = timeout_guest->tv_sec;
        timeout.tv_usec = timeout_guest->tv_usec;
    };

    res = syscall_neutral_64(PR_select, nfds, (uint64_t) (readfds_p?readfds:NULL), (uint64_t) (writefds_p?writefds:NULL),
                             (uint64_t) (exceptfds_p?exceptfds:NULL), (uint64_t) (timeout_p?&timeout:NULL), 0);

    if (timeout_p) {
        timeout_guest->tv_sec = timeout.tv_sec;
        timeout_guest->tv_usec = timeout.tv_usec;
    };

    return res;
}

static int getrusage_neutral(uint32_t who_p, uint32_t usage_p)
{
    long res;
    int who = (int) who_p;
    struct neutral_rusage_32 *usage_guest = (struct neutral_rusage_32 *) g_2_h(usage_p);
    struct neutral_rusage_64 rusage;

    if (usage_p == 0 || usage_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall_neutral_64(PR_getrusage, who, (uint64_t)&rusage, 0, 0, 0, 0);

        usage_guest->ru_utime.tv_sec = rusage.ru_utime.tv_sec;
        usage_guest->ru_utime.tv_usec = rusage.ru_utime.tv_usec;
        usage_guest->ru_stime.tv_sec = rusage.ru_stime.tv_sec;
        usage_guest->ru_stime.tv_usec = rusage.ru_stime.tv_usec;
        usage_guest->ru_maxrss = rusage.ru_maxrss;
        usage_guest->ru_ixrss = rusage.ru_ixrss;
        usage_guest->ru_idrss = rusage.ru_idrss;
        usage_guest->ru_isrss = rusage.ru_isrss;
        usage_guest->ru_minflt = rusage.ru_minflt;
        usage_guest->ru_majflt = rusage.ru_majflt;
        usage_guest->ru_nswap = rusage.ru_nswap;
        usage_guest->ru_inblock = rusage.ru_inblock;
        usage_guest->ru_oublock = rusage.ru_oublock;
        usage_guest->ru_msgsnd = rusage.ru_msgsnd;
        usage_guest->ru_msgrcv = rusage.ru_msgrcv;
        usage_guest->ru_nsignals = rusage.ru_nsignals;
        usage_guest->ru_nvcsw = rusage.ru_nvcsw;
        usage_guest->ru_nivcsw = rusage.ru_nivcsw;
    }

    return res;
}

/* FIXME : use alloca for buffer */
static int getdents_neutral(uint32_t fd_p, uint32_t dirp_p, uint32_t count_p)
{
    unsigned int fd = (unsigned int) fd_p;
    struct neutral_linux_dirent_32  *dirp = (struct neutral_linux_dirent_32 *) g_2_h(dirp_p);
    unsigned int count = (unsigned int) count_p;
    char buffer[4096];
    struct neutral_linux_dirent_64 *dirp_64 = (struct neutral_linux_dirent_64 *) buffer;
    int res_32 = 0;
    long res;

    res = syscall_neutral_64(PR_getdents, fd, (uint64_t)buffer, sizeof(buffer), 0, 0, 0);

    res_32 = res<0?res:0;
    if (res_32 >= 0) {
        if (dirp_64->d_reclen - 8 >= count)
            res_32 = -EINVAL;
        else {
            while(res > 0 && count > dirp_64->d_reclen - 8) {
                dirp->d_ino = dirp_64->d_ino;
                dirp->d_off = dirp_64->d_off;
                dirp->d_reclen = dirp_64->d_reclen - 8;
                // we copy according to dirp_x86->d_reclen size since there is d_type hidden field
                memcpy(dirp->d_name, dirp_64->d_name, dirp_64->d_reclen - 8 - 8 - 2);
                res -= dirp_64->d_reclen;
                dirp_64 = (struct neutral_linux_dirent_64 *) ((long)dirp_64 + dirp_64->d_reclen);
                res_32 += dirp->d_reclen;
                count -= dirp->d_reclen;
                dirp = (struct neutral_linux_dirent_32 *) ((long)dirp + dirp->d_reclen);
            }
        }
    }

    return res_32;
}

static int nanosleep_neutral(uint32_t req_p, uint32_t rem_p)
{
    struct neutral_timespec_32 *req_guest = (struct neutral_timespec_32 *) g_2_h(req_p);
    struct neutral_timespec_32 *rem_guest = (struct neutral_timespec_32 *) g_2_h(rem_p);
    struct neutral_timespec_64 req;
    struct neutral_timespec_64 rem;
    long res;

    req.tv_sec = req_guest->tv_sec;
    req.tv_nsec = req_guest->tv_nsec;

    //do x86 syscall
    res = syscall_neutral_64(PR_nanosleep, (uint64_t)&req, (uint64_t)(rem_p?&rem:NULL), 0, 0, 0, 0);

    if (rem_p) {
        rem_guest->tv_sec = rem.tv_sec;
        rem_guest->tv_nsec = rem.tv_nsec;
    }

    return res;
}

static int setitimer_neutral(uint32_t which_p, uint32_t new_value_p, uint32_t old_value_p)
{
    long res;
    int which = (int) which_p;
    struct neutral_itimerval_32 *new_value_guest = (struct neutral_itimerval_32 *) g_2_h(new_value_p);
    struct neutral_itimerval_32 *old_value_guest = (struct neutral_itimerval_32 *) g_2_h(old_value_p);
    struct neutral_itimerval_64 new_value;
    struct neutral_itimerval_64 old_value;

    if (new_value_p == 0xffffffff || old_value_p == 0xffffffff)
        res = -EFAULT;
    else {
        new_value.it_interval.tv_sec = new_value_guest->it_interval.tv_sec;
        new_value.it_interval.tv_usec = new_value_guest->it_interval.tv_usec;
        new_value.it_value.tv_sec = new_value_guest->it_value.tv_sec;
        new_value.it_value.tv_usec = new_value_guest->it_value.tv_usec;

        res = syscall_neutral_64(PR_setitimer, which, (uint64_t)&new_value, (uint64_t)(old_value_p?&old_value:NULL), 0, 0, 0);

        if (old_value_p) {
            old_value_guest->it_interval.tv_sec = old_value.it_interval.tv_sec;
            old_value_guest->it_interval.tv_usec = old_value.it_interval.tv_usec;
            old_value_guest->it_value.tv_sec = old_value.it_value.tv_sec;
            old_value_guest->it_value.tv_usec = old_value.it_value.tv_usec;
        }
    }

    return res;
}

static int times_neutral(uint32_t buf_p)
{
    long res;
    struct neutral_tms_32 *buf_guest = (struct neutral_tms_32 *) g_2_h(buf_p);
    struct neutral_tms_64 buf;

    res = syscall_neutral_64(PR_times, (uint64_t)&buf, 0, 0, 0, 0, 0);
    if (buf_p) {
        buf_guest->tms_utime = buf.tms_utime;
        buf_guest->tms_stime = buf.tms_stime;
        buf_guest->tms_cutime = buf.tms_cutime;
        buf_guest->tms_cstime = buf.tms_cstime;
    }

    return res;
}

static int getitimer_neutral(uint32_t which_p, uint32_t curr_value_p)
{
    long res;
    int which = (int) which_p;
    struct neutral_itimerval_32 *curr_value_guest = (struct neutral_itimerval_32 *) g_2_h(curr_value_p);
    struct neutral_itimerval_64 curr_value;

    if (curr_value_p == 0xffffffff || curr_value_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall_neutral_64(PR_getitimer, which, (uint64_t)&curr_value, 0, 0, 0, 0);
        curr_value_guest->it_interval.tv_sec = curr_value.it_interval.tv_sec;
        curr_value_guest->it_interval.tv_usec = curr_value.it_interval.tv_usec;
        curr_value_guest->it_value.tv_sec = curr_value.it_value.tv_sec;
        curr_value_guest->it_value.tv_usec = curr_value.it_value.tv_usec;
    }

    return res;
}

static int setrlimit_neutral(uint32_t resource_p, uint32_t rlim_p)
{
    int res;
    int ressource = (int) resource_p;
    struct neutral_rlimit_32 *rlim_guest = (struct neutral_rlimit_32 *) g_2_h(rlim_p);
    struct neutral_rlimit_64 rlim;

    rlim.rlim_cur = rlim_guest->rlim_cur;
    rlim.rlim_max = rlim_guest->rlim_max;
    res = syscall_neutral_64(PR_setrlimit, ressource, (uint64_t)&rlim, 0, 0, 0, 0);

    return res;
}

static int fstatfs_neutral(uint32_t fd_p, uint32_t buf_p)
{
    int fd = (int) fd_p;
    struct neutral_statfs_32 *buf_guest = (struct neutral_statfs_32 *) g_2_h(buf_p);
    struct neutral_statfs_64 buf;
    long res;

    if (buf_p == 0 || buf_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall_neutral_64(PR_fstatfs,(long) fd, (uint64_t) &buf, 0, 0, 0, 0);

        buf_guest->f_type = buf.f_type;
        buf_guest->f_bsize = buf.f_bsize;
        buf_guest->f_blocks = buf.f_blocks;
        buf_guest->f_bfree = buf.f_bfree;
        buf_guest->f_bavail = buf.f_bavail;
        buf_guest->f_files = buf.f_files;
        buf_guest->f_ffree = buf.f_ffree;
        buf_guest->f_fsid.val[0] = 0;
        buf_guest->f_fsid.val[1] = 0;
        buf_guest->f_namelen = buf.f_namelen;
        buf_guest->f_frsize = buf.f_frsize;
        buf_guest->f_flags = 0;
    }

    return res;
}

static int readv_neutral(uint32_t fd_p, uint32_t iov_p, uint32_t iovcnt_p)
{
    long res;
    int fd = (int) fd_p;
    struct neutral_iovec_32 *iov_guest = (struct neutral_iovec_32 *) g_2_h(iov_p);
    int iovcnt = (int) iovcnt_p;
    struct neutral_iovec_64 *iov;
    int i;

    if (iovcnt < 0)
        return -EINVAL;
    iov = (struct neutral_iovec_64 *) alloca(sizeof(struct neutral_iovec_64) * iovcnt);
    for(i = 0; i < iovcnt; i++) {
        iov[i].iov_base = ptr_2_int(g_2_h(iov_guest[i].iov_base));
        iov[i].iov_len = iov_guest[i].iov_len;
    }
    res = syscall_neutral_64(PR_readv, fd, (uint64_t) iov, iovcnt, 0, 0, 0);

    return res;
}

static int writev_neutral(uint32_t fd_p, uint32_t iov_p, uint32_t iovcnt_p)
{
    long res;
    int fd = (int) fd_p;
    struct neutral_iovec_32 *iov_guest = (struct neutral_iovec_32 *) g_2_h(iov_p);
    int iovcnt = (int) iovcnt_p;
    struct neutral_iovec_64 *iov;
    int i;

    if (iovcnt < 0)
        return -EINVAL;
    iov = (struct neutral_iovec_64 *) alloca(sizeof(struct neutral_iovec_64) * iovcnt);
    for(i = 0; i < iovcnt; i++) {
        iov[i].iov_base = ptr_2_int(g_2_h(iov_guest[i].iov_base));
        iov[i].iov_len = iov_guest[i].iov_len;
    }
    res = syscall_neutral_64(PR_writev, fd, (uint64_t) iov, iovcnt, 0, 0, 0);

    return res;
}

static int prctl_neutral(uint32_t option_p, uint32_t arg2_p, uint32_t arg3_p, uint32_t arg4_p, uint32_t arg5_p)
{
    long res;
    int option = (int) option_p;

    switch(option) {
        case NEUTRAL_PR_GET_PDEATHSIG:/*2*/
            res = syscall_neutral_64(PR_prctl, option, (uint64_t) g_2_h(arg2_p), arg3_p, arg4_p, arg5_p, 0);
            break;
        default:
            res = syscall_neutral_64(PR_prctl, option, arg2_p, arg3_p, arg4_p, arg5_p, 0);
    }

    return res;
}

static int timer_create_neutral(uint32_t clockid_p, uint32_t sevp_p, uint32_t timerid_p)
{
    long res;
    clockid_t clockid = (clockid_t) clockid_p;
    struct neutral_sigevent_32 *sevp_guest = (struct neutral_sigevent_32 *) g_2_h(sevp_p);
    timer_t *timerid = (timer_t *) g_2_h(timerid_p);
    struct neutral_sigevent_64 evp;

    if (sevp_p) {
        switch(sevp_guest->sigev_notify) {
            case NEUTRAL_SIGEV_NONE:
                evp.sigev_notify = NEUTRAL_SIGEV_NONE;
                break;
            case NEUTRAL_SIGEV_SIGNAL:
                evp.sigev_notify = NEUTRAL_SIGEV_SIGNAL;
                evp.sigev_signo = sevp_guest->sigev_signo;
                /* FIXME: need to check kernel since doc is not clear which union part is use */
                //evp.sigev_value.sival_ptr = (void *) g_2_h(sevp_guest->sigev_value.sival_ptr);
                evp.sigev_value.sival_int = sevp_guest->sigev_value.sival_int;
                break;
            default:
                /* FIXME : add SIGEV_THREAD + SIGEV_THREAD_ID support */
                assert(0);
        }
    }

    res = syscall_neutral_64(PR_timer_create, clockid, (uint64_t) (sevp_p?&evp:NULL), (uint64_t) timerid, 0, 0, 0);

    return res;
}

static int clock_getres_neutral(uint32_t clk_id_p, uint32_t res_p)
{
    long result;
    clockid_t clk_id = (clockid_t) clk_id_p;
    struct neutral_timespec_32 *res_guest = (struct neutral_timespec_32 *) g_2_h(res_p);
    struct neutral_timespec_64 res;

    result = syscall_neutral_64(PR_clock_getres, clk_id, (uint64_t) (res_p?&res:NULL), 0, 0, 0, 0);
    if (res_p) {
        res_guest->tv_sec = res.tv_sec;
        res_guest->tv_nsec = res.tv_nsec;
    }

    return result;
}

static int clock_nanosleep_neutral(uint32_t clock_id_p, uint32_t flags_p, uint32_t request_p, uint32_t remain_p)
{
    clockid_t clock_id = (clockid_t) clock_id_p;
    int flags = (int) flags_p;
    struct neutral_timespec_32 *request_guest = (struct neutral_timespec_32 *) g_2_h(request_p);
    struct neutral_timespec_32 *remain_guest = (struct neutral_timespec_32 *) g_2_h(remain_p);
    struct neutral_timespec_64 request;
    struct neutral_timespec_64 remain;
    long res;

    request.tv_sec = request_guest->tv_sec;
    request.tv_nsec = request_guest->tv_nsec;

    res = syscall_neutral_64(PR_clock_nanosleep, clock_id, flags, (uint64_t) &request, (uint64_t) (remain_p?&remain:NULL), 0, 0);
    if (remain_p) {
        remain_guest->tv_sec = remain.tv_sec;
        remain_guest->tv_nsec = remain.tv_nsec;
    }

    return res;
}

static int fstatfs64_neutral(uint32_t fd_p, uint32_t sz, uint32_t buf_p)
{
    unsigned int fd = (unsigned int) fd_p;
    struct neutral_statfs64_32 *buf_guest = (struct neutral_statfs64_32 *) g_2_h(buf_p);
    struct neutral_statfs64_64 buf;
    long res;

    if (buf_p == 0 || buf_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall_neutral_64(PR_fstatfs, fd, (uint64_t)&buf, 0, 0, 0, 0);

        buf_guest->f_type = buf.f_type;
        buf_guest->f_bsize = buf.f_bsize;
        buf_guest->f_blocks = buf.f_blocks;
        buf_guest->f_bfree = buf.f_bfree;
        buf_guest->f_bavail = buf.f_bavail;
        buf_guest->f_files = buf.f_files;
        buf_guest->f_ffree = buf.f_ffree;
        buf_guest->f_fsid.val[0] = 0;
        buf_guest->f_fsid.val[1] = 0;
        buf_guest->f_namelen = buf.f_namelen;
        buf_guest->f_frsize = buf.f_frsize;
        buf_guest->f_flags = buf.f_flags;
    }

    return res;
}

static int mq_notify_neutral(uint32_t mqdes_p, uint32_t sevp_p)
{
    long res;
    neutral_mqd_t mqdes = (neutral_mqd_t) mqdes_p;
    struct neutral_sigevent_32 *sevp_guest = (struct neutral_sigevent_32 *) g_2_h(sevp_p);
    struct neutral_sigevent_64 evp;

    if (sevp_p) {
        switch(sevp_guest->sigev_notify) {
            case NEUTRAL_SIGEV_NONE:
                evp.sigev_notify = NEUTRAL_SIGEV_NONE;
                break;
            case NEUTRAL_SIGEV_SIGNAL:
                evp.sigev_notify = NEUTRAL_SIGEV_SIGNAL;
                evp.sigev_signo = sevp_guest->sigev_signo;
                /* FIXME: need to check kernel since doc is not clear which union part is use */
                //evp.sigev_value.sival_ptr = (void *) g_2_h(sevp_guest->sigev_value.sival_ptr);
                evp.sigev_value.sival_int = sevp_guest->sigev_value.sival_int;
                break;
            case NEUTRAL_SIGEV_THREAD:
                /* sigev_signo is in this case a file descriptor of an AF_NETLINK socket */
                evp.sigev_notify = NEUTRAL_SIGEV_THREAD;
                evp.sigev_signo = sevp_guest->sigev_signo;
                evp.sigev_value.sival_ptr = ptr_2_int(g_2_h(sevp_guest->sigev_value.sival_ptr));
                break;
            default:
                /* let kernel return with EINVAL error */
                evp.sigev_notify = sevp_guest->sigev_notify;
                break;
        }
    }

    res = syscall_neutral_64(PR_mq_notify, mqdes, (uint64_t) (sevp_p?&evp:NULL), 0, 0, 0, 0);

    return res;
}

static int waitid_neutral(uint32_t which_p, uint32_t upid_p, uint32_t infop_p, uint32_t options_p, uint32_t rusage_p)
{
    long res;
    int which = (int) which_p;
    pid_t upid = (pid_t) upid_p;
    struct neutral_siginfo_32 *infop_guest = (struct neutral_siginfo_32 *) g_2_h(infop_p);
    int options = (int) options_p;
    struct neutral_rusage_32 *rusage_guest = (struct neutral_rusage_32 *) g_2_h(rusage_p);
    struct neutral_siginfo_64 infop;
    struct neutral_rusage_64 rusage;

    res = syscall_neutral_64(PR_waitid, which, upid, (uint64_t) &infop, options, (uint64_t) (rusage_p?&rusage:NULL), 0);
    if (infop_p) {
        infop_guest->si_signo = infop.si_signo;
        infop_guest->si_errno = infop.si_errno;
        infop_guest->si_code = infop.si_code;
        infop_guest->_sifields._sigchld._si_pid = infop._sifields._sigchld._si_pid;
        infop_guest->_sifields._sigchld._si_uid = infop._sifields._sigchld._si_uid;
        infop_guest->_sifields._sigchld._si_status = infop._sifields._sigchld._si_status;
    }
    if (rusage_p) {
        rusage_guest->ru_utime.tv_sec = rusage.ru_utime.tv_sec;
        rusage_guest->ru_utime.tv_usec = rusage.ru_utime.tv_usec;
        rusage_guest->ru_stime.tv_sec = rusage.ru_stime.tv_sec;
        rusage_guest->ru_stime.tv_usec = rusage.ru_stime.tv_usec;
        rusage_guest->ru_maxrss = rusage.ru_maxrss;
        rusage_guest->ru_ixrss = rusage.ru_ixrss;
        rusage_guest->ru_idrss = rusage.ru_idrss;
        rusage_guest->ru_isrss = rusage.ru_isrss;
        rusage_guest->ru_minflt = rusage.ru_minflt;
        rusage_guest->ru_majflt = rusage.ru_majflt;
        rusage_guest->ru_nswap = rusage.ru_nswap;
        rusage_guest->ru_inblock = rusage.ru_inblock;
        rusage_guest->ru_oublock = rusage.ru_oublock;
        rusage_guest->ru_msgsnd = rusage.ru_msgsnd;
        rusage_guest->ru_msgrcv = rusage.ru_msgrcv;
        rusage_guest->ru_nsignals = rusage.ru_nsignals;
        rusage_guest->ru_nvcsw = rusage.ru_nvcsw;
        rusage_guest->ru_nivcsw = rusage.ru_nivcsw;
    }

    return res;
}

static int timer_gettime_neutral(uint32_t timerid_p, uint32_t curr_value_p)
{
    long res;
    timer_t timerid = (timer_t)(long) timerid_p;
    struct neutral_itimerspec_32 *curr_value_guest = (struct neutral_itimerspec_32 *) g_2_h(curr_value_p);
    struct neutral_itimerspec_64 curr_value;

    if (curr_value_p == 0)
        res = -EFAULT;
    else {
        res = syscall_neutral_64(PR_timer_gettime, (uint64_t) timerid, (uint64_t) &curr_value, 0, 0, 0, 0);
        curr_value_guest->it_interval.tv_sec = curr_value.it_interval.tv_sec;
        curr_value_guest->it_interval.tv_nsec = curr_value.it_interval.tv_nsec;
        curr_value_guest->it_value.tv_sec = curr_value.it_value.tv_sec;
        curr_value_guest->it_value.tv_nsec = curr_value.it_value.tv_nsec;
    }

    return res;
}

static int mq_open_neutral(uint32_t name_p, uint32_t oflag_p, uint32_t mode_p, uint32_t attr_p)
{
    long res;
    char *name = (char *) g_2_h(name_p);
    int oflag = (int) oflag_p;
    mode_t mode = (mode_t) mode_p;
    struct neutral_mq_attr_32 *attr_guest = (struct neutral_mq_attr_32 *) g_2_h(attr_p);
    struct neutral_mq_attr_64 attr;

    if (attr_p) {
        attr.mq_flags = attr_guest->mq_flags;
        attr.mq_maxmsg = attr_guest->mq_maxmsg;
        attr.mq_msgsize = attr_guest->mq_msgsize;
        attr.mq_curmsgs = attr_guest->mq_curmsgs;
    }    
    res = syscall_neutral_64(PR_mq_open, (uint64_t) name, oflag, mode, (uint64_t) (attr_p?&attr:NULL), 0, 0);

    return res;
}

#define IPC_64      (0x100)
static int shmctl_neutral(uint32_t shmid_p, uint32_t cmd_p, uint32_t buf_p)
{
    long res;
    int shmid = (int) shmid_p;
    int cmd = (int) cmd_p;

    assert(cmd&IPC_64);
    cmd &= ~IPC_64;
    switch(cmd) {
        case NEUTRAL_IPC_RMID:
        case NEUTRAL_SHM_LOCK:
        case NEUTRAL_SHM_UNLOCK:
            res = syscall_neutral_64(PR_shmctl, shmid, cmd, (uint64_t) NULL, 0, 0, 0);
            break;
        case NEUTRAL_IPC_SET:
            {
                struct neutral_shmid64_ds_32 *buf_guest = (struct neutral_shmid64_ds_32 *) g_2_h(buf_p);
                struct neutral_shmid64_ds_64 buf;

                buf.shm_perm.key = buf_guest->shm_perm.key;
                buf.shm_perm.uid = buf_guest->shm_perm.uid;
                buf.shm_perm.gid = buf_guest->shm_perm.gid;
                buf.shm_perm.cuid = buf_guest->shm_perm.cuid;
                buf.shm_perm.cgid = buf_guest->shm_perm.cgid;
                buf.shm_perm.mode = buf_guest->shm_perm.mode;
                buf.shm_perm.seq = buf_guest->shm_perm.seq;
                buf.shm_segsz = buf_guest->shm_segsz;
                buf.shm_atime = buf_guest->shm_atime;
                buf.shm_dtime = buf_guest->shm_dtime;
                buf.shm_ctime = buf_guest->shm_ctime;
                buf.shm_cpid = buf_guest->shm_cpid;
                buf.shm_lpid = buf_guest->shm_lpid;

                res = syscall_neutral_64(PR_shmctl, shmid, cmd, (uint64_t) &buf, 0, 0, 0);

                buf_guest->shm_perm.key = buf.shm_perm.key;
                buf_guest->shm_perm.uid = buf.shm_perm.uid;
                buf_guest->shm_perm.gid = buf.shm_perm.gid;
                buf_guest->shm_perm.cuid = buf.shm_perm.cuid;
                buf_guest->shm_perm.cgid = buf.shm_perm.cgid;
                buf_guest->shm_perm.mode = buf.shm_perm.mode;
                buf_guest->shm_perm.seq = buf.shm_perm.seq;
                buf_guest->shm_segsz = buf.shm_segsz;
                buf_guest->shm_atime = buf.shm_atime;
                buf_guest->shm_dtime = buf.shm_dtime;
                buf_guest->shm_ctime = buf.shm_ctime;
                buf_guest->shm_cpid = buf.shm_cpid;
                buf_guest->shm_lpid = buf.shm_lpid;
            }
            break;
        case NEUTRAL_SHM_STAT:
        case NEUTRAL_IPC_STAT:
            {
                struct neutral_shmid64_ds_32 *buf_guest = (struct neutral_shmid64_ds_32 *) g_2_h(buf_p);
                struct neutral_shmid64_ds_64 buf;

                res = syscall_neutral_64(PR_shmctl, shmid, cmd, (uint64_t) &buf, 0, 0, 0);

                buf_guest->shm_perm.key = buf.shm_perm.key;
                buf_guest->shm_perm.uid = buf.shm_perm.uid;
                buf_guest->shm_perm.gid = buf.shm_perm.gid;
                buf_guest->shm_perm.cuid = buf.shm_perm.cuid;
                buf_guest->shm_perm.cgid = buf.shm_perm.cgid;
                buf_guest->shm_perm.mode = buf.shm_perm.mode;
                buf_guest->shm_perm.seq = buf.shm_perm.seq;
                buf_guest->shm_segsz = buf.shm_segsz;
                buf_guest->shm_atime = buf.shm_atime;
                buf_guest->shm_dtime = buf.shm_dtime;
                buf_guest->shm_ctime = buf.shm_ctime;
                buf_guest->shm_cpid = buf.shm_cpid;
                buf_guest->shm_lpid = buf.shm_lpid;
                buf_guest->shm_nattch = buf.shm_nattch;
            }
            break;
        case NEUTRAL_IPC_INFO:
            {
                struct neutral_shminfo64_32 *buf_guest = (struct neutral_shminfo64_32 *) g_2_h(buf_p);
                struct neutral_shminfo64_64 buf;

                res = syscall_neutral_64(PR_shmctl, shmid, cmd, (uint64_t) &buf, 0, 0, 0);

                buf_guest->shmmax = buf.shmmax;
                buf_guest->shmmin = buf.shmmin;
                buf_guest->shmmni = buf.shmmni;
                buf_guest->shmseg = buf.shmseg;
                buf_guest->shmall = buf.shmall;
            }
            break;
        case NEUTRAL_SHM_INFO:
            {
                struct neutral_shm_info_32 *buf_guest = (struct neutral_shm_info_32 *) g_2_h(buf_p);
                struct neutral_shm_info_64 buf;

                res = syscall_neutral_64(PR_shmctl, shmid, cmd, (uint64_t) &buf, 0, 0, 0);

                buf_guest->used_ids = buf.used_ids;
                buf_guest->shm_tot = buf.shm_tot;
                buf_guest->shm_rss = buf.shm_rss;
                buf_guest->shm_swp = buf.shm_swp;
                buf_guest->swap_attempts = buf.swap_attempts;
                buf_guest->swap_successes = buf.swap_successes;
            }
            break;
        default:
            fatal("shmctl_neutral: unsupported command %d\n", cmd);
    }

    return res;
}

static int keyctl_neutral(uint32_t cmd_p, uint32_t arg2_p, uint32_t arg3_p, uint32_t arg4_p, uint32_t arg5_p)
{
    long res;
    int cmd = (int) cmd_p;

    switch(cmd) {
        case NEUTRAL_KEYCTL_GET_KEYRING_ID:
            res = syscall_neutral_64(PR_keyctl, cmd, (neutral_key_serial_t) arg2_p, (int) arg3_p, arg4_p, arg5_p, 0);
            break;
        case NEUTRAL_KEYCTL_REVOKE:
            res = syscall_neutral_64(PR_keyctl, cmd, (neutral_key_serial_t) arg2_p, arg3_p, arg4_p, arg5_p, 0);
            break;
        case NEUTRAL_KEYCTL_READ:
            res = syscall_neutral_64(PR_keyctl, cmd, (neutral_key_serial_t) arg2_p, (uint64_t) g_2_h(arg3_p), (size_t) arg4_p, arg5_p, 0);
            break;
        default:
            fatal("keyctl cmd %d not supported\n", cmd);
    }

    return res;
}

static int futimesat_neutral(uint32_t dirfd_p, uint32_t pathname_p, uint32_t times_p)
{
    long res;
    int dirfd = (int) dirfd_p;
    char * pathname = (char *) g_2_h(pathname_p);
    struct neutral_timespec_32 *times_guest = (struct neutral_timespec_32 *) g_2_h(times_p);
    struct neutral_timespec_64 times[2];

    if (times_p) {
        times[0].tv_sec = times_guest[0].tv_sec;
        times[0].tv_nsec = times_guest[0].tv_nsec;
        times[1].tv_sec = times_guest[1].tv_sec;
        times[1].tv_nsec = times_guest[1].tv_nsec;
    }
    res = syscall_neutral_64(PR_futimesat, dirfd, (uint64_t) (pathname_p?pathname:NULL), (uint64_t) (times_p?times:NULL), 0, 0, 0);

    return res;
}

static int mq_timedsend_neutral(uint32_t mqdes_p, uint32_t msg_ptr_p, uint32_t msg_len_p, uint32_t msg_prio_p, uint32_t abs_timeout_p)
{
    int res;
    neutral_mqd_t mqdes = (neutral_mqd_t) mqdes_p;
    char *msg_ptr = (char *) g_2_h(msg_ptr_p);
    size_t msg_len = (size_t) msg_len_p;
    unsigned int msg_prio = (unsigned int) msg_prio_p;
    struct neutral_timespec_32 *abs_timeout_guest = (struct neutral_timespec_32 *) g_2_h(abs_timeout_p);
    struct neutral_timespec_64 abs_timeout;

    if (abs_timeout_p) {
        abs_timeout.tv_sec = abs_timeout_guest->tv_sec;
        abs_timeout.tv_nsec = abs_timeout_guest->tv_nsec;
    }
    res = syscall_neutral_64(PR_mq_timedsend, mqdes, (uint64_t) msg_ptr, msg_len, msg_prio, (uint64_t) (abs_timeout_p?&abs_timeout:NULL), 0);

    return res;
}

static int semctl_neutral(uint32_t semid_p, uint32_t semnum_p, uint32_t cmd_p, uint32_t arg0_p)
{
    long res;
    int semid = (int) semid_p;
    int semnum = (int) semnum_p;
    int cmd = (int) cmd_p;

    assert(cmd&IPC_64);
    cmd &= ~IPC_64;
    switch(cmd) {
        case NEUTRAL_GETVAL:
        case NEUTRAL_GETNCNT:
        case NEUTRAL_IPC_RMID:
        case NEUTRAL_GETPID:
        case NEUTRAL_GETZCNT:
            res = syscall_neutral_64(PR_semctl, semid, semnum, cmd, 0, 0, 0);
            break;
        case NEUTRAL_IPC_SET:
            {
                struct neutral_semid64_ds_32 *arg_guest = (struct neutral_semid64_ds_32 *) g_2_h(arg0_p);
                struct neutral_semid64_ds_64 buf;

                if (arg0_p == 0 || arg0_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    buf.sem_perm.key = arg_guest->sem_perm.key;
                    buf.sem_perm.uid = arg_guest->sem_perm.uid;
                    buf.sem_perm.gid = arg_guest->sem_perm.gid;
                    buf.sem_perm.cuid = arg_guest->sem_perm.cuid;
                    buf.sem_perm.cgid = arg_guest->sem_perm.cgid;
                    buf.sem_perm.mode = arg_guest->sem_perm.mode;
                    buf.sem_perm.seq = arg_guest->sem_perm.seq;
                    buf.sem_otime = arg_guest->sem_otime;
                    buf.sem_ctime = arg_guest->sem_ctime;
                    buf.sem_nsems = arg_guest->sem_nsems;
                    res = syscall_neutral_64(PR_semctl, semid, semnum, cmd, (uint64_t) &buf, 0, 0);
                    arg_guest->sem_perm.key = buf.sem_perm.key;
                    arg_guest->sem_perm.uid = buf.sem_perm.uid;
                    arg_guest->sem_perm.gid = buf.sem_perm.gid;
                    arg_guest->sem_perm.cuid = buf.sem_perm.cuid;
                    arg_guest->sem_perm.cgid = buf.sem_perm.cgid;
                    arg_guest->sem_perm.mode = buf.sem_perm.mode;
                    arg_guest->sem_perm.seq = buf.sem_perm.seq;
                    arg_guest->sem_otime = buf.sem_otime;
                    arg_guest->sem_ctime = buf.sem_ctime;
                    arg_guest->sem_nsems = buf.sem_nsems;
                }
            }
            break;
        case NEUTRAL_SEM_STAT:
        case NEUTRAL_IPC_STAT:
            {
                struct neutral_semid64_ds_32 *arg_guest = (struct neutral_semid64_ds_32 *) g_2_h(arg0_p);
                struct neutral_semid64_ds_64 buf;

                if (arg0_p == 0 || arg0_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    res = syscall_neutral_64(PR_semctl, semid, semnum, cmd, (uint64_t) &buf, 0, 0);
                    arg_guest->sem_perm.key = buf.sem_perm.key;
                    arg_guest->sem_perm.uid = buf.sem_perm.uid;
                    arg_guest->sem_perm.gid = buf.sem_perm.gid;
                    arg_guest->sem_perm.cuid = buf.sem_perm.cuid;
                    arg_guest->sem_perm.cgid = buf.sem_perm.cgid;
                    arg_guest->sem_perm.mode = buf.sem_perm.mode;
                    arg_guest->sem_perm.seq = buf.sem_perm.seq;
                    arg_guest->sem_otime = buf.sem_otime;
                    arg_guest->sem_ctime = buf.sem_ctime;
                    arg_guest->sem_nsems = buf.sem_nsems;
                }
            }
            break;
        case NEUTRAL_SEM_INFO:
        case NEUTRAL_IPC_INFO:
            {
                 struct seminfo* seminfo = (struct seminfo *) g_2_h(arg0_p);

                 res = syscall_neutral_64(PR_semctl, semid, semnum, cmd, (uint64_t) seminfo, 0, 0);
            }
            break;
        case NEUTRAL_GETALL:
            {
                unsigned short *array = (unsigned short *) g_2_h(arg0_p);

                res = syscall_neutral_64(PR_semctl, semid, semnum, cmd, (uint64_t) array, 0, 0);
            }
            break;
        case NEUTRAL_SETVAL:
            {
                int val = (int) arg0_p;

                res = syscall_neutral_64(PR_semctl, semid, semnum, cmd, val, 0, 0);
            }
            break;
        case NEUTRAL_SETALL:
            {
                unsigned short *array = (unsigned short *) g_2_h(arg0_p);

                res = syscall_neutral_64(PR_semctl, semid, semnum, cmd, (uint64_t) array, 0, 0);
            }
            break;
        default:
            warning("semctl_neutral: unsupported command %d\n", cmd);
            res = -EINVAL;
    }

    return res; 
}

static int msgsnd_neutral(uint32_t msqid_p, uint32_t msgp_p, uint32_t msgsz_p, uint32_t msgflg_p)
{
    int res;
    int msqid = (int) msqid_p;
    struct neutral_msgbuf_32 *msgp_guest = (struct neutral_msgbuf_32 *) g_2_h(msgp_p);
    size_t msgsz = (size_t) msgsz_p;
    int msgflg = (int) msgflg_p;
    struct neutral_msgbuf_64 *msgp;

    if ((int)msgsz < 0)
        return -EINVAL;

    msgp = (struct neutral_msgbuf_64 *) alloca(sizeof(uint64_t) + msgsz);
    assert(msgp != NULL);
    msgp->mtype = msgp_guest->mtype;
    if (msgsz)
        memcpy(msgp->mtext, msgp_guest->mtext, msgsz);
    res = syscall_neutral_64(PR_msgsnd, msqid, (uint64_t) msgp, msgsz, msgflg, 0, 0);

    return res;
}

static int msgctl_neutral(uint32_t msqid_p, uint32_t cmd_p, uint32_t buf_p)
{
    long res;
    int msqid = (int) msqid_p;
    int cmd = (int) cmd_p;

    assert(cmd&IPC_64);
    cmd &= ~IPC_64;
    switch(cmd) {
        case NEUTRAL_IPC_RMID:
            res = syscall_neutral_64(PR_msgctl, msqid, cmd, 0, 0, 0, 0);
            break;
        case NEUTRAL_IPC_SET:
            {
                struct neutral_msqid64_ds_32 *buf_guest = (struct neutral_msqid64_ds_32 *) g_2_h(buf_p);
                struct neutral_msqid64_ds_64 buf;

                if (buf_p == 0 || buf_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    buf.msg_perm.key = buf_guest->msg_perm.key;
                    buf.msg_perm.uid = buf_guest->msg_perm.uid;
                    buf.msg_perm.gid = buf_guest->msg_perm.gid;
                    buf.msg_perm.cuid = buf_guest->msg_perm.cuid;
                    buf.msg_perm.cgid = buf_guest->msg_perm.cgid;
                    buf.msg_perm.mode = buf_guest->msg_perm.mode;
                    buf.msg_perm.seq = buf_guest->msg_perm.seq;
                    buf.msg_stime = buf_guest->msg_stime;
                    buf.msg_rtime = buf_guest->msg_rtime;
                    buf.msg_ctime = buf_guest->msg_ctime;
                    buf.__msg_cbytes = buf_guest->__msg_cbytes;
                    buf.msg_qnum = buf_guest->msg_qnum;
                    buf.msg_qbytes = buf_guest->msg_qbytes;
                    buf.msg_lspid = buf_guest->msg_lspid;
                    buf.msg_lrpid = buf_guest->msg_lrpid;
                    res = syscall_neutral_64(PR_msgctl, msqid, cmd, (uint64_t) &buf, 0, 0, 0);
                    buf_guest->msg_perm.key = buf.msg_perm.key;
                    buf_guest->msg_perm.uid = buf.msg_perm.uid;
                    buf_guest->msg_perm.gid = buf.msg_perm.gid;
                    buf_guest->msg_perm.cuid = buf.msg_perm.cuid;
                    buf_guest->msg_perm.cgid = buf.msg_perm.cgid;
                    buf_guest->msg_perm.mode = buf.msg_perm.mode;
                    buf_guest->msg_perm.seq = buf.msg_perm.seq;
                    buf_guest->msg_stime = buf.msg_stime;
                    buf_guest->msg_rtime = buf.msg_rtime;
                    buf_guest->msg_ctime = buf.msg_ctime;
                    buf_guest->__msg_cbytes = buf.__msg_cbytes;
                    buf_guest->msg_qnum = buf.msg_qnum;
                    buf_guest->msg_qbytes = buf.msg_qbytes;
                    buf_guest->msg_lspid = buf.msg_lspid;
                    buf_guest->msg_lrpid = buf.msg_lrpid;
                }
            }
            break;
        case NEUTRAL_MSG_STAT:
        case NEUTRAL_IPC_STAT:
            {
                struct neutral_msqid64_ds_32 *buf_guest = (struct neutral_msqid64_ds_32 *) g_2_h(buf_p);
                struct neutral_msqid64_ds_64 buf;

                if (buf_p == 0 || buf_p == 0xffffffff)
                    res = -EFAULT;
                else {
                    res = syscall_neutral_64(PR_msgctl, msqid, cmd, (uint64_t) &buf, 0, 0, 0);
                    buf_guest->msg_perm.key = buf.msg_perm.key;
                    buf_guest->msg_perm.uid = buf.msg_perm.uid;
                    buf_guest->msg_perm.gid = buf.msg_perm.gid;
                    buf_guest->msg_perm.cuid = buf.msg_perm.cuid;
                    buf_guest->msg_perm.cgid = buf.msg_perm.cgid;
                    buf_guest->msg_perm.mode = buf.msg_perm.mode;
                    buf_guest->msg_perm.seq = buf.msg_perm.seq;
                    buf_guest->msg_stime = buf.msg_stime;
                    buf_guest->msg_rtime = buf.msg_rtime;
                    buf_guest->msg_ctime = buf.msg_ctime;
                    buf_guest->__msg_cbytes = buf.__msg_cbytes;
                    buf_guest->msg_qnum = buf.msg_qnum;
                    buf_guest->msg_qbytes = buf.msg_qbytes;
                    buf_guest->msg_lspid = buf.msg_lspid;
                    buf_guest->msg_lrpid = buf.msg_lrpid;
                }
            }
            break;
        case NEUTRAL_IPC_INFO:
        case NEUTRAL_MSG_INFO:
            res = syscall_neutral_64(PR_msgctl, msqid, cmd, (uint64_t) g_2_h(buf_p), 0, 0, 0);
            break;
        default:
            fatal("msgctl_neutral: unsupported command %d\n", cmd);
    }

    return res;
}

static int mq_timedreceive_neutral(uint32_t mqdes_p, uint32_t msg_ptr_p, uint32_t msg_len_p, uint32_t msg_prio_p, uint32_t abs_timeout_p)
{
    long res;
    neutral_mqd_t mqdes = (neutral_mqd_t) mqdes_p;
    char *msg_ptr = (char *) g_2_h(msg_ptr_p);
    size_t msg_len = (size_t) msg_len_p;
    unsigned int *msg_prio = (unsigned int *) g_2_h(msg_prio_p);
    struct neutral_timespec_32 *abs_timeout_guest = (struct neutral_timespec_32 *) g_2_h(abs_timeout_p);
    struct neutral_timespec_64 abs_timeout;

    if (abs_timeout_p) {
        abs_timeout.tv_sec = abs_timeout_guest->tv_sec;
        abs_timeout.tv_nsec = abs_timeout_guest->tv_nsec;
    }
    res = syscall_neutral_64(PR_mq_timedreceive, mqdes, (uint64_t) msg_ptr, msg_len, (uint64_t) (msg_prio_p?msg_prio:NULL), (uint64_t) (abs_timeout_p?&abs_timeout:NULL), 0);

    return res;
}

struct data_64_internal {
    const sigset_t *ss;
    size_t ss_len;
};

struct data_32_internal {
    uint32_t ss;
    uint32_t ss_len;
};

static int pselect6_neutral(uint32_t nfds_p, uint32_t readfds_p, uint32_t writefds_p, uint32_t exceptfds_p, uint32_t timeout_p, uint32_t data_p)
{
    long res;
    int nfds = (int) nfds_p;
    neutral_fd_set *readfds = (neutral_fd_set *) g_2_h(readfds_p);
    neutral_fd_set *writefds = (neutral_fd_set *) g_2_h(writefds_p);
    neutral_fd_set *exceptfds = (neutral_fd_set *) g_2_h(exceptfds_p);
    struct neutral_timespec_32 *timeout_guest = (struct neutral_timespec_32 *) g_2_h(timeout_p);
    struct data_32_internal *data_guest = (struct data_32_internal *) g_2_h(data_p);
    struct neutral_timespec_64 timeout;
    struct data_64_internal data;


    if (timeout_p) {
        timeout.tv_sec = timeout_guest->tv_sec;
        timeout.tv_nsec = timeout_guest->tv_nsec;
    };

    /* FIXME: what if data_p NULL ? */
    data.ss = (const sigset_t *) (data_guest->ss?g_2_h(data_guest->ss):NULL);
    data.ss_len = data_guest->ss_len;
    res = syscall_neutral_64(PR_pselect6, nfds, (uint64_t) (readfds_p?readfds:NULL), (uint64_t) (writefds_p?writefds:NULL),
                             (uint64_t) (exceptfds_p?exceptfds:NULL), (uint64_t) (timeout_p?&timeout:NULL), (uint64_t) (&data));

    if (timeout_p) {
        timeout_guest->tv_sec = timeout.tv_sec;
        timeout_guest->tv_nsec = timeout.tv_nsec;
    };

    return res;
}

static int ppoll_neutral(uint32_t fds_p, uint32_t nfds_p, uint32_t timeout_ts_p, uint32_t sigmask_p, uint32_t sigsetsize_p)
{
    int res;
    struct neutral_pollfd *fds = (struct neutral_pollfd *) g_2_h(fds_p);
    unsigned int nfds = (unsigned int) nfds_p;
    struct neutral_timespec_32 *timeout_ts_guest = (struct neutral_timespec_32 *) g_2_h(timeout_ts_p);
    sigset_t *sigmask = (sigset_t *) g_2_h(sigmask_p);
    size_t sigsetsize = (size_t) sigsetsize_p;
    struct neutral_timespec_64 timeout_ts;

    if (timeout_ts_p) {
        timeout_ts.tv_sec = timeout_ts_guest->tv_sec;
        timeout_ts.tv_nsec = timeout_ts_guest->tv_nsec;
    }
    res = syscall_neutral_64(PR_ppoll, (uint64_t) fds, nfds, (uint64_t) (timeout_ts_p?&timeout_ts:NULL),
                             (uint64_t) (sigmask_p?sigmask:NULL), sigsetsize, 0);


    return res;
}

static int timerfd_settime_neutral(uint32_t fd_p, uint32_t flags_p, uint32_t new_value_p, uint32_t old_value_p)
{
    int res;
    int fd = (int) fd_p;
    int flags = (int) flags_p;
    struct neutral_itimerspec_32 *new_value_guest = (struct neutral_itimerspec_32 *) g_2_h(new_value_p);
    struct neutral_itimerspec_32 *old_value_guest = (struct neutral_itimerspec_32 *) g_2_h(old_value_p);
    struct neutral_itimerspec_64 new_value;
    struct neutral_itimerspec_64 old_value;

    if (old_value_p == 0xffffffff || new_value_p == 0)
        res = -EFAULT;
    else {
        new_value.it_interval.tv_sec = new_value_guest->it_interval.tv_sec;
        new_value.it_interval.tv_nsec = new_value_guest->it_interval.tv_nsec;
        new_value.it_value.tv_sec = new_value_guest->it_value.tv_sec;
        new_value.it_value.tv_nsec = new_value_guest->it_value.tv_nsec;

        res = syscall_neutral_64(PR_timerfd_settime, fd, flags, (uint64_t) &new_value, (uint64_t) (old_value_p?&old_value:NULL), 0, 0);
        if (old_value_p) {
            old_value_guest->it_interval.tv_sec = old_value.it_interval.tv_sec;
            old_value_guest->it_interval.tv_nsec = old_value.it_interval.tv_nsec;
            old_value_guest->it_value.tv_sec = old_value.it_value.tv_sec;
            old_value_guest->it_value.tv_nsec = old_value.it_value.tv_nsec;
        }
    }

    return res;
}

static int timerfd_gettime_neutral(uint32_t fd_p, uint32_t curr_value_p)
{
    int res;
    int fd = (int) fd_p;
    struct neutral_itimerspec_32 *curr_value_guest = (struct neutral_itimerspec_32 *) g_2_h(curr_value_p);
    struct neutral_itimerspec_64 curr_value;

    if (curr_value_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall_neutral_64(PR_timerfd_gettime, fd, (uint64_t) &curr_value, 0, 0, 0, 0);
        if (curr_value_p) {
            curr_value_guest->it_interval.tv_sec = curr_value.it_interval.tv_sec;
            curr_value_guest->it_interval.tv_nsec = curr_value.it_interval.tv_nsec;
            curr_value_guest->it_value.tv_sec = curr_value.it_value.tv_sec;
            curr_value_guest->it_value.tv_nsec = curr_value.it_value.tv_nsec;
        }
    }

    return res;
}

static int getgroups_neutral(uint32_t size_p, uint32_t list_p)
{
    long res;
    int size = (int) size_p;
    uint16_t *list_guest = (uint16_t *) g_2_h(list_p);
    uint32_t *list = alloca(sizeof(uint32_t) * size);
    int i;

    res = syscall_neutral_64(PR_getgroups, size, (uint64_t) list, 0, 0, 0, 0);
    for(i = 0; i < size; i++)
        list_guest[i] = list[i];

    return res;
}

static int statfs_neutral(uint32_t path_p, uint32_t buf_p)
{
    const char *path = (const char *) g_2_h(path_p);
    struct neutral_statfs_32 *buf_guest = (struct neutral_statfs_32 *) g_2_h(buf_p);
    struct neutral_statfs_64 buf;
    long res;

     if (buf_p == 0 || buf_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall_neutral_64(PR_statfs, (uint64_t) path, (uint64_t) &buf, 0, 0, 0, 0);

        buf_guest->f_type = buf.f_type;
        buf_guest->f_bsize = buf.f_bsize;
        buf_guest->f_blocks = buf.f_blocks;
        buf_guest->f_bfree = buf.f_bfree;
        buf_guest->f_bavail = buf.f_bavail;
        buf_guest->f_files = buf.f_files;
        buf_guest->f_ffree = buf.f_ffree;
        buf_guest->f_fsid.val[0] = 0;
        buf_guest->f_fsid.val[1] = 0;
        buf_guest->f_namelen = buf.f_namelen;
        buf_guest->f_frsize = buf.f_frsize;
        buf_guest->f_flags = 0;
    }

    return res;
}

static int sendfile_neutral(uint32_t out_fd_p, uint32_t in_fd_p, uint32_t offset_p, uint32_t count_p)
{
    long res;
    int out_fd = (int) out_fd_p;
    int in_fd = (int) in_fd_p;
    int32_t *offset_guest = (int32_t *) g_2_h(offset_p);
    size_t count = (size_t) count_p;
    off_t offset;

    if (offset_p)
        offset = *offset_guest;
    res = syscall_neutral_64(PR_sendfile, out_fd, in_fd, (uint64_t) (offset_p?&offset:NULL), count, 0, 0);
    if (offset_p)
        *offset_guest = offset;

    return res;
}

static int utimes_neutral(uint32_t filename_p, uint32_t times_p)
{
    long res;
    char *filename = (char *) g_2_h(filename_p);
    struct neutral_timeval_32 *times_guest = (struct neutral_timeval_32 *) g_2_h(times_p);
    struct neutral_timeval_64 times[2];

    if (times_p) {
        times[0].tv_sec = times_guest[0].tv_sec;
        times[0].tv_usec = times_guest[0].tv_usec;
        times[1].tv_sec = times_guest[1].tv_sec;
        times[1].tv_usec = times_guest[1].tv_usec;
    }

    res = syscall_neutral_64(PR_utimes, (uint64_t) filename, (uint64_t) (times_p?times:NULL), 0, 0, 0, 0);

    return res;
}

static int vmsplice_neutral(uint32_t fd_p, uint32_t iov_p, uint32_t nr_segs_p, uint32_t flags_p)
{
    long res;
    int fd = (int) fd_p;
    struct neutral_iovec_32 *iov_guest = (struct neutral_iovec_32 *) g_2_h(iov_p);
    unsigned long nr_segs = (unsigned long) nr_segs_p;
    unsigned int flags = (unsigned int) flags_p;
    struct neutral_iovec_64 *iov;
    int i;

    iov = (struct neutral_iovec_64 *) alloca(sizeof(struct neutral_iovec_64) * nr_segs);
    for(i = 0; i < nr_segs; i++) {
        iov[i].iov_base = ptr_2_int(g_2_h(iov_guest->iov_base));
        iov[i].iov_len = iov_guest->iov_len;
    }
    res = syscall_neutral_64(PR_vmsplice, fd, (uint64_t) iov, nr_segs, flags, 0, 0);

    return res;
}

static int rt_sigtimedwait_neutral(uint32_t set_p, uint32_t info_p, uint32_t timeout_p)
{
    long res;
    sigset_t *set = (sigset_t *) g_2_h(set_p);
    neutral_siginfo_t_32 *info_guest = (neutral_siginfo_t_32 *) g_2_h(info_p);
    struct neutral_timespec_32 *timeout_guest = (struct neutral_timespec_32 *) g_2_h(timeout_p);
    neutral_siginfo_t_64 info;
    struct neutral_timespec_64 timeout;

    if (timeout_p) {
        timeout.tv_sec = timeout_guest->tv_sec;
        timeout.tv_nsec = timeout_guest->tv_nsec;
    }
    res = syscall_neutral_64(PR_rt_sigtimedwait, (uint64_t) set, (uint64_t) (info_p?&info:NULL), (uint64_t) (timeout_p?&timeout:NULL), NEUTRAL__NSIG / 8, 0, 0);
    if (info_p) {
        info_guest->si_signo = info.si_signo;
        info_guest->si_code = info.si_code;
        info_guest->_sifields._sigchld._si_pid = info._sifields._sigchld._si_pid;
        info_guest->_sifields._sigchld._si_uid = info._sifields._sigchld._si_uid;
        info_guest->_sifields._sigchld._si_status = info._sifields._sigchld._si_status;
    }

    return res;
}

static int msgrcv_neutral(uint32_t msqid_p, uint32_t msgp_p, uint32_t msgsz_p, uint32_t msgtyp_p, uint32_t msgflg_p)
{
    long res;
    int msqid = (int) msqid_p;
    struct neutral_msgbuf_32 *msgp_guest = (struct neutral_msgbuf_32 *) g_2_h(msgp_p);
    size_t msgsz = (size_t) msgsz_p;
    long msgtyp = (int) msgtyp_p;
    int msgflg = (int) msgflg_p;
    struct neutral_msgbuf_64 *msgp;

    if ((int)msgsz < 0)
        return -EINVAL;

    msgp = (struct neutral_msgbuf_64 *) alloca(sizeof(uint64_t) + msgsz);
    assert(msgp != NULL);
    msgp->mtype = msgp_guest->mtype;
    res = syscall_neutral_64(PR_msgrcv, msqid, (uint64_t) msgp, msgsz, msgtyp, msgflg, 0);
    if (res >= 0) {
        msgp_guest->mtype = msgp->mtype;
        memcpy(msgp_guest->mtext, msgp->mtext, res);
    }

    return res;
}

static int sendmsg_neutral(uint32_t sockfd_p, uint32_t msg_p, uint32_t flags_p)
{
    long res;
    int sockfd = (int) sockfd_p;
    struct neutral_msghdr_32 *msg_guest = (struct neutral_msghdr_32 *) g_2_h(msg_p);
    int flags = (int) flags_p;
    struct neutral_iovec_64 iovec[16]; //max 16 iovect. If more is need then use mmap or alloca
    struct neutral_msghdr_64 msg;
    int i;

    assert(msg_guest->msg_iovlen <= 16);

    msg.msg_name = ptr_2_int(msg_guest->msg_name?(void *) g_2_h(msg_guest->msg_name):NULL);
    msg.msg_namelen = msg_guest->msg_namelen;
    for(i = 0; i < msg_guest->msg_iovlen; i++) {
        struct neutral_iovec_32 *iovec_guest = (struct neutral_iovec_32 *) g_2_h(msg_guest->msg_iov + sizeof(struct neutral_iovec_32) * i);

        iovec[i].iov_base = ptr_2_int(g_2_h(iovec_guest->iov_base));
        iovec[i].iov_len = iovec_guest->iov_len;
    }
    msg.msg_iov = ptr_2_int(iovec);
    msg.msg_iovlen = msg_guest->msg_iovlen;
    msg.msg_control = ptr_2_int(g_2_h(msg_guest->msg_control));
    msg.msg_controllen = msg_guest->msg_controllen;
    msg.msg_flags = msg_guest->msg_flags;

    res = syscall_neutral_64(PR_sendmsg, sockfd, (uint64_t) &msg, flags, 0, 0, 0);

    return res;
}

static int clock_settime_neutral(uint32_t clk_id_p, uint32_t tp_p)
{
    long res;
    clockid_t clk_id = (clockid_t) clk_id_p;
    struct neutral_timespec_32 *tp_guest = (struct neutral_timespec_32 *) g_2_h(tp_p);
    struct neutral_timespec_64 tp;

    tp.tv_sec = tp_guest->tv_sec;
    tp.tv_nsec = tp_guest->tv_nsec;

    res = syscall_neutral_64(PR_clock_settime, clk_id, (uint64_t) &tp, 0, 0, 0, 0);

    return res;
}

static int mq_getsetattr_neutral(uint32_t mqdes_p, uint32_t newattr_p, uint32_t oldattr_p)
{
    long res;
    neutral_mqd_t mqdes = (neutral_mqd_t) mqdes_p;
    struct neutral_mq_attr_32 *newattr_guest = (struct neutral_mq_attr_32 *) g_2_h(newattr_p);
    struct neutral_mq_attr_32 *oldattr_guest = (struct neutral_mq_attr_32 *) g_2_h(oldattr_p);
    struct neutral_mq_attr_64 newattr;
    struct neutral_mq_attr_64 oldattr;

    if (newattr_p) {
        newattr.mq_flags = newattr_guest->mq_flags;
        newattr.mq_maxmsg = newattr_guest->mq_maxmsg;
        newattr.mq_msgsize = newattr_guest->mq_msgsize;
        newattr.mq_curmsgs = newattr_guest->mq_curmsgs;
    }
    res = syscall_neutral_64(PR_mq_getsetattr, mqdes, (uint64_t) (newattr_p?&newattr:NULL), (uint64_t) (oldattr_p?&oldattr:NULL), 0, 0, 0);
    if (oldattr_p) {
        oldattr_guest->mq_flags = oldattr.mq_flags;
        oldattr_guest->mq_maxmsg = oldattr.mq_maxmsg;
        oldattr_guest->mq_msgsize = oldattr.mq_msgsize;
        oldattr_guest->mq_curmsgs = oldattr.mq_curmsgs;
    }

    return res;
}

static int recvmsg_neutral(uint32_t sockfd_p, uint32_t msg_p, uint32_t flags_p)
{
    long res;
    int sockfd = (int) sockfd_p;
    struct neutral_msghdr_32 *msg_guest = (struct neutral_msghdr_32 *) g_2_h(msg_p);
    int flags = (int) flags_p;
    struct neutral_iovec_64 iovec[16]; //max 16 iovect. If more is need then use mmap or alloca
    struct neutral_msghdr_64 msg;
    int i;

    assert(msg_guest->msg_iovlen <= 16);

    msg.msg_name = ptr_2_int(msg_guest->msg_name?(void *) g_2_h(msg_guest->msg_name):NULL);
    msg.msg_namelen = msg_guest->msg_namelen;
    for(i = 0; i < msg_guest->msg_iovlen; i++) {
        struct neutral_iovec_32 *iovec_guest = (struct neutral_iovec_32 *) g_2_h(msg_guest->msg_iov + sizeof(struct neutral_iovec_32) * i);

        iovec[i].iov_base = ptr_2_int(g_2_h(iovec_guest->iov_base));
        iovec[i].iov_len = iovec_guest->iov_len;
    }
    msg.msg_iov = ptr_2_int(iovec);
    msg.msg_iovlen = msg_guest->msg_iovlen;
    msg.msg_control = ptr_2_int(g_2_h(msg_guest->msg_control));
    msg.msg_controllen = msg_guest->msg_controllen;
    msg.msg_flags = msg_guest->msg_flags;

    res = syscall_neutral_64(PR_recvmsg, sockfd, (uint64_t) &msg, flags, 0, 0, 0);
    if (res >= 0) {
        msg_guest->msg_namelen = msg.msg_namelen;
        msg_guest->msg_controllen = msg.msg_controllen;
    }

    return res;
}

static int sched_rr_get_interval_neutral(uint32_t pid_p, uint32_t tp_p)
{
    long result;
    pid_t pid = (pid_t) pid_p;
    struct neutral_timespec_32 *tp_guest = (struct neutral_timespec_32 *) g_2_h(tp_p);
    struct neutral_timespec_64 tp;

    result = syscall_neutral_64(PR_sched_rr_get_interval, pid, (uint64_t) &tp, 0, 0, 0, 0);
    if (tp_p) {
        tp_guest->tv_sec = tp.tv_sec;
        tp_guest->tv_nsec = tp.tv_nsec;
    }

    return result;
}

static int timer_settime_neutral(uint32_t timerid_p, uint32_t flags_p, uint32_t new_value_p, uint32_t old_value_p)
{
    long res;
    timer_t timerid = (timer_t)(long) timerid_p;
    int flags = (int) flags_p;
    struct neutral_itimerspec_32 *new_value_guest = (struct neutral_itimerspec_32 *) g_2_h(new_value_p);
    struct neutral_itimerspec_32 *old_value_guest = (struct neutral_itimerspec_32 *) g_2_h(old_value_p);
    struct neutral_itimerspec_64 new_value;
    struct neutral_itimerspec_64 old_value;

    new_value.it_interval.tv_sec = new_value_guest->it_interval.tv_sec;
    new_value.it_interval.tv_nsec = new_value_guest->it_interval.tv_nsec;
    new_value.it_value.tv_sec = new_value_guest->it_value.tv_sec;
    new_value.it_value.tv_nsec = new_value_guest->it_value.tv_nsec;

    res = syscall_neutral_64(PR_timer_settime, (uint64_t) timerid, flags, (uint64_t) &new_value, (uint64_t) (old_value_p?&old_value:NULL), 0, 0);

    if (old_value_p) {
        old_value_guest->it_interval.tv_sec = old_value.it_interval.tv_sec;
        old_value_guest->it_interval.tv_nsec = old_value.it_interval.tv_nsec;
        old_value_guest->it_value.tv_sec = old_value.it_value.tv_sec;
        old_value_guest->it_value.tv_nsec = old_value.it_value.tv_nsec;
    }

    return res;
}

static int sendmmsg_neutral(uint32_t sockfd_p, uint32_t msgvec_p, uint32_t vlen_p, uint32_t flags_p)
{
    long res;
    int sockfd = (int) sockfd_p;
    struct neutral_mmsghdr_32 *msgvec_guest = (struct neutral_mmsghdr_32 *) g_2_h(msgvec_p);
    unsigned int vlen = (unsigned int) vlen_p;
    unsigned int flags = (unsigned int) flags_p;
    struct neutral_mmsghdr_64 *msgvec = (struct neutral_mmsghdr_64 *) alloca(vlen * sizeof(struct neutral_mmsghdr_64));
    int i, j;

    for(i = 0; i < vlen; i++) {
        msgvec[i].msg_hdr.msg_name = ptr_2_int(msgvec_guest[i].msg_hdr.msg_name?(void *) g_2_h(msgvec_guest[i].msg_hdr.msg_name):NULL);
        msgvec[i].msg_hdr.msg_namelen = msgvec_guest[i].msg_hdr.msg_namelen;
        msgvec[i].msg_hdr.msg_iov = ptr_2_int(alloca(msgvec_guest[i].msg_hdr.msg_iovlen * sizeof(struct neutral_iovec_64)));
        for(j = 0; j < msgvec_guest[i].msg_hdr.msg_iovlen; j++) {
            struct neutral_iovec_32 *iovec_guest = (struct neutral_iovec_32 *) g_2_h(msgvec_guest[i].msg_hdr.msg_iov + sizeof(struct neutral_iovec_32) * j);
            struct neutral_iovec_64 *iovec = (struct neutral_iovec_64 *) (msgvec[i].msg_hdr.msg_iov + sizeof(struct neutral_iovec_64) * j);

            iovec->iov_base = ptr_2_int(g_2_h(iovec_guest->iov_base));
            iovec->iov_len = iovec_guest->iov_len;
        }
        msgvec[i].msg_hdr.msg_iovlen = msgvec_guest[i].msg_hdr.msg_iovlen;
        msgvec[i].msg_hdr.msg_control = ptr_2_int(g_2_h(msgvec_guest[i].msg_hdr.msg_control));
        msgvec[i].msg_hdr.msg_controllen = msgvec_guest[i].msg_hdr.msg_controllen;
        msgvec[i].msg_hdr.msg_flags = msgvec_guest[i].msg_hdr.msg_flags;
        msgvec[i].msg_len = msgvec_guest[i].msg_len;
    }
    res = syscall_neutral_64(PR_sendmmsg, sockfd, (uint64_t) msgvec, vlen, flags, 0, 0);
    for(i = 0; i < vlen; i++)
        msgvec_guest[i].msg_len = msgvec[i].msg_len;

    return res;
}

static int rt_sigqueueinfo_neutral(uint32_t pid_p, uint32_t sig_p, uint32_t uinfo_p)
{
    long res;
    pid_t pid = (pid_t) pid_p;
    int sig = (int) sig_p;
    neutral_siginfo_t_32 *uinfo_guest = (neutral_siginfo_t_32 *) g_2_h(uinfo_p);
    neutral_siginfo_t_64 uinfo;

    uinfo.si_code = uinfo_guest->si_code;
    uinfo._sifields._rt._si_pid = uinfo_guest->_sifields._rt._si_pid;
    uinfo._sifields._rt._si_uid = uinfo_guest->_sifields._rt._si_uid;
    uinfo._sifields._rt._si_sigval.sival_int = uinfo_guest->_sifields._rt._si_sigval.sival_int;

    res = syscall_neutral_64(PR_rt_sigqueueinfo, pid, sig, (uint64_t) &uinfo, 0, 0, 0);

    return res;
}

int syscall_adapter_guest32(Sysnum no, uint32_t p0, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4, uint32_t p5)
{
    int res = -ENOSYS;

    switch(no) {
        case PR__newselect:
            res = newselect_neutral(p0, p1, p2, p3, p4);
            break;
        case PR__llseek:
            res = llseek_neutral(p0, p1, p2, p3, p4);
            break;
        case PR_accept:
            res = syscall_neutral_64(PR_accept, p0, IS_NULL(p1), IS_NULL(p2), p3, p4, p5);
            break;
        case PR_accept4:
            res = syscall_neutral_64(PR_accept4, (int) p0, IS_NULL(p1), IS_NULL(p2), (int) p3, p4, p5);
            break;
        case PR_access:
            res = syscall_neutral_64(PR_access, (uint64_t)g_2_h(p0), (int)p1, p2, p3, p4, p5);
            break;
        case PR_add_key:
            res = syscall_neutral_64(PR_add_key, (uint64_t) g_2_h(p0), IS_NULL(p1), IS_NULL(p2), (size_t) p3, p4, p5);
            break;
        case PR_bind:
            res= syscall_neutral_64(PR_bind, (int) p0, (uint64_t) g_2_h(p1), (socklen_t) p2, p3, p4, p5);
            break;
        case PR_bdflush:
            /* This one is deprecated since 2.6 and p1 is no more use */
            res = 0;
            break;
        case PR_capget:
            res = syscall_neutral_64(PR_capget, (uint64_t) g_2_h(p0), IS_NULL(p1), p2, p3, p4, p5);
            break;
        case PR_capset:
            res = syscall_neutral_64(PR_capset, (uint64_t) g_2_h(p0), IS_NULL(p1), p2, p3, p4, p5);
            break;
        case PR_chdir:
            res = syscall_neutral_64(PR_chdir, (uint64_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_chmod:
            res = syscall_neutral_64(PR_chmod, (uint64_t) g_2_h(p0), (mode_t) p1, p2, p3, p4, p5);
            break;
        case PR_chown32:
            res = syscall_neutral_64(PR_chown, (uint64_t) g_2_h(p0), (uid_t)p1, (gid_t)p2, p3, p4, p5);
            break;
        case PR_chroot:
            res = syscall_neutral_64(PR_chroot, (uint64_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_clock_getres:
            res = clock_getres_neutral(p0, p1);
            break;
        case PR_clock_gettime:
            res = clock_gettime_neutral(p0, p1);
            break;
        case PR_clock_nanosleep:
            res = clock_nanosleep_neutral(p0,p1,p2,p3);
            break;
        case PR_clock_settime:
            res = clock_settime_neutral(p0, p1);
            break;
        case PR_close:
            res = syscall_neutral_64(PR_close, (int)p0, p1, p2, p3, p4, p5);
            break;
        case PR_connect:
            res = syscall_neutral_64(PR_connect, (int)p0, (uint64_t)g_2_h(p1), (socklen_t)p2, p3, p4, p5);
            break;
        case PR_creat:
            res = syscall_neutral_64(PR_creat, (uint64_t) g_2_h(p0), (mode_t) p1, p2, p3, p4, p5);
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
        case PR_epoll_ctl:
            res = syscall_neutral_64(PR_epoll_ctl, p0, p1, p2, (uint64_t) IS_NULL(p3), p4, p5);
            break;
        case PR_epoll_create:
            res = syscall_neutral_64(PR_epoll_create, (int) p0, p1, p2, p3, p4, p5);
            break;
        case PR_epoll_create1:
            res = syscall_neutral_64(PR_epoll_create1, (int) p0, p1, p2, p3, p4, p5);
            break;
        case PR_epoll_wait:
            res = syscall_neutral_64(PR_epoll_wait, (int) p0, (uint64_t)g_2_h(p1), (int) p2, (int) p3, p4, p5);
            break;
        case PR_eventfd:
            res = syscall_neutral_64(PR_eventfd, (int)p0, (int)(p1), p2, p3, p4, p5);
            break;
        case PR_eventfd2:
            res = syscall_neutral_64(PR_eventfd2, (unsigned int) p0, (int) p1, p2, p3, p4, p5);
            break;
        case PR_execve:
            res = execve_neutral(p0, p1, p2);
            break;
        case PR_exit_group:
            res = syscall_neutral_64(PR_exit_group, (int)p0, p1, p2, p3, p4, p5);
            break;
        case PR_faccessat:
            res = syscall_neutral_64(PR_faccessat, (int)p0, (uint64_t)g_2_h(p1), (int)p2, p3, p4, p5);
            break;
        case PR_fadvise64_64:
            res = syscall_neutral_64(PR_fadvise64, (int)p0, ((uint64_t)p2 << 32) | (uint64_t)p1,
                                                   ((uint64_t)p4 << 32) | (uint64_t)p3, (int)p5, 0, 0);
            break;
        case PR_fallocate:
            res = syscall_neutral_64(PR_fallocate, (int) p0, (int) p1, (uint64_t)p2 + ((uint64_t)p3 << 32),
                                                   (uint64_t)p4 + ((uint64_t)p5 << 32), 0, 0);
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
        case PR_fchown32:
            res = syscall_neutral_64(PR_fchown, (unsigned int)p0, (uid_t)p1, (gid_t)p2, p3, p4, p5);
            break;
        case PR_fchownat:
            res = syscall_neutral_64(PR_fchownat, (int) p0, (uint64_t) g_2_h(p1), (uid_t) p2, (gid_t) p3, (int) p4, p5);
            break;
        case PR_fcntl64:
            res = fcnt64_neutral(p0, p1, p2);
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
        case PR_ftruncate:
            res = syscall_neutral_64(PR_ftruncate, (int) p0, (off_t)(int) p1, p2, p3, p4, p5);
            break;
        case PR_ftruncate64:
            res = syscall_neutral_64(PR_ftruncate, p0, ((uint64_t)p2 << 32) | (uint64_t)p1, p2, p3, p4, p5);
            break;
        case PR_futex:
            res = futex_neutral(p0, p1, p2, p3, p4, p5);
            break;
        case PR_signalfd4:
            res = syscall_neutral_64(PR_signalfd4, (int) p0, (uint64_t) g_2_h(p1), (size_t) p2, (int) p3, p4, p5);
            break;
        case PR_fstat64:
            res = syscall_neutral_64(PR_fstat64, p0, (uint64_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_fstatat64:
            res = syscall_neutral_64(PR_fstatat64, (int)p0, (uint64_t)g_2_h(p1), (uint64_t)g_2_h(p2), (int)p3, p4, p5);
            break;
        case PR_fstatfs:
            res = fstatfs_neutral(p0, p1);
            break;
        case PR_fstatfs64:
            res = fstatfs64_neutral(p0, p1, p2);
            break;
        case PR_fsync:
            res = syscall_neutral_64(PR_fsync, (int)p0, p1, p2, p3, p4, p5);
            break;
        case PR_futimesat:
            res = futimesat_neutral(p0, p1, p2);
            break;
        case PR_getcwd:
            res = syscall_neutral_64(PR_getcwd, (uint64_t)g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_getdents:
            res = getdents_neutral(p0, p1, p2);
            break;
        case PR_getdents64:
            res = syscall_neutral_64(PR_getdents64, (int) p0, (uint64_t)g_2_h(p1), (unsigned int) p2, p3, p4, p5);
            break;
        case PR_getegid:
            res = syscall_neutral_64(PR_getegid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getegid32:
            res = syscall_neutral_64(PR_getegid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_geteuid:
            res = syscall_neutral_64(PR_geteuid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_geteuid32:
            res = syscall_neutral_64(PR_geteuid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getgid:
            res = syscall_neutral_64(PR_getgid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getgid32:
            res = syscall_neutral_64(PR_getgid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getitimer:
            res = getitimer_neutral(p0, p1);
            break;
        case PR_getgroups:
            res = getgroups_neutral(p0, p1);
            break;
        case PR_getgroups32:
            res = syscall_neutral_64(PR_getgroups, (int) p0, (uint64_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_getpgrp:
            res = syscall_neutral_64(PR_getpgrp, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getpid:
            res = syscall_neutral_64(PR_getpid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getppid:
            res = syscall_neutral_64(PR_getppid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getresgid32:
            res = syscall_neutral_64(PR_getresgid, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_getresuid32:
            res = syscall_neutral_64(PR_getresuid, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_getrlimit:
            res = getrlimit_neutral(p0, p1);
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
            res = gettimeofday_neutral(p0, p1);
            break;
        case PR_getuid:
            res = syscall_neutral_64(PR_getuid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getuid32:
            res = syscall_neutral_64(PR_getuid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getrusage:
            res = getrusage_neutral(p0, p1);
            break;
        case PR_getpeername:
            res = syscall_neutral_64(PR_getpeername, (int)p0, (uint64_t)g_2_h(p1), (uint64_t)g_2_h(p2), p3, p4, p5);
            break;
        case PR_getpgid:
            res = syscall_neutral_64(PR_getpgid, (pid_t)p0, p1, p2, p3, p4, p5);
            break;
        case PR_getpriority:
            res = syscall_neutral_64(PR_getpriority, (int) p0, (id_t) p1, p2, p3, p4, p5);
            break;
        case PR_getxattr:
            res = syscall_neutral_64(PR_getxattr, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), (size_t) p3, p4, p5);
            break;
        case PR_inotify_init:
            res = syscall_neutral_64(PR_inotify_init, p0, p1, p2, p3, p4, p5);
            break;
        case PR_inotify_init1:
            res = syscall_neutral_64(PR_inotify_init1, (int) p0, p1, p2, p3, p4, p5);
            break;
        case PR_inotify_add_watch:
            res = syscall_neutral_64(PR_inotify_add_watch, (int) p0, (uint64_t) g_2_h(p1), (uint32_t) p2, p3, p4, p5);
            break;
        case PR_inotify_rm_watch:
            res = syscall_neutral_64(PR_inotify_rm_watch, (int) p0, (int) p1, p2, p3, p4, p5);
            break;
        case PR_ioctl:
            /* FIXME: need specific version to offset param according to ioctl */
            res = syscall_neutral_64(PR_ioctl, (int) p0, (unsigned long) p1, (uint64_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_keyctl:
            res = keyctl_neutral(p0, p1, p2, p3, p4);
            break;
        case PR_kill:
            res = syscall_neutral_64(PR_kill, (pid_t)p0, (int)p1, p2, p3, p4, p5);
            break;
        case PR_lchown32:
            res = syscall_neutral_64(PR_lchown32, (uint64_t) g_2_h(p0), (uid_t) p1, (gid_t) p2, p3, p4, p5);
            break;
        case PR_link:
            res = syscall_neutral_64(PR_link, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_linkat:
            res = syscall_neutral_64(PR_linkat, (int) p0, (uint64_t) g_2_h(p1), (int) p2, (uint64_t) g_2_h(p3), (int) p4, p5);
            break;
        case PR_listen:
            res = syscall_neutral_64(PR_listen, p0, p1, p2, p3, p4, p5);
            break;
        case PR_lgetxattr:
            res = syscall_neutral_64(PR_lgetxattr, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), (size_t) p3, p4, p5);
            break;
        case PR_lseek:
            res = syscall_neutral_64(PR_lseek, (int)p0, (off_t)(int)p1, (int)p2, p3, p4, p5);
            break;
        case PR_lstat64:
            res = syscall_neutral_64(PR_lstat64, (uint64_t)g_2_h(p0), (uint64_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_madvise:
            res = syscall_neutral_64(PR_madvise, (uint64_t) g_2_h(p0), (size_t) p1, (int) p2, p3, p4, p5);
            break;
        case PR_mincore:
            res = syscall_neutral_64(PR_mincore, (uint64_t) g_2_h(p0), (size_t) p1, (uint64_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_mkdir:
            res = syscall_neutral_64(PR_mkdir, (uint64_t) g_2_h(p0), (mode_t) p1, p2, p3, p4, p5);
            break;
        case PR_mkdirat:
            res = syscall_neutral_64(PR_mkdirat, (int) p0, (uint64_t) g_2_h(p1), (mode_t) p2, p3, p4, p5);
            break;
        case PR_mknod:
            res = syscall_neutral_64(PR_mknod, (uint64_t) g_2_h(p0), (mode_t) p1, (dev_t) p2, p3, p4, p5);
            break;
        case PR_mknodat:
            res = syscall_neutral_64(PR_mknodat, (int) p0, (uint64_t) g_2_h(p1), (mode_t) p2, (dev_t) p3, p4, p5);
            break;
        case PR_mlock:
            res = syscall_neutral_64(PR_mlock, (uint64_t) g_2_h(p0), (size_t) p1, p2, p3, p4, p5);
            break;
        case PR_mprotect:
            res = syscall_neutral_64(PR_mprotect, (uint64_t) g_2_h(p0), (size_t) p1, (int) p2, p3, p4, p5);
            break;
        case PR_mq_getsetattr:
            res = mq_getsetattr_neutral(p0, p1, p2);
            break;
        case PR_mq_notify:
            res = mq_notify_neutral(p0, p1);
            break;
        case PR_mq_open:
            res = mq_open_neutral(p0, p1, p2, p3);
            break;
        case PR_mq_timedreceive:
            res = mq_timedreceive_neutral(p0, p1, p2, p3, p4);
            break;
        case PR_mq_timedsend:
            res = mq_timedsend_neutral(p0, p1, p2, p3, p4);
            break;
        case PR_mq_unlink:
            res = syscall_neutral_64(PR_mq_unlink, (uint64_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_msgctl:
            res = msgctl_neutral(p0, p1, p2);
            break;
        case PR_msgget:
            res = syscall_neutral_64(PR_msgget, (key_t) p0, (int) p1, p2, p3, p4, p5);
            break;
        case PR_msgrcv:
            res = msgrcv_neutral(p0, p1, p2, p3, p4);
            break;
        case PR_msgsnd:
            res = msgsnd_neutral(p0, p1, p2, p3);
            break;
        case PR_msync:
            res = syscall_neutral_64(PR_msync, (uint64_t) g_2_h(p0), (size_t) p1, (int) p2, p3, p4, p5);
            break;
        case PR_munlock:
            res = syscall_neutral_64(PR_munlock, (uint64_t) g_2_h(p0), (size_t) p1, p2, p3, p4, p5);
            break;
        case PR_nanosleep:
            res = nanosleep_neutral(p0, p1);
            break;
        case PR_open:
            res = syscall_neutral_64(PR_open, (uint64_t) g_2_h(p0), (int) p1, (mode_t) p2, p3, p4, p5);
            break;
        case PR_openat:
            res = syscall_neutral_64(PR_openat, (int) p0, (uint64_t) g_2_h(p1), (int) p2, (mode_t) p3, p4, p5);
            break;
        case PR_pause:
            res = syscall_neutral_64(PR_pause, p0, p1, p2, p3, p4, p5);
            break;
        case PR_perf_event_open:
            res = syscall_neutral_64(PR_perf_event_open, (uint64_t) g_2_h(p0), (pid_t) p1, (int) p2, (int) p3, (unsigned long) p4, p5);
            break;
        case PR_personality:
            res = syscall_neutral_64(PR_personality, (unsigned int)p0, p1, p2, p3, p4, p5);
            break;
        case PR_pipe:
            res = syscall_neutral_64(PR_pipe, (uint64_t)g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_pipe2:
            res = syscall_neutral_64(PR_pipe2, (uint64_t) g_2_h(p0), (int) p1, p2, p3, p4, p5);
            break;
        case PR_poll:
            res = syscall_neutral_64(PR_poll, (uint64_t)g_2_h(p0), (unsigned int)p1, (int)p2, p3, p4, p5);
            break;
        case PR_ppoll:
            res = ppoll_neutral(p0, p1, p2, p3, p4);
            break;
        case PR_prctl:
            res = prctl_neutral(p0, p1, p2, p3, p4);
            break;
        case PR_pread64:
            res = syscall_neutral_64(PR_pread64, (unsigned int)p0, (uint64_t)g_2_h(p1), (size_t)p2, 
                                                 ((uint64_t)p4 << 32) + (uint64_t)p3, 0, 0);
            break;
        case PR_prlimit64:
            res = syscall_neutral_64(PR_prlimit64, p0, p1, IS_NULL(p2), IS_NULL(p3), p4, p5);
            break;
        case PR_pselect6:
            res = pselect6_neutral(p0,p1,p2,p3,p4,p5);
            break;
        case PR_pwrite64:
            res = syscall_neutral_64(PR_pwrite64, (unsigned int)p0, (uint64_t)g_2_h(p1), (size_t)p2, 
                                                 ((uint64_t)p4 << 32) + (uint64_t)p3, 0, 0);
            break;
        case PR_quotactl:
            res = syscall_neutral_64(PR_quotactl, (int) p0, IS_NULL(p1), (int) p2, (uint64_t) g_2_h(p3), p4, p5);
            break;
        case PR_read:
            res = syscall_neutral_64(PR_read, (int)p0, (uint64_t) g_2_h(p1), (size_t) p2, p3, p4, p5);
            break;
        case PR_readahead:
            res = syscall_neutral_64(PR_readahead, p0, ((uint64_t)p2 << 32) | (uint64_t)p1, p3, p4, p5, 0);
            break;
        case PR_readlink:
            res = syscall_neutral_64(PR_readlink, (uint64_t)g_2_h(p0), (uint64_t)g_2_h(p1), (size_t)p2, p3, p4, p5);
            break;
        case PR_readlinkat:
            res = syscall_neutral_64(PR_readlinkat, (int) p0, (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), (size_t) p3, p4, p5);
            break;
        case PR_readv:
            res = readv_neutral(p0, p1, p2);
            break;
        case PR_recv:
            res = syscall_neutral_64(PR_recv, (int) p0, (uint64_t) g_2_h(p1), (size_t) p2, (int) p3, p4, p5);
            break;
        case PR_recvfrom:
            res = syscall_neutral_64(PR_recvfrom, (int) p0, (uint64_t) g_2_h(p1), (size_t) p2, (int) p3, IS_NULL(p4), IS_NULL(p5));
            break;
        case PR_recvmsg:
            res = recvmsg_neutral(p0,p1,p2);
            break;
        case PR_remap_file_pages:
            res = syscall_neutral_64(PR_remap_file_pages, (uint64_t) g_2_h(p0), (size_t) p1, (int) p2, (size_t) p3, (int) p4, p5);
            break;
        case PR_rename:
            res = syscall_neutral_64(PR_rename, (uint64_t)g_2_h(p0), (uint64_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_rmdir:
            res = syscall_neutral_64(PR_rmdir, (uint64_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_rt_sigpending:
            res = syscall_neutral_64(PR_rt_sigpending, (uint64_t) g_2_h(p0), (size_t) p1, p2, p3, p4, p5);
            break;
        case PR_rt_sigprocmask:
            res = syscall_neutral_64(PR_rt_sigprocmask, (int)p0, IS_NULL(p1), IS_NULL(p2), (size_t) p3, p4, p5);
            break;
        case PR_rt_sigqueueinfo:
            res = rt_sigqueueinfo_neutral(p0, p1, p2);
            break;
        case PR_rt_sigsuspend:
            res = syscall_neutral_64(PR_rt_sigsuspend, (uint64_t) g_2_h(p0), (size_t)p1, p2, p3, p4, p5);
            break;
        case PR_rt_sigtimedwait:
            res = rt_sigtimedwait_neutral(p0,p1,p2);
            break;
        case PR_tgkill:
            res = syscall_neutral_64(PR_tgkill, (int)p0, (int)p1, (int)p2, p3, p4, p5);
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
        case PR_sched_get_priority_max:
            res = syscall_neutral_64(PR_sched_get_priority_max, (int) p0, p1, p2, p3, p4, p5);
            break;
        case PR_sched_get_priority_min:
            res = syscall_neutral_64(PR_sched_get_priority_min, (int) p0, p1, p2, p3, p4, p5);
            break;
        case PR_sched_rr_get_interval:
            res = sched_rr_get_interval_neutral(p0, p1);
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
            res = semctl_neutral(p0, p1, p2, p3);
            break;
        case PR_semget:
            res = syscall_neutral_64(PR_semget, (key_t) p0, (int) p1, (int) p2, p3, p4, p5);
            break;
        case PR_semop:
            res = syscall_neutral_64(PR_semop, (int) p0, (uint64_t) g_2_h(p1), (size_t) p2, p3, p4, p5);
            break;
        case PR_send:
            res = syscall_neutral_64(PR_send, (int) p0, (uint64_t) g_2_h(p1), (size_t) p2, (unsigned int) p3, p4, p5);
            break;
        case PR_sendto:
            res = syscall_neutral_64(PR_sendto, (int) p0, (uint64_t) g_2_h(p1), (size_t) p2, (unsigned int) p3, (uint64_t) g_2_h(p4), (socklen_t) p5);
            break;
        case PR_sendfile:
            res = sendfile_neutral(p0, p1, p2, p3);
            break;
        case PR_sendfile64:
            res = syscall_neutral_64(PR_sendfile, (int) p0, (int) p1, IS_NULL(p2), (size_t) p3, p4, p5);
            break;
        case PR_sendmmsg:
            res = sendmmsg_neutral(p0, p1, p2, p3);
            break;
        case PR_sendmsg:
            res = sendmsg_neutral(p0,p1,p2);
            break;
        case PR_set_robust_list:
            /* FIXME: implement this correctly */
            res = -EPERM;
            break;
        case PR_set_tid_address:
            res = syscall_neutral_64(PR_set_tid_address, (uint64_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_setfsgid:
            res = syscall_neutral_64(PR_setfsgid, (gid_t) (int16_t)p0, p1, p2, p3, p4, p5);
            break;
        case PR_setfsgid32:
            res = syscall_neutral_64(PR_setfsgid, (gid_t) p0, p1, p2, p3, p4, p5);
            break;
        case PR_setfsuid:
            res = syscall_neutral_64(PR_setfsuid, (uid_t) (int16_t)p0, p1, p2, p3, p4, p5);
            break;
        case PR_setfsuid32:
            res = syscall_neutral_64(PR_setfsuid, (uid_t) p0, p1, p2, p3, p4, p5);
            break;
        case PR_setgroups32:
            res = syscall_neutral_64(PR_setgroups, (int) p0, (uint64_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_setgid:
            res = syscall_neutral_64(PR_setgid, (gid_t) (int16_t)p0, p1, p2, p3, p4, p5);
            break;
        case PR_setgid32:
            res = syscall_neutral_64(PR_setgid, (gid_t) p0, p1, p2, p3, p4, p5);
            break;
        case PR_sethostname:
            res = syscall_neutral_64(PR_sethostname, (uint64_t) g_2_h(p0), (size_t) p1, p2, p3, p4, p5);
            break;
        case PR_setitimer:
            res = setitimer_neutral(p0, p1, p2);
            break;
        case PR_setpgid:
            res = syscall_neutral_64(PR_setpgid, (pid_t)p0, (pid_t)p1, p2, p3, p4, p5);
            break;
        case PR_setpriority:
            res = syscall_neutral_64(PR_setpriority, (int) p0, (id_t) p1, (int) p2, p3, p4, p5);
            break;
        case PR_setresgid32:
            res = syscall_neutral_64(PR_setresgid32, (gid_t) p0, (gid_t) p1, (gid_t) p2, p3, p4, p5);
            break;
        case PR_setresuid32:
            res = syscall_neutral_64(PR_setresuid32, (uid_t) p0, (uid_t) p1, (uid_t) p2, p3, p4, p5);
            break;
        case PR_setreuid:
            res = syscall_neutral_64(PR_setreuid, (uid_t) (int16_t)p0, (uid_t) (int16_t)p1, p2, p3, p4, p5);
            break;
        case PR_setreuid32:
            res = syscall_neutral_64(PR_setreuid, (uid_t) p0, (uid_t) p1, p2, p3, p4, p5);
            break;
        case PR_setregid:
            res = syscall_neutral_64(PR_setregid, (gid_t) (int16_t)p0, (gid_t) (int16_t)p1, p2, p3, p4, p5);
            break;
        case PR_setregid32:
            res = syscall_neutral_64(PR_setregid, (gid_t) p0, (gid_t) p1, p2, p3, p4, p5);
            break;
        case PR_setrlimit:
            res = setrlimit_neutral(p0, p1);
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
        case PR_setuid32:
            res = syscall_neutral_64(PR_setuid, (uid_t) p0, p1, p2, p3, p4, p5);
            break;
        case PR_setxattr:
            res = syscall_neutral_64(PR_setxattr, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), (size_t) p3, (int) p4, p5);
            break;
        case PR_shmctl:
            res = shmctl_neutral(p0,p1,p2);
            break;
        case PR_shmget:
            res = syscall_neutral_64(PR_shmget, (key_t) p0, (size_t) p1, (int) p2, p3 ,p4, p5);
            break;
        case PR_shutdown:
            res = syscall_neutral_64(PR_shutdown, (int) p0, (int) p1, p2, p3 ,p4, p5);
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
        case PR_stat64:
            res = syscall_neutral_64(PR_stat64, (uint64_t) g_2_h(p0), (uint64_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_statfs:
            res = statfs_neutral(p0, p1);
            break;
        case PR_statfs64:
            res = statfs64_neutral(p0, p1, p2);
            break;
        case PR_symlink:
            res = syscall_neutral_64(PR_symlink, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_symlinkat:
            res = syscall_neutral_64(PR_symlinkat, (uint64_t) g_2_h(p0), (int) p1, (uint64_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_sync:
            res = syscall_neutral_64(PR_sync, p0, p1, p2, p3, p4, p5);
            break;
        case PR_sync_file_range:
            res = syscall_neutral_64(PR_sync_file_range, (int) p0, ((uint64_t)p2 << 32) + (uint64_t)p1,
                                                         ((uint64_t)p4 << 32) + (uint64_t)p3, (unsigned int) p5, 0, 0);
            break;
        case PR_sysinfo:
            res = sysinfo_neutral(p0);
            break;
        case PR_tee:
            res = syscall_neutral_64(PR_tee, (int) p0, (int) p1, (size_t) p2, (unsigned int) p3, p4, p5);
            break;
        case PR_timer_create:
            res = timer_create_neutral(p0, p1, p2);
            break;
        case PR_timer_delete:
            res = syscall_neutral_64(PR_timer_delete, p0, p1, p2, p3 ,p4, p5);
            break;
        case PR_timer_getoverrun:
            res = syscall_neutral_64(PR_timer_getoverrun, p0, p1, p2, p3 ,p4, p5);
            break;
        case PR_timer_gettime:
            res = timer_gettime_neutral(p0, p1);
            break;
        case PR_timer_settime:
            res = timer_settime_neutral(p0,p1,p2,p3);
            break;
        case PR_timerfd_create:
            res = syscall_neutral_64(PR_timerfd_create, (int) p0, (int) p1, p2, p3 ,p4, p5);
            break;
        case PR_timerfd_gettime:
            res = timerfd_gettime_neutral(p0, p1);
            break;
        case PR_timerfd_settime:
            res = timerfd_settime_neutral(p0, p1, p2, p3);
            break;
        case PR_times:
            res = times_neutral(p0);
            break;
        case PR_tkill:
            res = syscall_neutral_64(PR_tkill, (int) p0, (int) p1, p2, p3 ,p4, p5);
            break;
        case PR_truncate:
            res = syscall_neutral_64(PR_truncate, (uint64_t) g_2_h(p0), (off_t)(int) p1, p2, p3 ,p4, p5);
            break;
        case PR_truncate64:
            res = syscall_neutral_64(PR_truncate, (uint64_t) g_2_h(p0), ((uint64_t)p2 << 32) + (uint64_t)p1, p2, p3 ,p4, p5);
            break;
        case PR_umask:
            res = syscall_neutral_64(PR_umask, (mode_t) p0, p1, p2, p3 ,p4, p5);
            break;
        case PR_uname:
            res = syscall_neutral_64(PR_uname, (uint64_t) g_2_h(p0), p1, p2, p3 ,p4, p5);
            break;
        case PR_unlink:
            res = syscall_neutral_64(PR_unlink, (uint64_t) g_2_h(p0), p1, p2, p3 ,p4, p5);
            break;
        case PR_unlinkat:
            res = syscall_neutral_64(PR_unlinkat, (int)p0, (uint64_t) g_2_h(p1), (int)p2, p3, p4, p5);
            break;
        case PR_utimensat:
            res = utimensat_neutral(p0, p1, p2, p3);
            break;
        case PR_utimes:
            res = utimes_neutral(p0,p1);
            break;
        case PR_vfork:
            res = syscall_neutral_64(PR_vfork, p0, p1, p2, p3, p4, p5);
            break;
        case PR_vhangup:
            res = syscall_neutral_64(PR_vhangup, p0, p1, p2, p3, p4, p5);
            break;
        case PR_vmsplice:
            res = vmsplice_neutral(p0, p1, p2 , p3);
            break;
        case PR_waitid:
            res = waitid_neutral(p0, p1, p2, p3, p4);
            break;
        case PR_write:
            res = syscall_neutral_64(PR_write, (int)p0, (uint64_t)g_2_h(p1), (size_t)p2, p3, p4, p5);
            break;
        case PR_writev:
            res = writev_neutral(p0, p1, p2);
            break;
        default:
            fatal("syscall_adapter_guest32: unsupported neutral syscall %d\n", no);
    }

    return res;
}

