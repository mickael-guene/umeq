#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

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
