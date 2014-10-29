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
        default:
            fatal("unsupported fnctl command %d\n", cmd);
    }

    return res;
}
