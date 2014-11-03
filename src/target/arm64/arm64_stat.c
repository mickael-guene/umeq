#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "arm64_private.h"
#include "arm64_syscall.h"
#include "arm64_syscall_types.h"

long arm64_fstat(struct arm64_target *context)
{
    long res;
    int fd = (int) context->regs.r[0];
    struct stat_arm64 *buf_guest = (struct stat_arm64 *) g_2_h(context->regs.r[1]);
    struct stat buf;

    res = syscall(SYS_fstat, fd, &buf);
    buf_guest->st_dev = buf.st_dev;
    buf_guest->st_ino = buf.st_ino;
    buf_guest->st_mode = buf.st_mode;
    buf_guest->st_nlink = buf.st_nlink;
    buf_guest->st_uid = buf.st_uid;
    buf_guest->st_gid = buf.st_gid;
    buf_guest->st_rdev = buf.st_rdev;
    buf_guest->st_size = buf.st_size;
    buf_guest->st_blksize = buf.st_blksize;
    buf_guest->st_blocks = buf.st_blocks;
    buf_guest->st_atim = buf.st_atim;
    buf_guest->st_mtim = buf.st_mtim;
    buf_guest->st_ctim = buf.st_ctim;

    return res;
}

long arm64_fstatat64(struct arm64_target *context)
{
    long res;
    int dirfd = (int) context->regs.r[0];
    char *pathname = (char *) g_2_h(context->regs.r[1]);
    struct stat_arm64 *buf_guest = (struct stat_arm64 *) g_2_h(context->regs.r[2]);
    int flags = (int) context->regs.r[3];
    struct stat buf;

    res = syscall(SYS_newfstatat, dirfd, pathname, &buf, flags);
    buf_guest->st_dev = buf.st_dev;
    buf_guest->st_ino = buf.st_ino;
    buf_guest->st_mode = buf.st_mode;
    buf_guest->st_nlink = buf.st_nlink;
    buf_guest->st_uid = buf.st_uid;
    buf_guest->st_gid = buf.st_gid;
    buf_guest->st_rdev = buf.st_rdev;
    buf_guest->st_size = buf.st_size;
    buf_guest->st_blksize = buf.st_blksize;
    buf_guest->st_blocks = buf.st_blocks;
    buf_guest->st_atim = buf.st_atim;
    buf_guest->st_mtim = buf.st_mtim;
    buf_guest->st_ctim = buf.st_ctim;

    return res;
}
