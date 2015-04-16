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

struct elf32_fdpic_loadseg
{
  /* Core address to which the segment is mapped.  */
  guest_ptr addr;
  /* VMA recorded in the program header.  */
  guest_ptr p_vaddr;
  /* Size of this segment in memory.  */
  uint32_t p_memsz;
};

struct elf32_fdpic_loadmap {
  uint16_t version;
  uint16_t nsegs;
  struct elf32_fdpic_loadseg segs[2];
};

struct fdpic_info_32 {
    int is_fdpic;
    uint32_t stack_size;
    guest_ptr dl_dynamic_section_addr;
    guest_ptr dl_load_addr;
    struct elf32_fdpic_loadmap executable;
    struct elf32_fdpic_loadmap dl;
    guest_ptr executable_addr;
    guest_ptr dl_addr;
};

struct elf_loader_info_32 {
    guest_ptr load_AT_PHDR; /* Program headers for program */
    unsigned int load_AT_PHENT; /* Size of program header entry */
    unsigned int load_AT_PHNUM; /* Number of program headers */
    guest_ptr load_AT_ENTRY; /* Entry point of program */
    struct fdpic_info_32 fdpic_info;
};

extern guest_ptr load32(const char *file, struct elf_loader_info_32 *auxv_info);

#endif

