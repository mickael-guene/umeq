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

#ifndef __SYSCALLS_X86_64__
#define __SYSCALLS_X86_64__ 1

long syscall_x86_64_fstat64(uint64_t fd_p, uint64_t buf_p);
long syscall_x86_64_lstat64(uint64_t pathname_p, uint64_t buf_p);
long syscall_x86_64_stat64(uint64_t pathname_p, uint64_t buf_p);
long syscall_x86_64_fstatat64(uint64_t dirfd_p, uint64_t pathname_p, uint64_t buf_p, uint64_t flags_p);
long syscall_x86_64_fstat(uint64_t fd_p, uint64_t buf_p);
long syscall_x86_64_newfstatat(uint64_t dirfd_p, uint64_t pathname_p, uint64_t buf_p, uint64_t flags_p);

#endif

#ifdef __cplusplus
}
#endif
