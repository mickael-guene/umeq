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
    struct stat_arm64 *stat_guest = (struct stat_arm64 *) g_2_h_64(context->regs.r[1]);
    struct stat buf;

    res = syscall(SYS_fstat, fd, &buf);
    stat_guest->st_dev = buf.st_dev;
    stat_guest->st_ino = buf.st_ino;
    stat_guest->st_mode = buf.st_mode;
    stat_guest->st_nlink = buf.st_nlink;
    stat_guest->st_uid = buf.st_uid;
    stat_guest->st_gid = buf.st_gid;
    stat_guest->st_rdev = buf.st_rdev;
    stat_guest->st_size = buf.st_size;
    stat_guest->st_blksize = buf.st_blksize;
    stat_guest->st_blocks = buf.st_blocks;
    stat_guest->st_atim = buf.st_atim;
    stat_guest->st_mtim = buf.st_mtim;
    stat_guest->st_ctim = buf.st_ctim;

    return res;
}
