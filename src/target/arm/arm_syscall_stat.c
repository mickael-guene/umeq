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

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <sys/stat.h>

#include "arm_private.h"
#include "arm_helpers.h"
#include "sysnums-arm.h"
#include "hownums-arm.h"
#include "syscalls_neutral.h"
#include "syscalls_neutral_types.h"
#include "runtime.h"
#include "arm_syscall.h"
#include "cache.h"
#include "umeq.h"

//#define STAT64_HAS_BROKEN_ST_INO        1
struct arm_stat64 {
    uint64_t            st_dev;
    uint8_t             __pad0[4];
    uint32_t            __st_ino;
    uint32_t            st_mode;
    uint32_t            st_nlink;
    uint32_t            st_uid;
    uint32_t            st_gid;
    uint64_t            st_rdev;
    uint8_t             __pad3[4];
    uint32_t            __align_pad_1;
    int64_t             st_size;
    uint32_t            st_blksize;
    uint32_t            __align_pad_2;
    uint64_t            st_blocks;   /* Number 512-byte blocks allocated. */
    uint32_t            st_atime_;
    uint32_t            st_atime_nsec;
    uint32_t            st_mtime_;
    uint32_t            st_mtime_nsec;
    uint32_t            st_ctime_;
    uint32_t            st_ctime_nsec;
    uint64_t            st_ino;
};

struct arm_stat {
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
    uint32_t st_atime_;
    uint32_t st_atime_nsec;
    uint32_t st_mtime_;
    uint32_t st_mtime_nsec;
    uint32_t st_ctime_;
    uint32_t st_ctime_nsec;
    uint32_t __unused4;
    uint32_t __unused5;
};

static void host_2_guest_stat(struct stat *statbuf, uint32_t statbuf_p)
{
    struct arm_stat *statbuf_guest = (struct arm_stat *) g_2_h(statbuf_p);

    statbuf_guest->st_dev = statbuf->st_dev;
    statbuf_guest->st_ino = statbuf->st_ino;
    statbuf_guest->st_mode = statbuf->st_mode;
    statbuf_guest->st_nlink = statbuf->st_nlink;
    statbuf_guest->st_uid = statbuf->st_uid;
    statbuf_guest->st_gid = statbuf->st_gid;
    statbuf_guest->st_rdev = statbuf->st_rdev;
    statbuf_guest->st_size = statbuf->st_size;
    statbuf_guest->st_blksize = statbuf->st_blksize;
    statbuf_guest->st_blocks = statbuf->st_blocks;
    statbuf_guest->st_atime_ = statbuf->st_atim.tv_sec;
    statbuf_guest->st_atime_nsec = statbuf->st_atim.tv_nsec;
    statbuf_guest->st_mtime_ = statbuf->st_mtim.tv_sec;
    statbuf_guest->st_mtime_nsec = statbuf->st_mtim.tv_nsec;
    statbuf_guest->st_ctime_ = statbuf->st_ctim.tv_sec;
    statbuf_guest->st_ctime_nsec = statbuf->st_ctim.tv_nsec;
}

/* we can call syscall_adapter_guest32 since we have enought memory in buf for struct neutral_stat64 */
int arm_fstat64(struct arm_target *context)
{
    int res;

    if (!context->regs.r[1])
        return -EFAULT;

    res = syscall_adapter_guest32(PR_fstat64, context->regs.r[0], context->regs.r[1], 0, 0, 0, 0);
    if (!res) {
        struct neutral_stat64 neutral_stat64;
        struct arm_stat64 *arm_stat64 = (struct arm_stat64 *) g_2_h(context->regs.r[1]);

        memcpy(&neutral_stat64, g_2_h(context->regs.r[1]), sizeof(neutral_stat64));
        arm_stat64->st_dev = neutral_stat64.st_dev;
        arm_stat64->__st_ino = neutral_stat64.st_ino;
        arm_stat64->st_mode = neutral_stat64.st_mode;
        arm_stat64->st_nlink = neutral_stat64.st_nlink;
        arm_stat64->st_uid = neutral_stat64.st_uid;
        arm_stat64->st_gid = neutral_stat64.st_gid;
        arm_stat64->st_rdev = neutral_stat64.st_rdev;
        arm_stat64->st_size = neutral_stat64.st_size;
        arm_stat64->st_blksize = neutral_stat64.st_blksize;
        arm_stat64->st_blocks = neutral_stat64.st_blocks;
        arm_stat64->st_atime_ = neutral_stat64.st_atime_;
        arm_stat64->st_atime_nsec = neutral_stat64.st_atime_nsec;
        arm_stat64->st_mtime_ = neutral_stat64.st_mtime_;
        arm_stat64->st_mtime_nsec = neutral_stat64.st_mtime_nsec;
        arm_stat64->st_ctime_ = neutral_stat64.st_ctime_;
        arm_stat64->st_ctime_nsec = neutral_stat64.st_ctime_nsec;
        arm_stat64->st_ino = neutral_stat64.st_ino;
    }

    return res;
}

int arm_fstatat64(struct arm_target *context)
{
    int res;

    if (!context->regs.r[2])
        return -EFAULT;

    res = syscall_adapter_guest32(PR_fstatat64, context->regs.r[0], context->regs.r[1], context->regs.r[2], context->regs.r[3], 0, 0);
    if (!res) {
        struct neutral_stat64 neutral_stat64;
        struct arm_stat64 *arm_stat64 = (struct arm_stat64 *) g_2_h(context->regs.r[2]);

        memcpy(&neutral_stat64, g_2_h(context->regs.r[2]), sizeof(neutral_stat64));
        arm_stat64->st_dev = neutral_stat64.st_dev;
        arm_stat64->__st_ino = neutral_stat64.st_ino;
        arm_stat64->st_mode = neutral_stat64.st_mode;
        arm_stat64->st_nlink = neutral_stat64.st_nlink;
        arm_stat64->st_uid = neutral_stat64.st_uid;
        arm_stat64->st_gid = neutral_stat64.st_gid;
        arm_stat64->st_rdev = neutral_stat64.st_rdev;
        arm_stat64->st_size = neutral_stat64.st_size;
        arm_stat64->st_blksize = neutral_stat64.st_blksize;
        arm_stat64->st_blocks = neutral_stat64.st_blocks;
        arm_stat64->st_atime_ = neutral_stat64.st_atime_;
        arm_stat64->st_atime_nsec = neutral_stat64.st_atime_nsec;
        arm_stat64->st_mtime_ = neutral_stat64.st_mtime_;
        arm_stat64->st_mtime_nsec = neutral_stat64.st_mtime_nsec;
        arm_stat64->st_ctime_ = neutral_stat64.st_ctime_;
        arm_stat64->st_ctime_nsec = neutral_stat64.st_ctime_nsec;
        arm_stat64->st_ino = neutral_stat64.st_ino;
    }

    return res;
}

int arm_stat64(struct arm_target *context)
{
    int res;

    if (!context->regs.r[1])
        return -EFAULT;

    res = syscall_adapter_guest32(PR_stat64, context->regs.r[0], context->regs.r[1], 0, 0, 0, 0);
    if (!res) {
        struct neutral_stat64 neutral_stat64;
        struct arm_stat64 *arm_stat64 = (struct arm_stat64 *) g_2_h(context->regs.r[1]);

        memcpy(&neutral_stat64, g_2_h(context->regs.r[1]), sizeof(neutral_stat64));
        arm_stat64->st_dev = neutral_stat64.st_dev;
        arm_stat64->__st_ino = neutral_stat64.st_ino;
        arm_stat64->st_mode = neutral_stat64.st_mode;
        arm_stat64->st_nlink = neutral_stat64.st_nlink;
        arm_stat64->st_uid = neutral_stat64.st_uid;
        arm_stat64->st_gid = neutral_stat64.st_gid;
        arm_stat64->st_rdev = neutral_stat64.st_rdev;
        arm_stat64->st_size = neutral_stat64.st_size;
        arm_stat64->st_blksize = neutral_stat64.st_blksize;
        arm_stat64->st_blocks = neutral_stat64.st_blocks;
        arm_stat64->st_atime_ = neutral_stat64.st_atime_;
        arm_stat64->st_atime_nsec = neutral_stat64.st_atime_nsec;
        arm_stat64->st_mtime_ = neutral_stat64.st_mtime_;
        arm_stat64->st_mtime_nsec = neutral_stat64.st_mtime_nsec;
        arm_stat64->st_ctime_ = neutral_stat64.st_ctime_;
        arm_stat64->st_ctime_nsec = neutral_stat64.st_ctime_nsec;
        arm_stat64->st_ino = neutral_stat64.st_ino;
    }

    return res;
}

int arm_lstat64(struct arm_target *context)
{
    int res;

    if (!context->regs.r[1])
        return -EFAULT;

    res = syscall_adapter_guest32(PR_lstat64, context->regs.r[0], context->regs.r[1], 0, 0, 0, 0);
    if (!res) {
        struct neutral_stat64 neutral_stat64;
        struct arm_stat64 *arm_stat64 = (struct arm_stat64 *) g_2_h(context->regs.r[1]);

        memcpy(&neutral_stat64, g_2_h(context->regs.r[1]), sizeof(neutral_stat64));
        arm_stat64->st_dev = neutral_stat64.st_dev;
        arm_stat64->__st_ino = neutral_stat64.st_ino;
        arm_stat64->st_mode = neutral_stat64.st_mode;
        arm_stat64->st_nlink = neutral_stat64.st_nlink;
        arm_stat64->st_uid = neutral_stat64.st_uid;
        arm_stat64->st_gid = neutral_stat64.st_gid;
        arm_stat64->st_rdev = neutral_stat64.st_rdev;
        arm_stat64->st_size = neutral_stat64.st_size;
        arm_stat64->st_blksize = neutral_stat64.st_blksize;
        arm_stat64->st_blocks = neutral_stat64.st_blocks;
        arm_stat64->st_atime_ = neutral_stat64.st_atime_;
        arm_stat64->st_atime_nsec = neutral_stat64.st_atime_nsec;
        arm_stat64->st_mtime_ = neutral_stat64.st_mtime_;
        arm_stat64->st_mtime_nsec = neutral_stat64.st_mtime_nsec;
        arm_stat64->st_ctime_ = neutral_stat64.st_ctime_;
        arm_stat64->st_ctime_nsec = neutral_stat64.st_ctime_nsec;
        arm_stat64->st_ino = neutral_stat64.st_ino;
    }

    return res;
}

/* we don't use neutral syscall because guest statbuf is smaller that neutral one.
   So we can't use guest buffer to hold neutral content one. So only portable solutions
   are :
   - use mmmap_guest to allocate neutral statbuf and so we can pass a guest pointer to
   neutral syscall.
   - use per context mmmap_guest allocated area to handle such sycall case.
   I choose direct host syscall call since it's easy in this case and should be quite
   portable.
 */
int arm_fstat(struct arm_target *context)
{
    long res;
    struct stat statbuf;

    if (!context->regs.r[1])
        return -EFAULT;

    res = syscall(SYS_fstat, context->regs.r[0], &statbuf);
    if (!res) {
        host_2_guest_stat(&statbuf, context->regs.r[1]);
    }

    return res;
}

int arm_stat(struct arm_target *context)
{
    long res;
    struct stat statbuf;

    if (!context->regs.r[1])
        return -EFAULT;

    res = syscall(SYS_fstat, g_2_h(context->regs.r[0]), &statbuf);
    if (!res) {
        host_2_guest_stat(&statbuf, context->regs.r[1]);
    }

    return res;
}
