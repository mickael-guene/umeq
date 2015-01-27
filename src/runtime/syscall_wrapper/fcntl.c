#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#include <unistd.h>
#include <sys/syscall.h>

#include <unistd.h>
#include <fcntl.h>

#include <stdarg.h>
#include <assert.h>

int fcntl(int fd, int cmd, ... /* arg */ )
{
    long res;
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

    return res;
}
