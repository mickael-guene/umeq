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

#include <stdlib.h>
#include <stdint.h>

#include "jitter.h"
#include "gdb.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __TARGET__
#define __TARGET__ 1

struct target {
    void (*init)(struct target *target, struct target *prev_target, uint64_t entry, uint64_t stack_ptr, uint32_t signum, void *param);
    void (*disassemble)(struct target *target, struct irInstructionAllocator *irAlloc, uint64_t pc, int maxInsn);
    uint32_t (*isLooping)(struct target *target);
    uint32_t (*getExitStatus)(struct target *target);
    struct gdb *(*gdb)(struct target *target);
};

#endif

#ifdef __cplusplus
}
#endif
