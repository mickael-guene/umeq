#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#include <stdlib.h>

void exit(int status)
{
	syscall(SYS_exit, status);
    abort();
}
