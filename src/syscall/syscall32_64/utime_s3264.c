#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <utime.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int utimes_s3264(uint32_t filename_p, uint32_t times_p)
{
	int res;
	char *filename = (char *) g_2_h(filename_p);
	struct timeval_32 *times_guest = (struct timeval_32 *) g_2_h(times_p);
	struct timeval times[2];

    if (times_p) {
        times[0].tv_sec = times_guest[0].tv_sec;
        times[0].tv_usec = times_guest[0].tv_usec;
        times[1].tv_sec = times_guest[1].tv_sec;
        times[1].tv_usec = times_guest[1].tv_usec;
    }

    res = syscall(SYS_utimes, filename, times_p?times:NULL);

	return res;
}
