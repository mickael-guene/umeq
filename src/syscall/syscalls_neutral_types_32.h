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

#ifndef __SYSCALLS_NEUTRAL_TYPES_32__
#define __SYSCALLS_NEUTRAL_TYPES_32__ 1

struct neutral_timespec_32 {
    int32_t tv_sec;                /* seconds */
    int32_t tv_nsec;               /* nanoseconds */
};

struct neutral_rlimit_32 {
    uint32_t rlim_cur;
    uint32_t rlim_max;
};

struct neutral_statfs64_32 {
    uint32_t f_type;
    uint32_t f_bsize;
    uint64_t f_blocks;
    uint64_t f_bfree;
    uint64_t f_bavail;
    uint64_t f_files;
    uint64_t f_ffree;
    __neutral_kernel_fsid_t f_fsid;
    uint32_t f_namelen;
    uint32_t f_frsize;
    uint32_t f_flags;
    uint32_t f_spare[4];
    uint32_t __compiler_align_padding;
};

struct neutral_timeval_32 {
    int32_t tv_sec;
    int32_t tv_usec;
};

struct neutral_itimerval_32 {
   struct neutral_timeval_32 it_interval;
   struct neutral_timeval_32 it_value;
};

struct neutral_sysinfo_32 {
    int32_t uptime;
    uint32_t loads[3];
    uint32_t totalram;
    uint32_t freeram;
    uint32_t sharedram;
    uint32_t bufferram;
    uint32_t totalswap;
    uint32_t freeswap;
    uint16_t procs;
    uint16_t pad;
    uint32_t totalhigh;
    uint32_t freehigh;
    uint32_t mem_unit;
    char _f[20 - 2 * sizeof(uint32_t) - sizeof(uint32_t)];
};

struct neutral_rusage_32 {
    struct neutral_timeval_32 ru_utime; /* user time used */
    struct neutral_timeval_32 ru_stime; /* system time used */
    int32_t ru_maxrss;      /* maximum resident set size */
    int32_t ru_ixrss;       /* integral shared memory size */
    int32_t ru_idrss;       /* integral unshared data size */
    int32_t ru_isrss;       /* integral unshared stack size */
    int32_t ru_minflt;      /* page reclaims */
    int32_t ru_majflt;      /* page faults */
    int32_t ru_nswap;       /* swaps */
    int32_t ru_inblock;     /* block input operations */
    int32_t ru_oublock;     /* block output operations */
    int32_t ru_msgsnd;      /* messages sent */
    int32_t ru_msgrcv;      /* messages received */
    int32_t ru_nsignals;        /* signals received */
    int32_t ru_nvcsw;       /* voluntary context switches */
    int32_t ru_nivcsw;      /* involuntary " */
};

struct neutral_linux_dirent_32 {
    uint32_t    d_ino;
    uint32_t    d_off;
    uint16_t    d_reclen;
    char        d_name[1];
};


struct neutral_flock_32 {
    int16_t     l_type;
    int16_t     l_whence;
    int32_t     l_start;
    int32_t     l_len;
    int32_t     l_pid;
};

struct neutral_tms_32 {
    int32_t tms_utime;
    int32_t tms_stime;
    int32_t tms_cutime;
    int32_t tms_cstime;
};

struct neutral_statfs_32 {
    uint32_t f_type;
    uint32_t f_bsize;
    uint32_t f_blocks;
    uint32_t f_bfree;
    uint32_t f_bavail;
    uint32_t f_files;
    uint32_t f_ffree;
    __neutral_kernel_fsid_t f_fsid;
    uint32_t f_namelen;
    uint32_t f_frsize;
    uint32_t f_flags;
    uint32_t f_spare[4];
};

/* NOTE : iov_len is uint32_t but internally kernel cast it to int32_t when
   checking size limits. So we use int32_t so we extend sign. */
struct neutral_iovec_32 {
    uint32_t iov_base;
    int32_t iov_len;
};

union neutral_sigval_32 {
    int32_t sival_int;
    uint32_t sival_ptr;
};

struct neutral_sigevent_32 {
    union neutral_sigval_32 sigev_value;
    int32_t sigev_signo;
    int32_t sigev_notify;
    /* FIXME : union for SIGEV_THREAD+ SIGEV_THREAD_ID */
    union {
        int _pad[64 - 3];
    } _sigev_un;
};

typedef struct neutral_siginfo_32 {
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
            union neutral_sigval_32 _si_sigval;
        } _timer;
        /* POSIX.1b signals */
        struct {
            int32_t _si_pid;
            uint32_t _si_uid;
            union neutral_sigval_32 _si_sigval;
        } _rt;
        /* SIGCHLD */
        struct {
            int32_t _si_pid;
            uint32_t _si_uid;
            int32_t _si_status;
            int32_t _si_utime;
            int32_t _si_stime;
        } _sigchld;
        /* SIGILL, SIGFPE, SIGSEGV, SIGBUS */
        struct {
            uint32_t _si_addr;
        } _sigfault;
        /* SIGPOLL */
        struct {
            int32_t _si_band;
            int32_t _si_fd;
        } _sigpoll;
    } _sifields;
} neutral_siginfo_t_32;

struct neutral_itimerspec_32 {
    struct neutral_timespec_32 it_interval;  /* Timer interval */
    struct neutral_timespec_32 it_value;     /* Initial expiration */
};

struct neutral_mq_attr_32 {
    int32_t mq_flags;
    int32_t mq_maxmsg;
    int32_t mq_msgsize;
    int32_t mq_curmsgs;
    int32_t __reserved[4];
};

struct neutral_ipc64_perm_32 {
    int32_t key;
    uint32_t uid;
    uint32_t gid;
    uint32_t cuid;
    uint32_t cgid;
    uint32_t mode;
    uint16_t seq;
    uint16_t __pad2;
    uint32_t __unused1;
    uint32_t __unused2;
};

struct neutral_shmid64_ds_32 {
    struct neutral_ipc64_perm_32 shm_perm;
    uint32_t shm_segsz;
    int32_t shm_atime;
    uint32_t __unused1;
    int32_t shm_dtime;
    uint32_t __unused2;
    int32_t shm_ctime;
    uint32_t __unused3;
    int32_t shm_cpid;
    int32_t shm_lpid;
    uint32_t shm_nattch;
    uint32_t __unused4;
    uint32_t __unused5;
};

struct neutral_shminfo64_32 {
    uint32_t shmmax;
    uint32_t shmmin;
    uint32_t shmmni;
    uint32_t shmseg;
    uint32_t shmall;
    uint32_t __unused1;
    uint32_t __unused2;
    uint32_t __unused3;
    uint32_t __unused4;
};

struct neutral_shm_info_32 {
    int32_t used_ids;
    uint32_t shm_tot;
    uint32_t shm_rss;
    uint32_t shm_swp;
    uint32_t swap_attempts;
    uint32_t swap_successes;
};

struct neutral_semid64_ds_32 {
    struct neutral_ipc64_perm_32 sem_perm;
    int32_t sem_otime; /* last semop time */
    uint32_t __unused1;
    int32_t sem_ctime; /* last change time */
    uint32_t __unused2;
    uint32_t sem_nsems; /* no. of semaphores in array */
    uint32_t __unused3;
    uint32_t __unused4;
};

struct neutral_msgbuf_32 {
    int32_t mtype;
    char mtext[1];
};

struct neutral_msqid64_ds_32 {
    struct neutral_ipc64_perm_32 msg_perm;
    int32_t msg_stime;
    uint32_t __unused1;
    int32_t msg_rtime;
    uint32_t __unused2;
    int32_t msg_ctime;
    uint32_t __unused3;
    uint32_t __msg_cbytes;
    uint32_t msg_qnum;
    uint32_t msg_qbytes;
    int32_t msg_lspid;
    int32_t msg_lrpid;
    uint32_t __unused4;
    uint32_t __unused5;
};

struct neutral_flock64_32 {
    int16_t l_type;
    int16_t l_whence;
    uint32_t __pad0;
    int64_t l_start;
    int64_t l_len;
    int32_t l_pid;
};

struct neutral_msghdr_32 {
    uint32_t msg_name;
    int32_t msg_namelen;
    uint32_t msg_iov;
    uint32_t msg_iovlen;
    uint32_t msg_control;
    uint32_t msg_controllen;
    uint32_t msg_flags;
};

struct neutral_epoll_event_32 {
    uint32_t events;
    uint32_t __pad0;
    uint64_t data;
};

struct neutral_mmsghdr_32 {
    struct neutral_msghdr_32 msg_hdr;
    uint32_t msg_len;
};

#endif

#ifdef __cplusplus
}
#endif
