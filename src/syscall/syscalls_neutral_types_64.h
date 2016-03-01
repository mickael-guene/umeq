/* This file is part of Umeq, an equivalent of qemu user mode emulation with improved robustness.
 *
 * Copyright (C) 2016 STMicroelectronics
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

#include "syscalls_neutral_types_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __SYSCALLS_NEUTRAL_TYPES_64__
#define __SYSCALLS_NEUTRAL_TYPES_64__ 1

struct neutral_linux_dirent_64 {
    uint64_t    d_ino;
    uint64_t    d_off;
    uint16_t    d_reclen;
    char        d_name[1];
};

struct neutral_msgbuf_64 {
    uint64_t mtype;
    char mtext[1];
};

#endif

#ifdef __cplusplus
}
#endif
