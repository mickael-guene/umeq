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

#include <sys/types.h>
#include <unistd.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int llseek_s3264(uint32_t fd_p, uint32_t offset_high_p, uint32_t offset_low_p, uint32_t result_p, uint32_t whence_p)
{
    int fd = (int) fd_p;
    uint32_t offset_high = offset_high_p;
    uint32_t offset_low = offset_low_p;
    int64_t *result = (int64_t *) g_2_h(result_p);
    unsigned int whence = (unsigned int) whence_p;
    off_t offset = ((long)offset_high << 32) | offset_low;
    long lseek_res;

    lseek_res = syscall(SYS_lseek, fd, offset, whence);
    *result = lseek_res;

    return lseek_res<0?lseek_res:0;
}
