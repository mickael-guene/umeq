#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <sys/time.h>
#include <errno.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int gettimeofday_s3264(uint32_t tv_p, uint32_t tz_p)
{
    int res;
    struct timeval_32 *tv_guest = (struct timeval_32 *) g_2_h(tv_p);
    struct timezone *tz = (struct timezone *) g_2_h(tz_p);
    struct timeval tv;

    if (tv_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall(SYS_gettimeofday, tv_p?&tv:NULL, tz_p?tz:NULL);

        if (tv_p) {
            tv_guest->tv_sec = tv.tv_sec;
            tv_guest->tv_usec = tv.tv_usec;
        }
    }

    return res;
}
