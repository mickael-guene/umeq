#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

int open(__const char *__file, int __oflag, ...)
{
    int status;

    if (__oflag & O_CREAT) {
        mode_t mode;
        va_list ap;

        va_start(ap, __oflag);
        mode = va_arg(ap, mode_t);
        status = syscall((long) SYS_open, (long) __file, (long) __oflag, (long) mode);
        va_end(ap);
    } else {
        // TODO : do I have to provide a dummy mode ?
        status = syscall((long) SYS_open, (long) __file, (long) __oflag);
    }

    return status;
}
