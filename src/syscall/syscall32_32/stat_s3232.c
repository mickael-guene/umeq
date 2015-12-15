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

#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/vfs.h>

#include <errno.h>

#include "syscall32_32_types.h"
#include "syscall32_32_private.h"

int stat64_s3232(uint32_t pathname_p, uint32_t buf_p)
{
    int res;
    const char *path = (const char *) g_2_h(pathname_p);
    struct stat32_32 *buf_g = (struct stat32_32 *) g_2_h(buf_p);
    struct stat64 buf;

    res = syscall(SYS_stat64, path, &buf);

    buf_g->st_dev = buf.st_dev;
    buf_g->__st_ino = buf.__st_ino;
    buf_g->st_mode = buf.st_mode;
    buf_g->st_nlink = buf.st_nlink;
    buf_g->st_uid = buf.st_uid;
    buf_g->st_gid = buf.st_gid;
    buf_g->st_rdev = buf.st_rdev;
    buf_g->st_size = buf.st_size;
    buf_g->st_blksize = buf.st_blksize;
    buf_g->st_blocks = buf.st_blocks;
    buf_g->_st_atime = buf.st_atim.tv_sec;
    buf_g->st_atime_nsec = buf.st_atim.tv_nsec;
    buf_g->_st_mtime = buf.st_mtim.tv_sec;
    buf_g->st_mtime_nsec = buf.st_mtim.tv_nsec;
    buf_g->_st_ctime = buf.st_ctim.tv_sec;
    buf_g->st_ctime_nsec = buf.st_ctim.tv_nsec;
    buf_g->st_ino = buf.st_ino;

    return res;

}

int fstat64_s3232(uint32_t fd_p, uint32_t buf_p)
{
    int res;
    int fd = fd_p;
    struct stat32_32 *buf_g = (struct stat32_32 *) g_2_h(buf_p);
    struct stat64 buf;

    res = syscall(SYS_fstat64, fd, &buf);

    buf_g->st_dev = buf.st_dev;
    buf_g->__st_ino = buf.__st_ino;
    buf_g->st_mode = buf.st_mode;
    buf_g->st_nlink = buf.st_nlink;
    buf_g->st_uid = buf.st_uid;
    buf_g->st_gid = buf.st_gid;
    buf_g->st_rdev = buf.st_rdev;
    buf_g->st_size = buf.st_size;
    buf_g->st_blksize = buf.st_blksize;
    buf_g->st_blocks = buf.st_blocks;
    buf_g->_st_atime = buf.st_atim.tv_sec;
    buf_g->st_atime_nsec = buf.st_atim.tv_nsec;
    buf_g->_st_mtime = buf.st_mtim.tv_sec;
    buf_g->st_mtime_nsec = buf.st_mtim.tv_nsec;
    buf_g->_st_ctime = buf.st_ctim.tv_sec;
    buf_g->st_ctime_nsec = buf.st_ctim.tv_nsec;
    buf_g->st_ino = buf.st_ino;

    return res;
}

int lstat64_s3232(uint32_t pathname_p, uint32_t buf_p)
{
    int res;
    const char *path = (const char *) g_2_h(pathname_p);
    struct stat32_32 *buf_g = (struct stat32_32 *) g_2_h(buf_p);
    struct stat64 buf;

    res = syscall(SYS_lstat64, path, &buf);

    buf_g->st_dev = buf.st_dev;
    buf_g->__st_ino = buf.__st_ino;
    buf_g->st_mode = buf.st_mode;
    buf_g->st_nlink = buf.st_nlink;
    buf_g->st_uid = buf.st_uid;
    buf_g->st_gid = buf.st_gid;
    buf_g->st_rdev = buf.st_rdev;
    buf_g->st_size = buf.st_size;
    buf_g->st_blksize = buf.st_blksize;
    buf_g->st_blocks = buf.st_blocks;
    buf_g->_st_atime = buf.st_atim.tv_sec;
    buf_g->st_atime_nsec = buf.st_atim.tv_nsec;
    buf_g->_st_mtime = buf.st_mtim.tv_sec;
    buf_g->st_mtime_nsec = buf.st_mtim.tv_nsec;
    buf_g->_st_ctime = buf.st_ctim.tv_sec;
    buf_g->st_ctime_nsec = buf.st_ctim.tv_nsec;
    buf_g->st_ino = buf.st_ino;

    return res;

}

int fstatat64_s3232(uint32_t dirfd_p, uint32_t pathname_p, uint32_t buf_p, uint32_t flags_p)
{
    int res;
    int dirfd = (int) dirfd_p;
    const char *pathname = (const char *) g_2_h(pathname_p);
    struct stat32_32 *buf_g = (struct stat32_32 *) g_2_h(buf_p);
    int flags = (int) flags_p;
    struct stat64 buf;

    res = syscall(SYS_fstatat64, dirfd, pathname, &buf, flags);

    buf_g->st_dev = buf.st_dev;
    buf_g->__st_ino = buf.__st_ino;
    buf_g->st_mode = buf.st_mode;
    buf_g->st_nlink = buf.st_nlink;
    buf_g->st_uid = buf.st_uid;
    buf_g->st_gid = buf.st_gid;
    buf_g->st_rdev = buf.st_rdev;
    buf_g->st_size = buf.st_size;
    buf_g->st_blksize = buf.st_blksize;
    buf_g->st_blocks = buf.st_blocks;
    buf_g->_st_atime = buf.st_atim.tv_sec;
    buf_g->st_atime_nsec = buf.st_atim.tv_nsec;
    buf_g->_st_mtime = buf.st_mtim.tv_sec;
    buf_g->st_mtime_nsec = buf.st_mtim.tv_nsec;
    buf_g->_st_ctime = buf.st_ctim.tv_sec;
    buf_g->st_ctime_nsec = buf.st_ctim.tv_nsec;
    buf_g->st_ino = buf.st_ino;

    return res;
}
