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

#ifndef __SYSCALL32_32_TYPES__
#define __SYSCALL32_32_TYPES__ 1

#include <sys/syscall.h>
#include <sys/types.h>
#include <stdint.h>

struct stat32_32 {
    uint64_t    st_dev;
    uint8_t   __pad0[4];

    uint32_t    __st_ino;
    uint32_t    st_mode;
    uint32_t    st_nlink;

    uint32_t    st_uid;
    uint32_t    st_gid;

    uint64_t    st_rdev;
    uint8_t   __pad3[4];
    uint8_t   __pad4[4];

    int64_t     st_size;
    uint32_t    st_blksize;
    uint8_t   __pad5[4];
    uint64_t    st_blocks;

    uint32_t    _st_atime;
    uint32_t    st_atime_nsec;

    uint32_t    _st_mtime;
    uint32_t    st_mtime_nsec;

    uint32_t    _st_ctime;
    uint32_t    st_ctime_nsec;

    uint64_t    st_ino;
};

#endif

#ifdef __cplusplus
}
#endif
