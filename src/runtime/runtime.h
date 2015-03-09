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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __RUNTIME__
#define __RUNTIME__ 1

#include <stdio.h>
#include <stdarg.h>
#include <assert.h>

extern void debug(const char *format, ...);
extern void info(const char *format, ...);
extern void warning(const char *format, ...);
extern void error(const char *format, ...);
extern void fatal_internal(const char *format, ...);
#define fatal(...) \
do { \
fatal_internal(__VA_ARGS__); \
assert(0); \
} while(0)

#endif

#ifdef __cplusplus
}
#endif
