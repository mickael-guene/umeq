#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>
#include <assert.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int ugetrlimit_s3264(uint32_t resource_p, uint32_t rlim_p)
{
    int res;
    int ressource = (int) resource_p;
    struct rlimit_32 *rlim_guest = (struct rlimit_32 *) g_2_h(rlim_p);
    struct rlimit rlim;

    if (rlim_p == 0 || rlim_p == 0xffffffff)
        res = -EFAULT;
    else {
        res = syscall(SYS_getrlimit, ressource, &rlim);
        rlim_guest->rlim_cur = rlim.rlim_cur;
        rlim_guest->rlim_max = rlim.rlim_max;
    }

    return res;
}

int prlimit64_s3264(uint32_t pid_p, uint32_t resource_p, uint32_t new_limit_p, uint32_t old_limit_p)
{
    int res = 0;
    pid_t pid = (pid_t) pid_p;
    int resource = (int) resource_p;
    struct rlimit64_32 *new_limit_guest = (struct rlimit64_32 *) g_2_h(new_limit_p);
    struct rlimit64_32 *old_limit_guest = (struct rlimit64_32 *) g_2_h(old_limit_p);
    struct rlimit new_limit;
    struct rlimit old_limit;

    assert(pid == 0);
    if (old_limit_p) {
        res = syscall(SYS_getrlimit, resource, &old_limit);
        old_limit_guest->rlim_cur = old_limit.rlim_cur;
        old_limit_guest->rlim_max = old_limit.rlim_max;
    }
    if (res == 0 && new_limit_p) {
        new_limit.rlim_cur = new_limit_guest->rlim_cur;
        new_limit.rlim_max = new_limit_guest->rlim_max;

        res = syscall(SYS_setrlimit, resource, &new_limit);
    }

    return res;
}
