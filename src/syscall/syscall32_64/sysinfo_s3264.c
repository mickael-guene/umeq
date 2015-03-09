/* This file is part of Umeq, an equivalent of qemu user mode emulation with improved robustness.
 *
 * Copyright (C) 2015 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

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
