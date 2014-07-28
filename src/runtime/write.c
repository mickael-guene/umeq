#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#include <unistd.h>
#include <sys/syscall.h>

ssize_t write(int fd, const void *buf, size_t count)
{
    return syscall((long) SYS_write, (long) fd, (long) buf, (long) count);
}
