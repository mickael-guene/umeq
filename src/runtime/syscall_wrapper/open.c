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

#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>

int open(__const char *__file, int __oflag, ...)
{
    int status;

    if (__oflag & O_CREAT) {
        mode_t mode;
        va_list ap;

        va_start(ap, __oflag);
        mode = va_arg(ap, mode_t);
        status = syscall((long) SYS_open, (long) __file, (long) __oflag, (long) mode);
        va_end(ap);
    } else {
        // TODO : do I have to provide a dummy mode ?
        status = syscall((long) SYS_open, (long) __file, (long) __oflag);
    }

    return status;
}
