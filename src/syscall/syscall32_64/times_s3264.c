#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <sys/times.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int times_s3264(uint32_t buf_p)
{
    int res;
    struct tms_32 *buf_guest = (struct tms_32 *) g_2_h(buf_p);
    struct tms buf;

    res = syscall(SYS_times, &buf);
    if (buf_p) {
        buf_guest->tms_utime = buf.tms_utime;
        buf_guest->tms_stime = buf.tms_stime;
        buf_guest->tms_cutime = buf.tms_cutime;
        buf_guest->tms_cstime = buf.tms_cstime;
    }

    return res;
}
