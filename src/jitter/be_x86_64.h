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

#include "jitter.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __BE_X86_64__
#define __BE_X86_64__ 1

#define BE_X86_64_MIN_CONTEXT_SIZE     (32 * 1024)

struct backend *createX86_64Backend(void *memory, int size);
void deleteX86_64Backend(struct backend *backend);

#endif

#ifdef __cplusplus
}
#endif
