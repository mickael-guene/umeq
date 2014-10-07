#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/select.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

struct data_64_internal {
    const sigset_t *ss;
    size_t ss_len;
};

struct data_32_internal {
    uint32_t ss;
    uint32_t ss_len;
};

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

    res = syscall(SYS_select, nfds, readfds_p?readfds:NULL, writefds_p?writefds:NULL,
                  exceptfds_p?exceptfds:NULL, timeout_p?&timeout:NULL);

    if (timeout_p) {
        timeout_guest->tv_sec = timeout.tv_sec;
        timeout_guest->tv_usec = timeout.tv_usec;
    };

	return res;
}

int pselect6_s3264(uint32_t nfds_p, uint32_t readfds_p, uint32_t writefds_p, uint32_t exceptfds_p, uint32_t timeout_p, uint32_t data_p)
{
    int res;
    int nfds = (int) nfds_p;
    fd_set *readfds = (fd_set *) g_2_h(readfds_p);
    fd_set *writefds = (fd_set *) g_2_h(writefds_p);
    fd_set *exceptfds = (fd_set *) g_2_h(exceptfds_p);
    struct timespec_32 *timeout_guest = (struct timespec_32 *) g_2_h(timeout_p);
    struct data_32_internal *data_guest = (struct data_32_internal *) g_2_h(data_p);
    struct timespec timeout;
    struct data_64_internal data;


    if (timeout_p) {
        timeout.tv_sec = timeout_guest->tv_sec;
        timeout.tv_nsec = timeout_guest->tv_nsec;
    };

    data.ss = (const sigset_t *) g_2_h(data_guest->ss);
    data.ss_len = data_guest->ss_len;
    res = syscall(SYS_pselect6, nfds, readfds_p?readfds:NULL, writefds_p?writefds:NULL,
                  exceptfds_p?exceptfds:NULL, timeout_p?&timeout:NULL, &data);

    if (timeout_p) {
        timeout_guest->tv_sec = timeout.tv_sec;
        timeout_guest->tv_nsec = timeout.tv_nsec;
    };

    return res;
}
