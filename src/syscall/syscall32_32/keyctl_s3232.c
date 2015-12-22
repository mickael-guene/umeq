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

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>

#include "syscall32_32_types.h"
#include "syscall32_32_private.h"
#include "runtime.h"

typedef int32_t key_serial_t;

#ifndef KEYCTL_GET_KEYRING_ID
#define KEYCTL_GET_KEYRING_ID       0   /* ask for a keyring's ID */
#endif
#ifndef KEYCTL_REVOKE
#define KEYCTL_REVOKE           3   /* revoke a key */
#endif
#ifndef KEYCTL_READ
#define KEYCTL_READ         11  /* read a key or keyring's contents */
#endif

int keyctl_s3232(uint32_t cmd_p, uint32_t arg2_p, uint32_t arg3_p, uint32_t arg4_p, uint32_t arg5_p)
{
    int res;
    int cmd = (int) cmd_p;

    switch(cmd) {
        case KEYCTL_GET_KEYRING_ID:
            res = syscall(SYS_keyctl, cmd, (key_serial_t) arg2_p, (int) arg3_p);
            break;
        case KEYCTL_REVOKE:
            res = syscall(SYS_keyctl, cmd, (key_serial_t) arg2_p);
            break;
        case KEYCTL_READ:
            res = syscall(SYS_keyctl, cmd, (key_serial_t) arg2_p, (char *) g_2_h(arg3_p), (size_t) arg4_p);
            break;
        default:
            fatal("keyctl cmd %d not supported\n", cmd);
    }

    return res;
}
