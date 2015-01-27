#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#include <unistd.h>
#include <sys/syscall.h>

#include <sys/time.h>
#include <sys/resource.h>

int getrlimit(__rlimit_resource_t resource, struct rlimit *rlim)
{
    return  syscall(SYS_getrlimit, (long)resource, (long) rlim);
}

int setrlimit(__rlimit_resource_t resource, const struct rlimit *rlim)
{
    return  syscall(SYS_setrlimit, (long)resource, (long) rlim);
}
