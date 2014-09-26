#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <time.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int clock_getres_s3264(uint32_t clk_id_p, uint32_t res_p)
{
	int result;
	clockid_t clk_id = (clockid_t) clk_id_p;
	struct timespec_32 *res_guest = (struct timespec_32 *) g_2_h(res_p);
	struct timespec res;

	result = syscall(SYS_clock_getres, clk_id, res_p?&res:NULL);
    if (res_p) {
        res_guest->tv_sec = res.tv_sec;
        res_guest->tv_nsec = res.tv_nsec;
    }

	return result;
}

int clock_gettime_s3264(uint32_t clk_id_p, uint32_t tp_p)
{
	int res;
	clockid_t clk_id = (clockid_t) clk_id_p;
	struct timespec_32 *tp_guest = (struct timespec_32 *) g_2_h(tp_p);
	struct timespec tp;

	res = syscall(SYS_clock_gettime, clk_id, &tp);
	
	tp_guest->tv_sec = tp.tv_sec;
    tp_guest->tv_nsec = tp.tv_nsec;

	return res;
}

int nanosleep_s3264(uint32_t req_p, uint32_t rem_p)
{
	struct timespec_32 *req_guest = (struct timespec_32 *) g_2_h(req_p);
	struct timespec_32 *rem_guest = (struct timespec_32 *) g_2_h(rem_p);
	struct timespec req;
	struct timespec rem;
	int res;

	req.tv_sec = req_guest->tv_sec;
	req.tv_nsec = req_guest->tv_nsec;

	//do x86 syscall
	res = syscall(SYS_nanosleep, &req, rem_p?&rem:NULL);

	if (rem_guest) {
		rem_guest->tv_sec = rem.tv_sec;
		rem_guest->tv_nsec = rem.tv_nsec;
	}

	return res;
}
