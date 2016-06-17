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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <elf.h>
#include <sys/mman.h>
#include <assert.h>

#include "loader64.h"
#include "runtime.h"

/* FIXME: move this in global config */
#ifndef EM_AARCH64
#define EM_AARCH64 183
#endif

#define PAGE_SIZE                       4096UL
#define PAGE_MASK                       (PAGE_SIZE - 1)
#define DL_LOAD_ADDR                    0x40000000
#define DL_SHARE_ADDR                   0x20000000

#define DL_NAME_MAX_SIZE                256

extern int isGdb;

guest_ptr startbrk_64;
static guest_ptr load_AT_PHDR_init = 0;

static guest_ptr getEntry(int fd, Elf64_Ehdr *hdr, int is_dl)
{
    guest_ptr entry = 0;

    if (read(fd, hdr, sizeof(*hdr)) == sizeof(*hdr)) {
        /* check it's an elf file */
        if (hdr->e_ident[EI_MAG0] == ELFMAG0 &&
            hdr->e_ident[EI_MAG1] == ELFMAG1 &&
            hdr->e_ident[EI_MAG2] == ELFMAG2 &&
            hdr->e_ident[EI_MAG3] == ELFMAG3) {
            /* today only support aarch64 executable load onto x86_64 (noswap .... shortcuts) */
            if (hdr->e_ident[EI_CLASS] == ELFCLASS64 &&
                hdr->e_ident[EI_DATA] == ELFDATA2LSB &&
                (hdr->e_type == ET_EXEC || hdr->e_type == ET_DYN) &&
                hdr->e_machine == EM_AARCH64) {
                    if (is_dl)
                        entry = hdr->e_entry+DL_LOAD_ADDR;
                    else if (hdr->e_type == ET_DYN)
                        entry = hdr->e_entry+DL_SHARE_ADDR;
                    else
                        entry = hdr->e_entry;
            }
        }
    }

    return entry;
}

static int getSegment(int fd, Elf64_Ehdr *hdr, unsigned int idx, Elf64_Phdr *segment)
{
    int status = 0;

    if (idx < hdr->e_phnum) {
        off_t seekOffset = hdr->e_phoff + idx * hdr->e_phentsize;

        if (lseek(fd, seekOffset, SEEK_SET) >= 0) {
            if (read(fd, segment, sizeof(*segment)) == sizeof(*segment))
                status = 1;
        }
    }

    return status;
}

static int elfToMapProtection(uint32_t flags)
{
    int prot = 0;

    if (flags & PF_X)
        prot |= PROT_EXEC;
    if (flags & PF_W)
        prot |= PROT_WRITE;
    if (flags & PF_R)
        prot |= PROT_READ;

    if (isGdb)
        prot |= PROT_WRITE;

    return prot;
}

static int mapSegment(int fd, Elf64_Phdr *segment, struct load_auxv_info_64 *auxv_info, int is_dl, int is_share_object)
{
    uint64_t vaddr;
    uint64_t offset;
    uint64_t padding;
    int status = 0;
    guest_ptr mapFileResult;

    /* adjest load address according to module type */
    if (is_dl)
        segment->p_vaddr += DL_LOAD_ADDR;
    else if (is_share_object)
        segment->p_vaddr += DL_SHARE_ADDR;

    /* aligned on page start */
    vaddr = segment->p_vaddr & ~PAGE_MASK;
    offset = segment->p_offset & ~PAGE_MASK;
    padding = segment->p_vaddr & PAGE_MASK;

    // TODO : perhaps need to check it's entirely into this map segment and
    // not completely sure calculation is ok
    // adjust load_AT_PHDR
    if (load_AT_PHDR_init >= segment->p_offset &&
        load_AT_PHDR_init < segment->p_offset + segment->p_memsz) {
        auxv_info->load_AT_PHDR = load_AT_PHDR_init + segment->p_vaddr - segment->p_offset;
    }

    /* map file part */
    if (segment->p_filesz) {
        mapFileResult = mmap_guest(vaddr, segment->p_filesz + padding, elfToMapProtection(segment->p_flags), MAP_PRIVATE | MAP_FIXED, fd, offset);
        if (mapFileResult != vaddr)
            goto exit;
        else
            status = 1;
    }
    /* map memory part */
    if (segment->p_memsz != segment->p_filesz) {
        uint64_t zeroAddr = segment->p_vaddr + segment->p_filesz;
        uint64_t zeroAddrAlignedOnNextPage = (zeroAddr + PAGE_SIZE - 1) & ~PAGE_MASK;
        uint64_t zeroAddrSize = segment->p_memsz - segment->p_filesz;
        int64_t zeroAddrMapSize = zeroAddrSize - (zeroAddrAlignedOnNextPage - zeroAddr);
        char *zeroAddrBuffer = (char *) g_2_h(zeroAddr);

        assert(segment->p_memsz > segment->p_filesz);

        /* need to map more memory for bss ? */
        if (zeroAddrMapSize > 0) {
            mapFileResult = mmap_guest(zeroAddrAlignedOnNextPage, zeroAddrMapSize, elfToMapProtection(segment->p_flags), MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);
            if (mapFileResult != zeroAddrAlignedOnNextPage)
                goto exit;
        }
        /* clear bss */
        while(zeroAddrSize--)
            *zeroAddrBuffer++ = 0;
        // clear memory until end of page
        if (elfToMapProtection(segment->p_flags) & PROT_WRITE) {
            while((long) zeroAddrBuffer % PAGE_SIZE != 0)
                *zeroAddrBuffer++ = 0;
        }
        status = 1;

    }
    /* setup startbrk_64 at the end of the last segment */
    if (!is_dl)
        startbrk_64 = segment->p_vaddr + segment->p_memsz;

    exit:
    return status;
}

static void unmapSegment(int fd, Elf64_Phdr *segment)
{
    munmap_guest(segment->p_vaddr, segment->p_memsz);
}

static void dl_copy_dl_name(int fd, Elf64_Phdr *segment, char *name)
{
    unsigned int p_filesz = segment->p_filesz;

    if (p_filesz > DL_NAME_MAX_SIZE)
        p_filesz = DL_NAME_MAX_SIZE;
    if (lseek(fd, segment->p_offset, SEEK_SET) >= 0) {
        ssize_t res = read(fd, name, p_filesz);
        assert(res == p_filesz);
        name[segment->p_filesz] = '\0';
    }
}

static guest_ptr load64_internal(const char *file, struct load_auxv_info_64 *auxv_info, int is_dl)
{
    Elf64_Ehdr elf_header;
    Elf64_Phdr segment;
    guest_ptr dl_entry = 0;
    guest_ptr entry = 0;
    int fd;
    int i = 0;
    int is_share_object;

    elf_header.e_phnum = 0;
    fd = open(file, O_RDONLY);
    if (fd < 0)
        goto end;
    /* first read header and extrace entry point */
    entry = getEntry(fd, &elf_header, is_dl);
    if (entry == 0)
        goto end;
    is_share_object = (elf_header.e_type == ET_DYN)?1:0;

    load_AT_PHDR_init = elf_header.e_phoff;
    auxv_info->load_AT_PHENT = elf_header.e_phentsize;
    auxv_info->load_AT_PHNUM = elf_header.e_phnum;
    auxv_info->load_AT_ENTRY = entry;
    /* map all segments */
    for(i=0; i< elf_header.e_phnum; i++) {
        if (getSegment(fd, &elf_header, i, &segment)) {
            if (segment.p_type == PT_LOAD) {
                if (!mapSegment(fd, &segment, auxv_info, is_dl, is_share_object)) {
                    entry = 0;
                    goto end;
                }
            } else if (segment.p_type == PT_INTERP) {
                char dl_name[DL_NAME_MAX_SIZE];
                struct load_auxv_info_64 dl_auxv_info;

                dl_copy_dl_name(fd, &segment, dl_name);
                dl_entry = load64_internal(dl_name, &dl_auxv_info, 1);
                if (!dl_entry) {
                    entry = 0;
                    goto end;
                }
            }
        } else {
            entry = 0;
            goto end;
        }
    }

end:
    //TODO : unmap in case of error
    if (i < elf_header.e_phnum) {
        while(i--) {
            if (getSegment(fd, &elf_header, i, &segment)) {
                if (segment.p_type == PT_LOAD)
                    unmapSegment(fd, &segment);
            }
        }
    }

    if (fd >= 0)
        close(fd);

    return entry?(dl_entry?dl_entry:entry):0;
}

/* public api */
guest_ptr load64(const char *file, struct load_auxv_info_64 *auxv_info)
{
    return load64_internal(file, auxv_info, 0);
}
