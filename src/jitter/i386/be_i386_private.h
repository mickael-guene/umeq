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

#ifndef __BE_I386_PRIVATE__
#define __BE_I386_PRIVATE__ 1

struct backend_execute_result execute_be_i386(struct backend *backend, char *buffer, uint64_t context);
struct backend_execute_result restore_be_i386(struct backend *backend, uint64_t result);

#endif

#ifdef __cplusplus
}
#endif
