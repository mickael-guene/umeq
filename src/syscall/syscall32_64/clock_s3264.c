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

int clock_nanosleep_s3264(uint32_t clock_id_p, uint32_t flags_p, uint32_t request_p, uint32_t remain_p)
{
    clockid_t clock_id = (clockid_t) clock_id_p;
    int flags = (int) flags_p;
    struct timespec_32 *request_guest = (struct timespec_32 *) g_2_h(request_p);
    struct timespec_32 *remain_guest = (struct timespec_32 *) g_2_h(remain_p);
    struct timespec request;
    struct timespec remain;
    int res;

    request.tv_sec = request_guest->tv_sec;
    request.tv_nsec = request_guest->tv_nsec;

    res = syscall(SYS_clock_nanosleep, clock_id, flags, &request, remain_p?&remain:NULL);
    if (remain_p) {
        remain_guest->tv_sec = remain.tv_sec;
        remain_guest->tv_nsec = remain.tv_nsec;
    }

    return res;
}
