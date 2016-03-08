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

#include <string.h>

#include "arm64_syscall.h"
#include "syscalls_neutral.h"
#include "syscalls_neutral_types.h"

long arm64_uname(struct arm64_target *context)
{
    long res;
    struct neutral_new_utsname *buf = (struct neutral_new_utsname *) g_2_h(context->regs.r[0]);

    res = syscall_adapter_guest64(PR_uname, context->regs.r[0], context->regs.r[1], context->regs.r[2],
                                            context->regs.r[3], context->regs.r[4], context->regs.r[5]);
    if (res == 0)
        strcpy(buf->machine, "aarch64");

    return res;
}
