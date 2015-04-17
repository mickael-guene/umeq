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
#include <sys/types.h>
#include <sys/types.h>
#include <unistd.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int getgroups_s3264(uint32_t size_p, uint32_t list_p)
{
    int res;
    int size = (int) size_p;
    uint16_t *list_guest = (uint16_t *) g_2_h(list_p);
    gid_t *list = alloca(sizeof(gid_t) * size);
    int i;

    res = syscall(SYS_getgroups, size, list);
    for(i = 0; i < size; i++)
        list_guest[i] = list[i];

    return res;
}
