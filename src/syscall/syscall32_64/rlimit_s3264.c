#include <sys/time.h>
#include <sys/resource.h>
#include <errno.h>

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
