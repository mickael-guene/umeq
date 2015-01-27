#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#include <unistd.h>
#include <sys/syscall.h>

#include <sys/mman.h>

int munmap(void *addr, size_t length)
{
    return syscall((long) SYS_munmap, (long) addr, (long) length);
}

int mprotect(void *addr, size_t len, int prot)
{
    return syscall((long) SYS_mprotect, (long) addr, (long) len, (long) prot);
}
