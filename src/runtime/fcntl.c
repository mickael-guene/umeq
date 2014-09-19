#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <stdarg.h>
#include <fcntl.h>
#include <assert.h>

int fcntl(int fd, int cmd, ... /* arg */ )
{
    int res;
    va_list args;

    va_start(args, cmd);
    switch(cmd) {
        case F_SETFL:
            {
                long p0 = va_arg(args, long);
                res = syscall((long) SYS_fcntl, (long) fd, (long) cmd, (long) p0);
            }
            break;
        default:
        assert(0);
    }
    va_end(args);
    if (res < 0) {
        errno = -res;
        res = -1;
    }

    return res;
}
