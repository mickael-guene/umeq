#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#include <unistd.h>
#include <sys/syscall.h>

ssize_t read(int fd, void *buf, size_t count)
{
    return syscall((long) SYS_read, (long) fd, (long) buf, (long) count);
}
