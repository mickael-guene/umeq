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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __SYSCALLS_I386__
#define __SYSCALLS_I386__ 1

int syscall_i386_fstat64(uint32_t fd_p, uint32_t buf_p);
int syscall_i386_lstat64(uint32_t pathname_p, uint32_t buf_p);
int syscall_i386_stat64(uint32_t pathname_p, uint32_t buf_p);
int syscall_i386_fstatat64(uint32_t dirfd_p, uint32_t pathname_p, uint32_t buf_p, uint32_t flags_p);

#endif

#ifdef __cplusplus
}
#endif
