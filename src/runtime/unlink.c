#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>

int unlink(const char *pathname)
{
    return syscall((long) SYS_unlink, (long) pathname);
}
