#include <unistd.h>
#include <fcntl.h>

#include <errno.h>

#include "runtime.h"
#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int fnctl64_s3264(uint32_t fd_p, uint32_t cmd_p, uint32_t opt_p)
{
    int res = -EINVAL;
    int fd = fd_p;
    int cmd = cmd_p;

    switch(cmd) {
        case F_GETFD:
            res = syscall(SYS_fcntl, fd, cmd);
            break;
        default:
            fatal("unsupported fnctl64 command %d\n", cmd);
    }

    return res;
}
