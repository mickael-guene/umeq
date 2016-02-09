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

#include "syscalls_neutral_types.h"
#include "syscalls_neutral.h"
#include "runtime.h"
#include "target32.h"
#include "umeq.h"

#define IS_NULL(px) ((uint64_t)((px)?g_2_h((px)):NULL))

#include <linux/futex.h>
#include <sys/time.h>
static int futex_neutral(uint32_t uaddr_p, uint32_t op_p, uint32_t val_p, uint32_t timeout_p, uint32_t uaddr2_p, uint32_t val3_p)
{
    int res;
    int *uaddr = (int *) g_2_h(uaddr_p);
    int op = (int) op_p;
    int val = (int) val_p;
    const struct neutral_timespec_32 *timeout_guest = (struct neutral_timespec_32 *) g_2_h(timeout_p);
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

    res = syscall_neutral_64(PR_futex, (uint64_t)uaddr, op, val, syscall_timeout, (uint64_t)uaddr2, val3);

    return res;
}

#include <sys/resource.h>
static  int getrlimit_neutral(uint32_t resource_p, uint32_t rlim_p)
{
    int res;
    int ressource = (int) resource_p;
    struct neutral_rlimit_32 *rlim_guest = (struct neutral_rlimit_32 *) g_2_h(rlim_p);
    struct rlimit rlim;

    if (rlim_p == 0 || rlim_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall_neutral_64(PR_getrlimit, ressource, (uint64_t)&rlim, 0, 0, 0, 0);
        rlim_guest->rlim_cur = rlim.rlim_cur;
        rlim_guest->rlim_max = rlim.rlim_max;
    }

    return res;
}

#include <sys/vfs.h>
static int statfs64_neutral(uint32_t path_p, uint32_t sz, uint32_t buf_p)
{
    const char *path = (const char *) g_2_h(path_p);
    struct neutral_statfs64_32 *buf_guest = (struct neutral_statfs64_32 *) g_2_h(buf_p);
    struct statfs buf;
    int res;

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

#include <fcntl.h>
static int fcnt64_neutral(uint32_t fd_p, uint32_t cmd_p, uint32_t opt_p)
{
    int res = -EINVAL;
    int fd = fd_p;
    int cmd = cmd_p;

    switch(cmd) {
        case F_DUPFD:/*0*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, (int)opt_p, 0, 0, 0);
            break;
        case F_GETFD:/*1*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, opt_p, 0, 0, 0);
            break;
        case F_SETFD:/*2*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, (int)opt_p, 0, 0, 0);
            break;
        case F_GETFL:/*3*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, opt_p, 0, 0, 0);
            break;
        case F_SETFL:/*4*/
            res = syscall_neutral_64(PR_fcntl, fd, cmd, opt_p, 0, 0, 0);
            break;
        case F_SETLKW:/*7*/
            {
                struct neutral_flock_32 *lock_guest = (struct neutral_flock_32 *) g_2_h(opt_p);
                struct flock lock;

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
    off_t offset = ((long)offset_high << 32) | offset_low;
    long lseek_res;

    lseek_res = syscall_neutral_64(PR_lseek, fd, offset, whence, 0, 0, 0);
    *result = lseek_res;

    return lseek_res<0?lseek_res:0;
}

static int clock_gettime_neutral(uint32_t clk_id_p, uint32_t tp_p)
{
    int res;
    clockid_t clk_id = (clockid_t) clk_id_p;
    struct neutral_timespec_32 *tp_guest = (struct neutral_timespec_32 *) g_2_h(tp_p);
    struct timespec tp;

    res = syscall_neutral_64(PR_clock_gettime, clk_id, (uint64_t)&tp, 0, 0, 0, 0);

    tp_guest->tv_sec = tp.tv_sec;
    tp_guest->tv_nsec = tp.tv_nsec;

    return res;
}

static int gettimeofday_neutral(uint32_t tv_p, uint32_t tz_p)
{
    int res;
    struct neutral_timeval_32 *tv_guest = (struct neutral_timeval_32 *) g_2_h(tv_p);
    struct timezone *tz = (struct timezone *) g_2_h(tz_p);
    struct timeval tv;

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

    res = syscall_neutral_64(PR_execve, (uint64_t)filename, (uint64_t)argv, (uint64_t)envp, 0, 0, 0);

    return res;
}

static int utimensat_neutral(uint32_t dirfd_p, uint32_t pathname_p, uint32_t times_p, uint32_t flags_p)
{
    int res;
    int dirfd = (int) dirfd_p;
    char * pathname = (char *) g_2_h(pathname_p);
    struct neutral_timespec_32 *times_guest = (struct neutral_timespec_32 *) g_2_h(times_p);
    int flags = (int) flags_p;
    struct timespec times[2];

    if (times_p) {
        times[0].tv_sec = times_guest[0].tv_sec;
        times[0].tv_nsec = times_guest[0].tv_nsec;
        times[1].tv_sec = times_guest[1].tv_sec;
        times[1].tv_nsec = times_guest[1].tv_nsec;
    }

    res = syscall_neutral_64(PR_utimensat, dirfd, (uint64_t)(pathname_p?pathname:NULL), (uint64_t)(times_p?times:NULL), flags, 0, 0);

    return res;
}

#include <sys/sysinfo.h>
static int sysinfo_neutral(uint32_t info_p)
{
    struct neutral_sysinfo_32 *sysinfo_guest = (struct neutral_sysinfo_32 *) g_2_h(info_p);
    struct sysinfo sysinfo;
    int res;

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
    int res;
    int nfds = (int) nfds_p;
    fd_set *readfds = (fd_set *) g_2_h(readfds_p);
    fd_set *writefds = (fd_set *) g_2_h(writefds_p);
    fd_set *exceptfds = (fd_set *) g_2_h(exceptfds_p);
    struct neutral_timeval_32 *timeout_guest = (struct neutral_timeval_32 *) g_2_h(timeout_p);
    struct timeval timeout;

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
    int res;
    int who = (int) who_p;
    struct neutral_rusage_32 *usage_guest = (struct neutral_rusage_32 *) g_2_h(usage_p);
    struct rusage rusage;

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

#include <string.h>
/* FIXME : use alloca for buffer */
static int getdents_neutral(uint32_t fd_p, uint32_t dirp_p, uint32_t count_p)
{
    unsigned int fd = (unsigned int) fd_p;
    struct neutral_linux_dirent_32  *dirp = (struct neutral_linux_dirent_32 *) g_2_h(dirp_p);
    unsigned int count = (unsigned int) count_p;
    char buffer[4096];
    struct neutral_linux_dirent_64 *dirp_64 = (struct neutral_linux_dirent_64 *) buffer;
    int res_32 = 0;
    int res;

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
    struct timespec req;
    struct timespec rem;
    int res;

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
    int res;
    int which = (int) which_p;
    struct neutral_itimerval_32 *new_value_guest = (struct neutral_itimerval_32 *) g_2_h(new_value_p);
    struct neutral_itimerval_32 *old_value_guest = (struct neutral_itimerval_32 *) g_2_h(old_value_p);
    struct itimerval new_value;
    struct itimerval old_value;

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
        case PR_access:
            res = syscall_neutral_64(PR_access, (uint64_t)g_2_h(p0), (int)p1, p2, p3, p4, p5);
            break;
        case PR_chdir:
            res = syscall_neutral_64(PR_chdir, (uint64_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_chmod:
            res = syscall_neutral_64(PR_chmod, (uint64_t) g_2_h(p0), (mode_t) p1, p2, p3, p4, p5);
            break;
        case PR_clock_gettime:
            res = clock_gettime_neutral(p0, p1);
            break;
        case PR_close:
            res = syscall_neutral_64(PR_close, (int)p0, p1, p2, p3, p4, p5);
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
        case PR_fchdir:
            res = syscall_neutral_64(PR_fchdir, (int)p0, p1, p2, p3, p4, p5);
            break;
        case PR_fchown32:
            res = syscall_neutral_64(PR_fchown, (unsigned int)p0, (uid_t)p1, (gid_t)p2, p3, p4, p5);
            break;
        case PR_fcntl64:
            res = fcnt64_neutral(p0, p1, p2);
            break;
        case PR_futex:
            res = futex_neutral(p0, p1, p2, p3, p4, p5);
            break;
        case PR_fstat64:
            res = syscall_neutral_64(PR_fstat64, p0, (uint64_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_fstatat64:
            res = syscall_neutral_64(PR_fstatat64, (int)p0, (uint64_t)g_2_h(p1), (uint64_t)g_2_h(p2), (int)p3, p4, p5);
            break;
        case PR_fsync:
            res = syscall_neutral_64(PR_fsync, (int)p0, p1, p2, p3, p4, p5);
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
        case PR_getegid32:
            res = syscall_neutral_64(PR_getegid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_geteuid32:
            res = syscall_neutral_64(PR_geteuid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_getgid32:
            res = syscall_neutral_64(PR_getgid, p0, p1, p2, p3, p4, p5);
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
        case PR_getrlimit:
            res = getrlimit_neutral(p0, p1);
            break;
        case PR_gettid:
            res = syscall_neutral_64(PR_gettid, p0, p1, p2, p3, p4, p5);
            break;
        case PR_gettimeofday:
            res = gettimeofday_neutral(p1, p2);
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
        case PR_getxattr:
            res = syscall_neutral_64(PR_getxattr, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), (size_t) p3, p4, p5);
            break;
        case PR_ioctl:
            /* FIXME: need specific version to offset param according to ioctl */
            res = syscall_neutral_64(PR_ioctl, (int) p0, (unsigned long) p1, (uint64_t) g_2_h(p2), p3, p4, p5);
            break;
        case PR_kill:
            res = syscall_neutral_64(PR_kill, (pid_t)p0, (int)p1, p2, p3, p4, p5);
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
        case PR_mprotect:
            res = syscall_neutral_64(PR_mprotect, (uint64_t) g_2_h(p0), (size_t) p1, (int) p2, p3, p4, p5);
            break;
        case PR_nanosleep:
            res = nanosleep_neutral(p0, p1);
            break;
        case PR_personality:
            res = syscall_neutral_64(PR_personality, (unsigned int)p0, p1, p2, p3, p4, p5);
            break;
        case PR_pipe:
            res = syscall_neutral_64(PR_pipe, (uint64_t)g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_poll:
            res = syscall_neutral_64(PR_poll, (uint64_t)g_2_h(p0), (unsigned int)p1, (int)p2, p3, p4, p5);
            break;
        case PR_pread64:
            res = syscall_neutral_64(PR_pread64, (unsigned int)p0, (uint64_t)g_2_h(p1), (size_t)p2, 
                                                 ((uint64_t)p4 << 32) + (uint64_t)p3, 0, 0);
            break;
        case PR_prlimit64:
            res = syscall_neutral_64(PR_prlimit64, p0, p1, IS_NULL(p2), IS_NULL(p3), p4, p5);
            break;
        case PR_read:
            res = syscall_neutral_64(PR_read, (int)p0, (uint64_t) g_2_h(p1), (size_t) p2, p3, p4, p5);
            break;
        case PR_readlink:
            res = syscall_neutral_64(PR_readlink, (uint64_t)g_2_h(p0), (uint64_t)g_2_h(p1), (size_t)p2, p3, p4, p5);
            break;
        case PR_rename:
            res = syscall_neutral_64(PR_rename, (uint64_t)g_2_h(p0), (uint64_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_rt_sigprocmask:
            res = syscall_neutral_64(PR_rt_sigprocmask, (int)p0, IS_NULL(p1), IS_NULL(p2), (size_t) p3, p4, p5);
            break;
        case PR_rt_sigsuspend:
            res = syscall_neutral_64(PR_rt_sigsuspend, (uint64_t)g_2_h(p0), (size_t)p1, p2, p3, p4, p5);
            break;
        case PR_tgkill:
            res = syscall_neutral_64(PR_tgkill, (int)p0, (int)p1, (int)p2, p3, p4, p5);
            break;
        case PR_set_robust_list:
            /* FIXME: implement this correctly */
            res = -EPERM;
            break;
        case PR_set_tid_address:
            res = syscall_neutral_64(PR_set_tid_address, (uint64_t) g_2_h(p0), p1, p2, p3, p4, p5);
            break;
        case PR_setitimer:
            res = setitimer_neutral(p0, p1, p2);
            break;
        case PR_setpgid:
            res = syscall_neutral_64(PR_setpgid, (pid_t)p0, (pid_t)p1, p2, p3, p4, p5);
            break;
        case PR_setreuid32:
            res = syscall_neutral_64(PR_setreuid, (uid_t) p0, (uid_t) p1, p2, p3, p4, p5);
            break;
        case PR_setxattr:
            res = syscall_neutral_64(PR_setxattr, (uint64_t) g_2_h(p0), (uint64_t) g_2_h(p1), (uint64_t) g_2_h(p2), (size_t) p3, (int) p4, p5);
            break;
        case PR_socket:
            res = syscall_neutral_64(PR_socket, (int)p0, (int)p1, (int)p2, p3 ,p4, p5);
            break;
        case PR_socketpair:
            res = syscall_neutral_64(PR_socketpair, (int)p0, (int)p1, (int)p2, (uint64_t) g_2_h(p3), p4, p5);
            break;
        case PR_stat64:
            res = syscall_neutral_64(PR_stat64, (uint64_t) g_2_h(p0), (uint64_t)g_2_h(p1), p2, p3, p4, p5);
            break;
        case PR_statfs64:
            res = statfs64_neutral(p0, p1, p2);
            break;
        case PR_sysinfo:
            res = sysinfo_neutral(p0);
            break;
        case PR_umask:
            res = syscall_neutral_64(PR_umask, (mode_t) p0, p1, p2, p3 ,p4, p5);
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
        case PR_vfork:
            res = syscall_neutral_64(PR_vfork, p0, p1, p2, p3, p4, p5);
            break;
        case PR_write:
            res = syscall_neutral_64(PR_write, (int)p0, (uint64_t)g_2_h(p1), (size_t)p2, p3, p4, p5);
            break;
        default:
            fatal("syscall_adapter_guest32: unsupported neutral syscall %d\n", no);
    }

    return res;
}

