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

#endif

#ifdef __cplusplus
}
#endif
