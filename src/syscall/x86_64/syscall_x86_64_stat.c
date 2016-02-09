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
#include <sys/syscall.h>
#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "syscalls_neutral.h"
#include "syscalls_neutral_types.h"

long syscall_x86_64_fstat64(uint64_t fd_p, uint64_t buf_p)
{
    int fd = (int) fd_p;
    struct neutral_stat64 *neutral_stat64 = (struct neutral_stat64 *) buf_p;
    struct stat buf;
    int res;

    res = syscall(SYS_fstat, fd, &buf);
    if (!res) {
        neutral_stat64->st_dev = buf.st_dev;
        neutral_stat64->st_ino = buf.st_ino;
        neutral_stat64->st_mode = buf.st_mode;
        neutral_stat64->st_nlink = buf.st_nlink;
        neutral_stat64->st_uid = buf.st_uid;
        neutral_stat64->st_gid = buf.st_gid;
        neutral_stat64->st_rdev = buf.st_rdev;
        neutral_stat64->st_size = buf.st_size;
        neutral_stat64->st_blksize = buf.st_blksize;
        neutral_stat64->st_blocks = buf.st_blocks;
        neutral_stat64->st_atime_ = buf.st_atim.tv_sec;
        neutral_stat64->st_atime_nsec = buf.st_atim.tv_nsec;
        neutral_stat64->st_mtime_ = buf.st_mtim.tv_sec;
        neutral_stat64->st_mtime_nsec = buf.st_mtim.tv_nsec;
        neutral_stat64->st_ctime_ = buf.st_ctim.tv_sec;
        neutral_stat64->st_ctime_nsec = buf.st_ctim.tv_nsec;
    }

    return res;
}

long syscall_x86_64_lstat64(uint64_t pathname_p, uint64_t buf_p)
{
    const char *path = (const char *) pathname_p;
    struct neutral_stat64 *neutral_stat64 = (struct neutral_stat64 *) buf_p;
    struct stat buf;
    int res;

    res = syscall(SYS_lstat, path, &buf);
    if (!res) {
        neutral_stat64->st_dev = buf.st_dev;
        neutral_stat64->st_ino = buf.st_ino;
        neutral_stat64->st_mode = buf.st_mode;
        neutral_stat64->st_nlink = buf.st_nlink;
        neutral_stat64->st_uid = buf.st_uid;
        neutral_stat64->st_gid = buf.st_gid;
        neutral_stat64->st_rdev = buf.st_rdev;
        neutral_stat64->st_size = buf.st_size;
        neutral_stat64->st_blksize = buf.st_blksize;
        neutral_stat64->st_blocks = buf.st_blocks;
        neutral_stat64->st_atime_ = buf.st_atim.tv_sec;
        neutral_stat64->st_atime_nsec = buf.st_atim.tv_nsec;
        neutral_stat64->st_mtime_ = buf.st_mtim.tv_sec;
        neutral_stat64->st_mtime_nsec = buf.st_mtim.tv_nsec;
        neutral_stat64->st_ctime_ = buf.st_ctim.tv_sec;
        neutral_stat64->st_ctime_nsec = buf.st_ctim.tv_nsec;
    }

    return res;
}

long syscall_x86_64_stat64(uint64_t pathname_p, uint64_t buf_p)
{
    const char *path = (const char *) pathname_p;
    struct neutral_stat64 *neutral_stat64 = (struct neutral_stat64 *) buf_p;
    struct stat buf;
    int res;

    res = syscall(SYS_stat, path, &buf);
    if (!res) {
        neutral_stat64->st_dev = buf.st_dev;
        neutral_stat64->st_ino = buf.st_ino;
        neutral_stat64->st_mode = buf.st_mode;
        neutral_stat64->st_nlink = buf.st_nlink;
        neutral_stat64->st_uid = buf.st_uid;
        neutral_stat64->st_gid = buf.st_gid;
        neutral_stat64->st_rdev = buf.st_rdev;
        neutral_stat64->st_size = buf.st_size;
        neutral_stat64->st_blksize = buf.st_blksize;
        neutral_stat64->st_blocks = buf.st_blocks;
        neutral_stat64->st_atime_ = buf.st_atim.tv_sec;
        neutral_stat64->st_atime_nsec = buf.st_atim.tv_nsec;
        neutral_stat64->st_mtime_ = buf.st_mtim.tv_sec;
        neutral_stat64->st_mtime_nsec = buf.st_mtim.tv_nsec;
        neutral_stat64->st_ctime_ = buf.st_ctim.tv_sec;
        neutral_stat64->st_ctime_nsec = buf.st_ctim.tv_nsec;
    }

    return res;
}

long syscall_x86_64_fstatat64(uint64_t dirfd_p, uint64_t pathname_p, uint64_t buf_p, uint64_t flags_p)
{
    long res;
    int dirfd = (int) dirfd_p;
    const char *pathname = (const char *) pathname_p;
    struct neutral_stat64 *neutral_stat64 = (struct neutral_stat64 *) buf_p;
    int flags = (int) flags_p;
    struct stat buf;

    res = syscall(SYS_newfstatat, dirfd, pathname, &buf, flags);
    if (!res) {
        neutral_stat64->st_dev = buf.st_dev;
        neutral_stat64->st_ino = buf.st_ino;
        neutral_stat64->st_mode = buf.st_mode;
        neutral_stat64->st_nlink = buf.st_nlink;
        neutral_stat64->st_uid = buf.st_uid;
        neutral_stat64->st_gid = buf.st_gid;
        neutral_stat64->st_rdev = buf.st_rdev;
        neutral_stat64->st_size = buf.st_size;
        neutral_stat64->st_blksize = buf.st_blksize;
        neutral_stat64->st_blocks = buf.st_blocks;
        neutral_stat64->st_atime_ = buf.st_atim.tv_sec;
        neutral_stat64->st_atime_nsec = buf.st_atim.tv_nsec;
        neutral_stat64->st_mtime_ = buf.st_mtim.tv_sec;
        neutral_stat64->st_mtime_nsec = buf.st_mtim.tv_nsec;
        neutral_stat64->st_ctime_ = buf.st_ctim.tv_sec;
        neutral_stat64->st_ctime_nsec = buf.st_ctim.tv_nsec;
    }

    return res;
}
