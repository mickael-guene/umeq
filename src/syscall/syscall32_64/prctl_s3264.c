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

#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/prctl.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int prctl_s3264(uint32_t option_p, uint32_t arg2_p, uint32_t arg3_p, uint32_t arg4_p, uint32_t arg5_p)
{
	int res;
	int option = (int) option_p;

	switch(option) {
		case PR_GET_PDEATHSIG:
			res = syscall(SYS_prctl, option, (int *) g_2_h(arg2_p));
			break;
		default:
			res = syscall(SYS_prctl, option, (unsigned long) arg2_p, (unsigned long) arg3_p, (unsigned long) arg4_p, (unsigned long) arg5_p);
	}

	return res;
}