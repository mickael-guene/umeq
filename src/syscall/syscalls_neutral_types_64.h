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

typedef struct neutral_siginfo_64 {
    int32_t si_signo;
    int32_t si_errno;
    int32_t si_code;
    union {
        int32_t _pad[29];
        /* kill */
        struct {
            int32_t _si_pid;
            uint32_t _si_uid;
        } _kill;
        /* POSIX.1b timers */
        struct {
            int32_t _si_tid;
            int32_t _si_overrun;
            union neutral_sigval_64 _si_sigval;
        } _timer;
        /* POSIX.1b signals */
        struct {
            int32_t _si_pid;
            uint32_t _si_uid;
            union neutral_sigval_64 _si_sigval;
        } _rt;
        /* SIGCHLD */
        struct {
            int32_t _si_pid;
            uint32_t _si_uid;
            int32_t _si_status;
            int64_t _si_utime;
            int64_t _si_stime;
        } _sigchld;
        /* SIGILL, SIGFPE, SIGSEGV, SIGBUS */
        struct {
            uint64_t _si_addr;
        } _sigfault;
        /* SIGPOLL */
        struct {
            int64_t _si_band;
            int32_t _si_fd;
        } _sigpoll;
    } _sifields;
} neutral_siginfo_t_64;

struct neutral_msghdr_64 {
    uint64_t msg_name;
    int32_t msg_namelen;
    uint32_t __pad0;
    uint64_t msg_iov;
    uint64_t msg_iovlen;
    uint64_t msg_control;
    uint64_t msg_controllen;
    uint32_t msg_flags;
    uint32_t __pad1;
};

struct neutral_mmsghdr_64 {
    struct neutral_msghdr_64 msg_hdr;
    uint32_t msg_len;
};

struct neutral_ipc64_perm_64 {
    int32_t key;
    uint32_t uid;
    uint32_t gid;
    uint32_t cuid;
    uint32_t cgid;
    uint32_t mode;
    uint16_t seq;
    uint16_t __pad2;
    uint32_t __pad3;
    uint64_t __unused1;
    uint64_t __unused2;
};

struct neutral_shmid64_ds_64 {
    struct neutral_ipc64_perm_64 shm_perm;
    uint64_t shm_segsz;
    int64_t shm_atime;
    int64_t shm_dtime;
    int64_t shm_ctime;
    int32_t shm_cpid;
    int32_t shm_lpid;
    uint64_t shm_nattch;
    uint64_t __unused4;
    uint64_t __unused5;
};

struct neutral_shminfo64_64 {
    uint64_t shmmax;
    uint64_t shmmin;
    uint64_t shmmni;
    uint64_t shmseg;
    uint64_t shmall;
    uint64_t __unused1;
    uint64_t __unused2;
    uint64_t __unused3;
    uint64_t __unused4;
};

struct neutral_shm_info_64 {
    int32_t used_ids;
    uint32_t __pad0;
    uint64_t shm_tot;
    uint64_t shm_rss;
    uint64_t shm_swp;
    uint64_t swap_attempts;
    uint64_t swap_successes;
};

struct neutral_semid64_ds_64 {
    struct neutral_ipc64_perm_64 sem_perm;
    int64_t sem_otime; /* last semop time */
    int64_t sem_ctime; /* last change time */
    uint64_t sem_nsems; /* no. of semaphores in array */
    uint64_t __unused3;
    uint64_t __unused4;
};

struct neutral_msqid64_ds_64 {
    struct neutral_ipc64_perm_64 msg_perm;
    int64_t msg_stime;
    int64_t msg_rtime;
    int64_t msg_ctime;
    uint64_t __msg_cbytes;
    uint64_t msg_qnum;
    uint64_t msg_qbytes;
    int32_t msg_lspid;
    int32_t msg_lrpid;
    uint64_t __unused4;
    uint64_t __unused5;
};

struct neutral_stat_64 {
    uint64_t            st_dev;      /* Device.  */
    uint64_t            st_ino;      /* File serial number.  */
    uint32_t            st_mode;        /* File mode.  */
    uint32_t            st_nlink;       /* Link count.  */
    uint32_t            st_uid;         /* User ID of the file's owner.  */
    uint32_t            st_gid;         /* Group ID of the file's group. */
    uint64_t            st_rdev;     /* Device number, if device.  */
    uint64_t            __pad1;
    int64_t             st_size;        /* Size of file, in bytes.  */
    int32_t             st_blksize;     /* Optimal block size for I/O.  */
    int32_t             __pad2;
    int64_t             st_blocks;      /* Number 512-byte blocks allocated. */
    int64_t             st_atime_;       /* Time of last access.  */
    uint64_t            st_atime_nsec;
    int64_t             st_mtime_;       /* Time of last modification.  */
    uint64_t            st_mtime_nsec;
    int64_t             st_ctime_;       /* Time of last status change.  */
    uint64_t            st_ctime_nsec;
    uint32_t            __unused4;
    uint32_t            __unused5;
};

#endif

#ifdef __cplusplus
}
#endif
