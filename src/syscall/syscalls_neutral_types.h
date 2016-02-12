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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __SYSCALLS_NEUTRAL_TYPES__
#define __SYSCALLS_NEUTRAL_TYPES__ 1

struct neutral_stat64 {
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
    int32_t             st_atime_;       /* Time of last access.  */
    uint32_t            st_atime_nsec;
    int32_t             st_mtime_;       /* Time of last modification.  */
    uint32_t            st_mtime_nsec;
    int32_t             st_ctime_;       /* Time of last status change.  */
    uint32_t            st_ctime_nsec;
    uint32_t            __unused4;
    uint32_t            __unused5;
};

struct neutral_timespec_32 {
    int32_t tv_sec;                /* seconds */
    int32_t tv_nsec;               /* nanoseconds */
};

struct neutral_rlimit_32 {
    uint32_t rlim_cur;
    uint32_t rlim_max;
};

typedef struct {
    int32_t     val[2];
} __neutral_kernel_fsid_t;

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

struct neutral_linux_dirent_64 {
    uint64_t    d_ino;
    uint64_t    d_off;
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

#endif

#ifdef __cplusplus
}
#endif
