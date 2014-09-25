#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>

int kill(int pid, int sig)
{
    return syscall(SYS_kill, (long) pid, (long) sig);
}

int tkill(int pid, int sig)
{
    return syscall(SYS_tkill, (long) pid, (long) sig);
}

void abort()
{
	syscall(SYS_exit, -1);
}
