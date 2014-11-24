#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#include <unistd.h>
#include <fcntl.h>

#include <errno.h>

#include "runtime.h"
#include "syscall64_64_private.h"

/* FIXME: Found a way to handle this */
long arm64ToX86Flags(long arm64_flags);
long x86ToArm64Flags(long x86_flags);

long fcntl_s6464(uint64_t fd_p, uint64_t cmd_p, uint64_t opt_p)
{
    long res = -EINVAL;
    int fd = fd_p;
    int cmd = cmd_p;

    switch(cmd) {
        case F_DUPFD:
        case F_DUPFD_CLOEXEC:
            res = syscall(SYS_fcntl, fd, cmd, (int) opt_p);
            break;
        case F_GETFD:
            res = syscall(SYS_fcntl, fd, cmd);
            break;
        case F_SETFD:
            res = syscall(SYS_fcntl, fd, cmd, (int) opt_p);
            break;
        case F_GETFL:
            res = syscall(SYS_fcntl, fd, cmd);
            res = x86ToArm64Flags(res);
            break;
        case F_SETFL:
            res = syscall(SYS_fcntl, fd, cmd, arm64ToX86Flags(opt_p));
            break;
        case F_GETLK:
            res = syscall(SYS_fcntl, fd, cmd, (struct flock *) g_2_h(opt_p));
            break;
        case F_SETLK:
            res = syscall(SYS_fcntl, fd, cmd, (struct flock *) g_2_h(opt_p));
            break;
        case F_SETLKW:
            res = syscall(SYS_fcntl, fd, cmd, (struct flock *) g_2_h(opt_p));
            break;
        case F_SETSIG:
            res = syscall(SYS_fcntl, fd, cmd, (int) opt_p);
            break;
        case F_GETSIG:
            res = syscall(SYS_fcntl, fd, cmd);
            break;
        case F_SETOWN:
            res = syscall(SYS_fcntl, fd, cmd, (int) opt_p);
            break;
        case F_GETOWN_EX:
            res = syscall(SYS_fcntl, fd, cmd, (struct f_owner_ex *) g_2_h(opt_p));
            break;
        case F_SETLEASE:
            res = syscall(SYS_fcntl, fd, cmd, (int) opt_p);
            break;
        case F_GETLEASE:
            res = syscall(SYS_fcntl, fd, cmd);
            break;
        case F_GETPIPE_SZ:
            res = syscall(SYS_fcntl, fd, cmd);
            break;
        case F_SETPIPE_SZ:
            res = syscall(SYS_fcntl, fd, cmd, (int) opt_p);
            break;
        default:
            fprintf(stderr, "unsupported fnctl command %d\n", cmd);
            res = -EINVAL;
    }

    return res;
}
