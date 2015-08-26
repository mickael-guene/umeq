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

#include "umeq.h"
#include "syscall64_64_private.h"

/* FIXME: use alloca to allocate space for ptr */
/* FIXME: work only under proot */
long execve_s6464(uint64_t filename_p, uint64_t argv_p, uint64_t envp_p)
{
    long res;
    const char *filename = (const char *) g_2_h(filename_p);
    uint64_t * argv_guest = (uint64_t *) g_2_h(argv_p);
    uint64_t * envp_guest = (uint64_t *) g_2_h(envp_p);
    char *ptr[4096];
    char **argv;
    char **envp;
    int index = 0;

    /* Manual say 'On Linux, argv and envp can be specified as NULL' */
    /* In that case we give array of length 1 with only one null element */

    argv = &ptr[index];
    if (argv_p) {
        while(*argv_guest != 0) {
            ptr[index++] = (char *) g_2_h(*argv_guest);
            argv_guest = (uint64_t *) ((long)argv_guest + 8);
        }
    }
    ptr[index++] = NULL;
    envp = &ptr[index];
    /* add internal umeq environment variable if process may be ptraced */
    if (maybe_ptraced)
        ptr[index++] = "__UMEQ_INTERNAL_MAYBE_PTRACED__=1";
    if (envp_p) {
        while(*envp_guest != 0) {
            ptr[index++] = (char *) g_2_h(*envp_guest);
            envp_guest = (uint64_t *) ((long)envp_guest + 8);
        }
    }
    ptr[index++] = NULL;
    /* sanity check in case we overflow => see fixme */
    if (index >= sizeof(ptr) / sizeof(char *))
        assert(0);

    res = syscall(SYS_execve, filename, argv, envp);

    return res;
}
