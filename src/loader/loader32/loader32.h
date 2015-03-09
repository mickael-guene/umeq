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

#ifndef LOADER32_H
#define LOADER32_H

#include "target32.h"

struct load_auxv_info_32 {
	guest_ptr load_AT_PHDR; /* Program headers for program */
	unsigned int load_AT_PHENT; /* Size of program header entry */
	unsigned int load_AT_PHNUM; /* Number of program headers */
	guest_ptr load_AT_ENTRY; /* Entry point of program */
};

extern guest_ptr load32(const char *file, struct load_auxv_info_32 *auxv_info);

#endif

