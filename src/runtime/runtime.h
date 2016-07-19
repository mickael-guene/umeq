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

//#define ASSERT_ON_ILLEGAL   1

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

#ifdef __IN_HELPERS
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <signal.h>

#ifdef ASSERT_ON_ILLEGAL
#define fatal_illegal_opcode(...) \
do { \
fatal_internal(__VA_ARGS__); \
assert(0); \
} while(0)

#define assert_illegal_opcode(cond) \
do { \
assert(cond); \
} while(0)
#else /* ASSERT_ON_ILLEGAL */

#define fatal_illegal_opcode(...) \
do { \
    generate_illegal_signal(); \
} while(0)

#define assert_illegal_opcode(cond) \
do { \
if (!(cond)) { \
    generate_illegal_signal(); \
} \
} while(0)
#endif /* ASSERT_ON_ILLEGAL */
#else /*__IN_HELPERS*/
#ifdef ASSERT_ON_ILLEGAL
#define fatal_illegal_opcode(...) \
do { \
(void) context; \
(void) ir; \
fatal_internal(__VA_ARGS__); \
assert(0); \
return 1; \
} while(0)

#define assert_illegal_opcode(cond) \
do { \
(void) context; \
(void) ir; \
if (!(cond)) { \
    assert(cond); \
    return 1; \
} \
} while(0)
#else /* ASSERT_ON_ILLEGAL */
#define fatal_illegal_opcode(...) \
do { \
mk_gdb_breakpoint_instruction(context, ir); \
return 1; \
} while(0)

#define assert_illegal_opcode(cond) \
do { \
if (!(cond)) { \
    mk_gdb_breakpoint_instruction(context, ir); \
    return 1; \
} \
} while(0)
#endif /* ASSERT_ON_ILLEGAL */
#endif /* __IN_HELPERS */

#endif

#ifdef __cplusplus
}
#endif
