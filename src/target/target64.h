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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __TARGET64__
#define __TARGET64__ 1

typedef uint64_t guest_ptr;
#define g_2_h(ptr)  ((void *)(ptr))
#define h_2_g(ptr)  ((uint64_t)(ptr))

extern guest_ptr mmap_guest(guest_ptr addr, size_t length, int prot, int flags, int fd, off_t offset);
extern int munmap_guest(guest_ptr addr, size_t length);
extern void *munmap_guest_ongoing(guest_ptr addr, size_t length);

#endif

#ifdef __cplusplus
}
#endif
