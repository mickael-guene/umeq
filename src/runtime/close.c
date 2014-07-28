#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#include <unistd.h>
#include <sys/syscall.h>

int close(int fd)
{
    return syscall((long) SYS_close, (long) fd);
}
