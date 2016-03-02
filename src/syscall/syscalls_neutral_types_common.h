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

#ifndef __SYSCALLS_NEUTRAL_TYPES_COMMON__
#define __SYSCALLS_NEUTRAL_TYPES_COMMON__ 1

#define NEUTRAL_FUTEX_WAKE                  1
#define NEUTRAL_FUTEX_REQUEUE               3
#define NEUTRAL_FUTEX_CMP_REQUEUE           4
#define NEUTRAL_FUTEX_WAKE_OP               5
#define NEUTRAL_FUTEX_CMP_REQUEUE_PI        12
#define NEUTRAL_FUTEX_PRIVATE_FLAG          128
#define NEUTRAL_FUTEX_CLOCK_REALTIME        256
#define NEUTRAL_FUTEX_CMD_MASK              ~(NEUTRAL_FUTEX_PRIVATE_FLAG | NEUTRAL_FUTEX_CLOCK_REALTIME)

#define NEUTRAL_F_DUPFD         0
#define NEUTRAL_F_GETFD         1
#define NEUTRAL_F_SETFD         2
#define NEUTRAL_F_GETFL         3
#define NEUTRAL_F_SETFL         4
#define NEUTRAL_F_GETLK         5
#define NEUTRAL_F_SETLK         6
#define NEUTRAL_F_SETLKW        7
#define NEUTRAL_F_SETOWN        8
#define NEUTRAL_F_GETOWN        9
#define NEUTRAL_F_SETSIG        10
#define NEUTRAL_F_GETSIG        11
#define NEUTRAL_F_GETLK64       12
#define NEUTRAL_F_SETLK64       13
#define NEUTRAL_F_SETLKW64      14
#define NEUTRAL_F_SETOWN_EX     15
#define NEUTRAL_F_GETOWN_EX     16
#define NEUTRAL_F_SETLEASE      1024
#define NEUTRAL_F_GETLEASE      1025
#define NEUTRAL_F_DUPFD_CLOEXEC 1030
#define NEUTRAL_F_SETPIPE_SZ    1031
#define NEUTRAL_F_GETPIPE_SZ    1032

#define NEUTRAL_PR_GET_PDEATHSIG    2

#define NEUTRAL_KEYCTL_GET_KEYRING_ID       0
#define NEUTRAL_KEYCTL_REVOKE               3
#define NEUTRAL_KEYCTL_READ                 11

#define NEUTRAL_SETVAL          16

#define NEUTRAL_SIGEV_SIGNAL        0
#define NEUTRAL_SIGEV_NONE          1
#define NEUTRAL_SIGEV_THREAD        2

#define NEUTRAL__NSIG               64

#define NEUTRAL_IPC_RMID            0
#define NEUTRAL_IPC_SET             1
#define NEUTRAL_IPC_STAT            2
#define NEUTRAL_IPC_INFO            3

#define NEUTRAL_SHM_LOCK            11
#define NEUTRAL_SHM_UNLOCK          12
#define NEUTRAL_SHM_STAT            13
#define NEUTRAL_SHM_INFO            14
#define NEUTRAL_SEM_STAT            18
#define NEUTRAL_SEM_INFO            19

#define NEUTRAL_GETPID              11
#define NEUTRAL_GETVAL              12
#define NEUTRAL_GETALL              13
#define NEUTRAL_GETNCNT             14
#define NEUTRAL_GETZCNT             15
#define NEUTRAL_SETALL              17

#define NEUTRAL_MSG_STAT            11
#define NEUTRAL_MSG_INFO            12

typedef int32_t             neutral_mqd_t;

typedef int32_t             neutral_key_serial_t;

typedef struct {
    int32_t     val[2];
} __neutral_kernel_fsid_t;

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

struct neutral_epoll_event {
    uint32_t events;
    uint32_t __pad0;
    uint64_t data;
};

struct neutral_timezone {
    int32_t tz_minuteswest;
    int32_t tz_dsttime;
};

struct neutral_pollfd {
    int32_t fd;
    int16_t events;
    int16_t revents;
};

typedef struct {
    uint64_t fds_bits[1024 / 64];
} neutral_fd_set;

struct neutral_new_utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

#endif

#ifdef __cplusplus
}
#endif
