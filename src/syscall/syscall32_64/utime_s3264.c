#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <utime.h>
#include <fcntl.h>
#include <sys/stat.h>

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

int utimensat_s3264(uint32_t dirfd_p, uint32_t pathname_p, uint32_t times_p, uint32_t flags_p)
{
    int res;
    int dirfd = (int) dirfd_p;
    char * pathname = (char *) g_2_h(pathname_p);
    struct timespec_32 *times_guest = (struct timespec_32 *) g_2_h(times_p);
    int flags = (int) flags_p;
    struct timespec times[2];

    if (times_p) {
        times[0].tv_sec = times_guest[0].tv_sec;
        times[0].tv_nsec = times_guest[0].tv_nsec;
        times[1].tv_sec = times_guest[1].tv_sec;
        times[1].tv_nsec = times_guest[1].tv_nsec;
    }

    res = syscall(SYS_utimensat, dirfd, pathname, (times_p?times:NULL), flags);

    return res;
}
