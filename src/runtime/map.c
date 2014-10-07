#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mman.h>

void *mmap(void *addr, size_t length, int prot, int flags, int fd, off_t offset)
{
    return (void *) syscall((long) SYS_mmap, (long) addr, (long) length, (long) prot, (long) flags, (long) fd, (long) offset);
}

int munmap(void *addr, size_t length)
{
    return syscall((long) SYS_munmap, (long) addr, (long) length);
}

int mprotect(void *addr, size_t len, int prot)
{
    return syscall((long) SYS_mprotect, (long) addr, (long) len, (long) prot);
}
