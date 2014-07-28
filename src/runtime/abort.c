#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>

void abort()
{
	syscall(SYS_exit, -1);
}
