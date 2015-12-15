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
#include <unistd.h>
#include <assert.h>

#include "syscall32_32_types.h"
#include "syscall32_32_private.h"

/* FIXME: use alloca to allocate space for ptr */
/* FIXME: work only under proot */
int execve_s3232(uint32_t filename_p,uint32_t argv_p,uint32_t envp_p)
{
    int res;
    const char *filename = (const char *) g_2_h(filename_p);
    uint32_t * argv_guest = (uint32_t *) g_2_h(argv_p);
    uint32_t * envp_guest = (uint32_t *) g_2_h(envp_p);
    char *ptr[4096];
    char **argv;
    char **envp;
    int index = 0;

    /* FIXME: do we really need to support this ? */
    /* Manual say 'On Linux, argv and envp can be specified as NULL' */
    assert(argv_p != 0);
    assert(envp_p != 0);

    argv = &ptr[index];
    while(*argv_guest != 0) {
        ptr[index++] = (char *) g_2_h(*argv_guest);
        argv_guest = (uint32_t *) ((long)argv_guest + 4);
    }
    ptr[index++] = NULL;
    envp = &ptr[index];
    while(*envp_guest != 0) {
        ptr[index++] = (char *) g_2_h(*envp_guest);
        envp_guest = (uint32_t *) ((long)envp_guest + 4);
    }
    ptr[index++] = NULL;
    /* sanity check in case we overflow => see fixme */
    if (index >= sizeof(ptr) / sizeof(char *))
        assert(0);

    res = syscall(SYS_execve, filename, argv, envp);

    return res;
}
