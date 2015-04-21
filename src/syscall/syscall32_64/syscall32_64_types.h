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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __SYSCALL32_64_TYPES__
#define __SYSCALL32_64_TYPES__ 1

#include <sys/syscall.h>
#include <sys/types.h>
#include <stdint.h>

struct stat64_32 {
    uint64_t    st_dev;
    uint8_t   __pad0[4];

    uint32_t    __st_ino;
    uint32_t    st_mode;
    uint32_t    st_nlink;

    uint32_t    st_uid;
    uint32_t    st_gid;

    uint64_t    st_rdev;
    uint8_t   __pad3[4];

    int64_t     st_size;
    uint32_t    st_blksize;
    uint64_t    st_blocks;

    uint32_t    _st_atime;
    uint32_t    st_atime_nsec;

    uint32_t    _st_mtime;
    uint32_t    st_mtime_nsec;

    uint32_t    _st_ctime;
    uint32_t    st_ctime_nsec;

    uint64_t    st_ino;
};

struct linux_dirent_32 {
    uint32_t    d_ino;
    uint32_t    d_off;
    uint16_t    d_reclen;
    char        d_name[1];
};

struct linux_dirent64_32 {
    uint64_t    d_ino;
    uint64_t    d_off;
    uint16_t    d_reclen;
    uint8_t     d_type;
    char        d_name[1];
};

struct linux_dirent_64 {
    uint64_t    d_ino;
    uint64_t    d_off;
    uint16_t    d_reclen;
    char        d_name[1];
};

struct timespec_32 {
   int32_t tv_sec;                /* seconds */
   int32_t tv_nsec;               /* nanoseconds */
};

struct timeval_32 {
   int32_t tv_sec;                /* seconds */
   int32_t tv_usec;               /* microseconds */
};

struct rlimit_32 {
    uint32_t rlim_cur;
    uint32_t rlim_max;
};

struct rlimit64_32 {
    uint64_t rlim_cur;
    uint64_t rlim_max;
};

struct rusage_32 {
    struct timeval_32 ru_utime; /* user time used */
    struct timeval_32 ru_stime; /* system time used */
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

struct iovec_32 {
    uint32_t iov_base;
    int32_t iov_len;
};

typedef struct {
    int32_t val[2];
} __kernel_fsid_32_t;

struct statfs_32 {
    uint32_t f_type;
    uint32_t f_bsize;
    uint32_t f_blocks;
    uint32_t f_bfree;
    uint32_t f_bavail;
    uint32_t f_files;
    uint32_t f_ffree;
    __kernel_fsid_32_t f_fsid;
    uint32_t f_namelen;
    uint32_t f_frsize;
    uint32_t f_flags;
    uint32_t f_spare[4];
};

struct statfs64_32 {
    uint32_t f_type;
    uint32_t f_bsize;
    uint64_t f_blocks;
    uint64_t f_bfree;
    uint64_t f_bavail;
    uint64_t f_files;
    uint64_t f_ffree;
    __kernel_fsid_32_t f_fsid;
    uint32_t f_namelen;
    uint32_t f_frsize;
    uint32_t f_flags;
    uint32_t f_spare[4];
};

struct flock_32 {
    uint16_t  l_type;
    uint16_t  l_whence;
    int32_t l_start;
    uint32_t l_len;
    int  l_pid;
};

struct flock64_32 {
    uint16_t  l_type;
    uint16_t  l_whence;
    uint64_t l_start;
    uint64_t l_len;
    int  l_pid;
};

struct msghdr_32 {
    uint32_t msg_name;
    uint32_t msg_namelen;
    uint32_t msg_iov;
    uint32_t msg_iovlen;
    uint32_t msg_control;
    uint32_t msg_controllen;
    uint32_t msg_flags;
};

struct mmsghdr_32 {
    struct msghdr_32 msg_hdr;
    uint32_t msg_len;
};

struct sysinfo_32 {
    int32_t uptime;
    uint32_t loads[3];
    uint32_t totalram;
    uint32_t freeram;
    uint32_t sharedram;
    uint32_t bufferram;
    uint32_t totalswap;
    uint32_t freeswap;
    uint16_t procs;
    uint32_t totalhigh;
    uint32_t freehigh;
    uint32_t mem_unit;
};

struct itimerval_32 {
   struct timeval_32 it_interval; /* next value */
   struct timeval_32 it_value;    /* current value */
};

union sigval_32 {
    uint32_t sival_int;
    uint32_t sival_ptr;
};

struct sigevent_32 {
    union sigval_32 sigev_value;
    uint32_t sigev_signo;
    uint32_t sigev_notify;
    /* FIXME : union for SIGEV_THREAD+ SIGEV_THREAD_ID */
};

struct itimerspec_32 {
    struct timespec_32 it_interval;  /* Timer interval */
    struct timespec_32 it_value;     /* Initial expiration */
};

typedef struct siginfo_32 {
    uint32_t si_signo;
    uint32_t si_errno;
    uint32_t si_code;
    union {
        uint32_t _pad[29];
        /* kill */
        struct {
            uint32_t _si_pid;
            uint32_t _si_uid;
        } _kill;
        /* POSIX.1b timers */
        struct {
            uint32_t _si_tid;
            uint32_t _si_overrun;
            union sigval_32 _si_sigval;
        } _timer;
        /* POSIX.1b signals */
        struct {
            uint32_t _si_pid;
            uint32_t _si_uid;
            union sigval_32 _si_sigval;
        } _rt;
        /* SIGCHLD */
        struct {
            uint32_t _si_pid;
            uint32_t _si_uid;
            uint32_t _si_status;
            uint32_t _si_utime;
            uint32_t _si_stime;
        } _sigchld;
        /* SIGILL, SIGFPE, SIGSEGV, SIGBUS */
        struct {
            uint32_t _si_addr;
        } _sigfault;
        /* SIGPOLL */
        struct {
            uint32_t _si_band;
            uint32_t _si_fd;
        } _sigpoll;
    } _sifields;
} siginfo_t_32;

struct ipc_perm_32 {
    int32_t __key;
    uint32_t uid;
    uint32_t gid;
    uint32_t cuid;
    uint32_t cgid;
    uint16_t mode;
    uint16_t __pad1;
    uint16_t __seq;
    uint16_t __pad2;
    uint32_t __unused1;
    uint32_t __unused2;
};

struct shmid_ds_32 {
    struct ipc_perm_32 shm_perm;
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

struct semid_ds_32 {
    struct ipc_perm_32 sem_perm;  /* Ownership and permissions */
    uint32_t sem_otime; /* Last semop time */
    uint32_t __unused1;
    uint32_t sem_ctime; /* Last change time */
    uint32_t __unused2;
    uint32_t sem_nsems; /* No. of semaphores in set */
    uint32_t __unused3;
    uint32_t __unused4;
};

struct msqid_ds_32 {
	struct ipc_perm_32 msg_perm;
	uint32_t msg_stime;
	uint32_t __unused1;
	uint32_t msg_rtime;
	uint32_t __unused2;
	uint32_t msg_ctime;
	uint32_t __unused3;
	uint32_t __msg_cbytes;
	uint32_t msg_qnum;
	uint32_t msg_qbytes;
	uint32_t msg_lspid;
	uint32_t msg_lrpid;
	uint32_t __unused4;
	uint32_t __unused5;
};

struct sembuf_32 {
    uint16_t sem_num;
    int16_t sem_op;
    int16_t sem_flg;
};

struct tms_32 {
    uint32_t tms_utime;
    uint32_t tms_stime;
    uint32_t tms_cutime;
    uint32_t tms_cstime;
};

struct epoll_event_32 {
    uint32_t events;
    uint32_t __pad0;
    uint64_t data;
};

struct msgbuf_32 {
    int32_t mtype;
    char mtext[1];
};

struct stat_32 {
    uint32_t st_dev;
    uint32_t st_ino;
    uint16_t st_mode;
    uint16_t st_nlink;
    uint16_t st_uid;
    uint16_t st_gid;
    uint32_t st_rdev;
    uint32_t st_size;
    uint32_t st_blksize;
    uint32_t st_blocks;
    uint32_t _st_atime;
    uint32_t st_atime_nsec;
    uint32_t _st_mtime;
    uint32_t st_mtime_nsec;
    uint32_t _st_ctime;
    uint32_t st_ctime_nsec;
    uint32_t __unused4;
    uint32_t __unused5;
};

struct mq_attr_32 {
    uint32_t mq_flags;
    uint32_t mq_maxmsg;
    uint32_t mq_msgsize;
    uint32_t mq_curmsgs;
    uint32_t __reserved[4];
};

struct msginfo_32 {
    uint32_t msgpool;
    uint32_t msgmap;
    uint32_t msgmax;
    uint32_t msgmnb;
    uint32_t msgmni;
    uint32_t msgssz;
    uint32_t msgtql;
    uint16_t msgseg;
};

struct shminfo64_32 {
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

struct shm_info_32 {
    int32_t used_ids;
    uint32_t shm_tot;
    uint32_t shm_rss;
    uint32_t shm_swp;
    uint32_t swap_attempts;
    uint32_t swap_successes;
};

#endif

#ifdef __cplusplus
}
#endif
