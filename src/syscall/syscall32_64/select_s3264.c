#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/select.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int newselect_s3264(uint32_t nfds_p, uint32_t readfds_p, uint32_t writefds_p, uint32_t exceptfds_p, uint32_t timeout_p)
{
	int res;
	int nfds = (int) nfds_p;
	fd_set *readfds = (fd_set *) g_2_h(readfds_p);
	fd_set *writefds = (fd_set *) g_2_h(writefds_p);
	fd_set *exceptfds = (fd_set *) g_2_h(exceptfds_p);
	struct timeval_32 *timeout_guest = (struct timeval_32 *) g_2_h(timeout_p);
	struct timeval timeout;

    if (timeout_p) {
        timeout.tv_sec = timeout_guest->tv_sec;
        timeout.tv_usec = timeout_guest->tv_usec;
    };

    res = syscall(SYS_select, nfds, readfds, writefds, exceptfds, timeout_p?&timeout:NULL);

    if (timeout_p) {
        timeout_guest->tv_sec = timeout.tv_sec;
        timeout_guest->tv_usec = timeout.tv_usec;
    };

	return res;
}
