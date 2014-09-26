#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <sys/sysinfo.h>
#include <errno.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int sysinfo_s3264(uint32_t info_p)
{
    struct sysinfo_32 *sysinfo_guest = (struct sysinfo_32 *) g_2_h(info_p);
    struct sysinfo sysinfo;
    int res;

    if (info_p == 0 || info_p == 0xffffffff)
        res = -EFAULT;
    else {
	    res = syscall(SYS_sysinfo,(long) &sysinfo);
        sysinfo_guest->uptime = sysinfo.uptime;
        sysinfo_guest->loads[0] = sysinfo.loads[0];
        sysinfo_guest->loads[1] = sysinfo.loads[1];
        sysinfo_guest->loads[2] = sysinfo.loads[2];
        sysinfo_guest->totalram = sysinfo.totalram;
        sysinfo_guest->freeram = sysinfo.freeram;
        sysinfo_guest->sharedram = sysinfo.sharedram;
        sysinfo_guest->bufferram = sysinfo.bufferram;
        sysinfo_guest->totalswap = sysinfo.totalswap;
        sysinfo_guest->freeswap = sysinfo.freeswap;
        sysinfo_guest->procs = sysinfo.procs;
        sysinfo_guest->totalhigh = sysinfo.totalhigh;
        sysinfo_guest->freehigh = sysinfo.freehigh;
        sysinfo_guest->mem_unit = sysinfo.mem_unit;
    }

    return res;
}
