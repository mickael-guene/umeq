#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>

int getrlimit(__rlimit_resource_t resource, struct rlimit *rlim)
{
    long syscall_res;

    syscall_res = syscall(SYS_getrlimit, (long)resource, (long) rlim);
    if (syscall_res < 0) {
        errno = -syscall_res;
        syscall_res = -1;
    }

    return syscall_res;
}

int setrlimit(__rlimit_resource_t resource, const struct rlimit *rlim)
{
    long syscall_res;

    syscall_res = syscall(SYS_setrlimit, (long)resource, (long) rlim);
    if (syscall_res < 0) {
        errno = -syscall_res;
        syscall_res = -1;
    }

    return syscall_res;
}
