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

#include "syscalls_neutral_types_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __SYSCALLS_NEUTRAL_TYPES_64__
#define __SYSCALLS_NEUTRAL_TYPES_64__ 1

struct neutral_timespec_64 {
    int64_t tv_sec;                /* seconds */
    int64_t tv_nsec;               /* nanoseconds */
};

struct neutral_rlimit_64 {
    uint64_t rlim_cur;
    uint64_t rlim_max;
};

struct neutral_statfs_64 {
    int64_t f_type;
    int64_t f_bsize;
    int64_t f_blocks;
    int64_t f_bfree;
    int64_t f_bavail;
    int64_t f_files;
    int64_t f_ffree;
    __neutral_kernel_fsid_t f_fsid;
    int64_t f_namelen;
    int64_t f_frsize;
    int64_t f_flags;
    int64_t f_spare[4];
};

struct neutral_linux_dirent_64 {
    uint64_t    d_ino;
    uint64_t    d_off;
    uint16_t    d_reclen;
    char        d_name[1];
};

struct neutral_flock_64 {
    int16_t     l_type;
    int16_t     l_whence;
    uint32_t    __pad0;
    int64_t     l_start;
    int64_t     l_len;
    int32_t     l_pid;
    uint32_t    __pad1;
};

struct neutral_msgbuf_64 {
    uint64_t mtype;
    char mtext[1];
};

struct neutral_timeval_64 {
    int64_t tv_sec;
    int64_t tv_usec;
};

struct neutral_sysinfo_64 {
    int64_t uptime;
    uint64_t loads[3];
    uint64_t totalram;
    uint64_t freeram;
    uint64_t sharedram;
    uint64_t bufferram;
    uint64_t totalswap;
    uint64_t freeswap;
    uint16_t procs;
    uint16_t pad;
    uint32_t __pad0;
    uint64_t totalhigh;
    uint64_t freehigh;
    uint32_t mem_unit;
    char _f[20 - 2 * sizeof(int64_t) - sizeof(int32_t)];
};

struct neutral_rusage_64 {
    struct neutral_timeval_64 ru_utime; /* user time used */
    struct neutral_timeval_64 ru_stime; /* system time used */
    int64_t ru_maxrss;      /* maximum resident set size */
    int64_t ru_ixrss;       /* integral shared memory size */
    int64_t ru_idrss;       /* integral unshared data size */
    int64_t ru_isrss;       /* integral unshared stack size */
    int64_t ru_minflt;      /* page reclaims */
    int64_t ru_majflt;      /* page faults */
    int64_t ru_nswap;       /* swaps */
    int64_t ru_inblock;     /* block input operations */
    int64_t ru_oublock;     /* block output operations */
    int64_t ru_msgsnd;      /* messages sent */
    int64_t ru_msgrcv;      /* messages received */
    int64_t ru_nsignals;        /* signals received */
    int64_t ru_nvcsw;       /* voluntary context switches */
    int64_t ru_nivcsw;      /* involuntary " */
};

struct neutral_itimerval_64 {
    struct neutral_timeval_64 it_interval;
    struct neutral_timeval_64 it_value;
};

struct neutral_tms_64 {
    int64_t tms_utime;
    int64_t tms_stime;
    int64_t tms_cutime;
    int64_t tms_cstime;
};

/* NOTE : iov_len is uint64_t but internally kernel cast it to int64_t when
   checking size limits. So we use int32_t so we extend sign. */
struct neutral_iovec_64 {
    uint64_t iov_base;
    int64_t iov_len;
};

union neutral_sigval_64 {
    int32_t sival_int;
    uint64_t sival_ptr;
};

struct neutral_sigevent_64 {
    union neutral_sigval_64 sigev_value;
    int32_t sigev_signo;
    int32_t sigev_notify;
    /* FIXME : union for SIGEV_THREAD+ SIGEV_THREAD_ID */
    union {
        int _pad[64 - 4];
    } _sigev_un;
};

struct neutral_statfs64_64 {
    int64_t f_type;
    int64_t f_bsize;
    uint64_t f_blocks;
    uint64_t f_bfree;
    uint64_t f_bavail;
    uint64_t f_files;
    uint64_t f_ffree;
    __neutral_kernel_fsid_t f_fsid;
    int64_t f_namelen;
    int64_t f_frsize;
    int64_t f_flags;
    int64_t f_spare[4];
};

struct neutral_itimerspec_64 {
    struct neutral_timespec_64 it_interval;  /* Timer interval */
    struct neutral_timespec_64 it_value;     /* Initial expiration */
};

struct neutral_mq_attr_64 {
    int64_t mq_flags;
    int64_t mq_maxmsg;
    int64_t mq_msgsize;
    int64_t mq_curmsgs;
    int64_t __reserved[4];
};

struct neutral_ipc_perm_64 {
    int32_t __key;
    uint32_t uid;
    uint32_t gid;
    uint32_t cuid;
    uint32_t cgid;
    uint32_t mode;
    uint16_t seq;
    uint16_t __pad0;
};

struct neutral_shmid_ds_64 {
    struct neutral_ipc_perm_64 shm_perm;
    int32_t shm_segsz;
    uint32_t __pad0;
    int64_t shm_atime;
    int64_t shm_dtime;
    int64_t shm_ctime;
    int32_t shm_cpid;
    int32_t shm_lpid;
    uint16_t shm_nattch;
    uint16_t shm_unused;
    uint64_t shm_unused2;
    uint64_t shm_unused3;
};

#endif

#ifdef __cplusplus
}
#endif
