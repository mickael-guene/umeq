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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/vfs.h>

#include <errno.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int stat64_s3264(uint32_t pathname_p, uint32_t buf_p)
{
    int res;
    const char *path = (const char *) g_2_h(pathname_p);
    struct stat64_32 *buf_g = (struct stat64_32 *) g_2_h(buf_p);
    struct stat buf;

    res = syscall(SYS_stat, path, &buf);

    buf_g->st_dev = buf.st_dev;
    buf_g->__st_ino = buf.st_ino;
    buf_g->st_mode = buf.st_mode;
    buf_g->st_nlink = buf.st_nlink;
    buf_g->st_uid = buf.st_uid;
    buf_g->st_gid = buf.st_gid;
    buf_g->st_rdev = buf.st_rdev;
    buf_g->st_size = buf.st_size;
    buf_g->st_blksize = buf.st_blksize;
    buf_g->st_blocks = buf.st_blocks;
    buf_g->_st_atime = buf.st_atime;
    buf_g->st_atime_nsec = 0;
    buf_g->_st_mtime = buf.st_mtime;
    buf_g->st_mtime_nsec = 0;
    buf_g->_st_ctime = buf.st_ctime;
    buf_g->st_ctime_nsec = 0;
    buf_g->st_ino = buf.st_ino;

    return res;

}

int fstat64_s3264(uint32_t fd_p, uint32_t buf_p)
{
    int res;
    int fd = fd_p;
    struct stat64_32 *buf_g = (struct stat64_32 *) g_2_h(buf_p);
    struct stat buf;

    res = syscall(SYS_fstat, fd, &buf);

    buf_g->st_dev = buf.st_dev;
    buf_g->__st_ino = buf.st_ino;
    buf_g->st_mode = buf.st_mode;
    buf_g->st_nlink = buf.st_nlink;
    buf_g->st_uid = buf.st_uid;
    buf_g->st_gid = buf.st_gid;
    buf_g->st_rdev = buf.st_rdev;
    buf_g->st_size = buf.st_size;
    buf_g->st_blksize = buf.st_blksize;
    buf_g->st_blocks = buf.st_blocks;
    buf_g->_st_atime = buf.st_atime;
    buf_g->st_atime_nsec = 0;
    buf_g->_st_mtime = buf.st_mtime;
    buf_g->st_mtime_nsec = 0;
    buf_g->_st_ctime = buf.st_ctime;
    buf_g->st_ctime_nsec = 0;
    buf_g->st_ino = buf.st_ino;

    return res;
}

int fstatat64_s3264(uint32_t dirfd_p, uint32_t pathname_p, uint32_t buf_p, uint32_t flags_p)
{
    int res;
    int dirfd = (int) dirfd_p;
    const char *pathname = (const char *) g_2_h(pathname_p);
    struct stat64_32 *buf_g = (struct stat64_32 *) g_2_h(buf_p);
    int flags = (int) flags_p;
    struct stat buf;

    //do host syscall
    res = syscall(SYS_newfstatat, dirfd, pathname, &buf, flags);

    buf_g->st_dev = buf.st_dev;
    buf_g->__st_ino = buf.st_ino;
    buf_g->st_mode = buf.st_mode;
    buf_g->st_nlink = buf.st_nlink;
    buf_g->st_uid = buf.st_uid;
    buf_g->st_gid = buf.st_gid;
    buf_g->st_rdev = buf.st_rdev;
    buf_g->st_size = buf.st_size;
    buf_g->st_blksize = buf.st_blksize;
    buf_g->st_blocks = buf.st_blocks;
    buf_g->_st_atime = buf.st_atime;
    buf_g->st_atime_nsec = 0;
    buf_g->_st_mtime = buf.st_mtime;
    buf_g->st_mtime_nsec = 0;
    buf_g->_st_ctime = buf.st_ctime;
    buf_g->st_ctime_nsec = 0;
    buf_g->st_ino = buf.st_ino;

    return res;
}

int lstat64_s3264(uint32_t pathname_p, uint32_t buf_p)
{
    int res;
    const char *path = (const char *) g_2_h(pathname_p);
    struct stat64_32 *buf_g = (struct stat64_32 *) g_2_h(buf_p);
    struct stat buf;

    res = syscall(SYS_lstat, path, &buf);

    buf_g->st_dev = buf.st_dev;
    buf_g->__st_ino = buf.st_ino;
    buf_g->st_mode = buf.st_mode;
    buf_g->st_nlink = buf.st_nlink;
    buf_g->st_uid = buf.st_uid;
    buf_g->st_gid = buf.st_gid;
    buf_g->st_rdev = buf.st_rdev;
    buf_g->st_size = buf.st_size;
    buf_g->st_blksize = buf.st_blksize;
    buf_g->st_blocks = buf.st_blocks;
    buf_g->_st_atime = buf.st_atime;
    buf_g->st_atime_nsec = 0;
    buf_g->_st_mtime = buf.st_mtime;
    buf_g->st_mtime_nsec = 0;
    buf_g->_st_ctime = buf.st_ctime;
    buf_g->st_ctime_nsec = 0;
    buf_g->st_ino = buf.st_ino;

    return res;

}

/* FIXME: need to check params + buf is really a statfs_32 ?*/
int statfs64_s3264(uint32_t path_p, uint32_t dummy_p, uint32_t buf_p)
{
    const char *path = (const char *) g_2_h(path_p);
    struct statfs_32 *buf_guest = (struct statfs_32 *) g_2_h(buf_p);
    struct statfs buf;
    int res;

    if (buf_p == 0 || buf_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall(SYS_statfs, path, &buf);

        buf_guest->f_type = buf.f_type;
        buf_guest->f_bsize = buf.f_bsize;
        buf_guest->f_blocks = buf.f_blocks;
        buf_guest->f_bfree = buf.f_bfree;
        buf_guest->f_bavail = buf.f_bavail;
        buf_guest->f_files = buf.f_files;
        buf_guest->f_ffree = buf.f_ffree;
        buf_guest->f_fsid.val[0] = 0;
        buf_guest->f_fsid.val[1] = 0;
        buf_guest->f_namelen = buf.f_namelen;
        buf_guest->f_frsize = buf.f_frsize;
        buf_guest->f_flags = 0;
    }

    return res;
}

int statfs_s3264(uint32_t path_p, uint32_t buf_p)
{
    const char *path = (const char *) g_2_h(path_p);
    struct statfs_32 *buf_guest = (struct statfs_32 *) g_2_h(buf_p);
    struct statfs buf;
    int res;

     if (buf_p == 0 || buf_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall(SYS_statfs, path, &buf);

        buf_guest->f_type = buf.f_type;
        buf_guest->f_bsize = buf.f_bsize;
        buf_guest->f_blocks = buf.f_blocks;
        buf_guest->f_bfree = buf.f_bfree;
        buf_guest->f_bavail = buf.f_bavail;
        buf_guest->f_files = buf.f_files;
        buf_guest->f_ffree = buf.f_ffree;
        buf_guest->f_fsid.val[0] = 0;
        buf_guest->f_fsid.val[1] = 0;
        buf_guest->f_namelen = buf.f_namelen;
        buf_guest->f_frsize = buf.f_frsize;
        buf_guest->f_flags = 0;
    }

    return res;
}

int fstatfs64_s3264(uint32_t fd_p, uint32_t dummy_p, uint32_t buf_p)
{
    int fd = (int) fd_p;
    struct statfs_32 *buf_guest = (struct statfs_32 *) g_2_h(buf_p);
    struct statfs buf;
    int res;

    if (buf_p == 0 || buf_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall(SYS_fstatfs, fd, &buf);

        buf_guest->f_type = buf.f_type;
        buf_guest->f_bsize = buf.f_bsize;
        buf_guest->f_blocks = buf.f_blocks;
        buf_guest->f_bfree = buf.f_bfree;
        buf_guest->f_bavail = buf.f_bavail;
        buf_guest->f_files = buf.f_files;
        buf_guest->f_ffree = buf.f_ffree;
        buf_guest->f_fsid.val[0] = 0;
        buf_guest->f_fsid.val[1] = 0;
        buf_guest->f_namelen = buf.f_namelen;
        buf_guest->f_frsize = buf.f_frsize;
        buf_guest->f_flags = 0;
    }

    return res;
}

int fstatfs_s3264(uint32_t fd_p, uint32_t buf_p)
{
    int fd = (int) fd_p;
    struct statfs_32 *buf_guest = (struct statfs_32 *) g_2_h(buf_p);
    struct statfs buf;
    int res;

    if (buf_p == 0 || buf_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall(SYS_fstatfs,(long) fd, (long) &buf);

        buf_guest->f_type = buf.f_type;
        buf_guest->f_bsize = buf.f_bsize;
        buf_guest->f_blocks = buf.f_blocks;
        buf_guest->f_bfree = buf.f_bfree;
        buf_guest->f_bavail = buf.f_bavail;
        buf_guest->f_files = buf.f_files;
        buf_guest->f_ffree = buf.f_ffree;
        buf_guest->f_fsid.val[0] = 0;
        buf_guest->f_fsid.val[1] = 0;
        buf_guest->f_namelen = buf.f_namelen;
        buf_guest->f_frsize = buf.f_frsize;
        buf_guest->f_flags = 0;
    }

    return res;
}
