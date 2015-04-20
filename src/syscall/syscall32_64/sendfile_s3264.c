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

#include <sys/sendfile.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int sendfile_s3264(uint32_t out_fd_p, uint32_t in_fd_p, uint32_t offset_p, uint32_t count_p)
{
    int res;
    int out_fd = (int) out_fd_p;
    int in_fd = (int) in_fd_p;
    int32_t *offset_guest = (int32_t *) g_2_h(offset_p);
    size_t count = (size_t) count_p;
    off_t offset;

    if (offset_p)
        offset = *offset_guest;
    res = syscall(SYS_sendfile, out_fd, in_fd, offset_p?&offset:NULL, count);
    if (offset_p)
        *offset_guest = offset;

    return res;
}
