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

#ifndef __SYSCALL32_32_PRIVATE__
#define __SYSCALL32_32_PRIVATE__ 1

#include "target32.h"

extern int writev_s3232(uint32_t fd_p, uint32_t iov_p, uint32_t iovcnt_p);
extern int futex_s3232(uint32_t uaddr_p, uint32_t op_p, uint32_t val_p, uint32_t timeout_p, uint32_t uaddr2_p, uint32_t val3_p);
extern int fnctl64_s3232(uint32_t fd_p, uint32_t cmd_p, uint32_t opt_p);
extern int stat64_s3232(uint32_t pathname_p, uint32_t buf_p);
extern int fstat64_s3232(uint32_t fd_p, uint32_t buf_p);
extern int lstat64_s3232(uint32_t pathname_p, uint32_t buf_p);
extern int execve_s3232(uint32_t filename_p,uint32_t argv_p,uint32_t envp_p);
extern int fstatat64_s3232(uint32_t dirfd_p, uint32_t pathname_p, uint32_t buf_p, uint32_t flags_p);

#endif

#ifdef __cplusplus
}
#endif
