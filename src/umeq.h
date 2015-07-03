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

#include <stdint.h>
#include "jitter.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __UMEQ__
#define __UMEQ__ 1

#define KB  (1024)
#define MB  (KB * KB)

#define JIT_AREA_SIZE               16 * KB
#define JIT_AREA_MAGIC_SIZE         16
#define JIT_AREA_JIT_BUFFER_SIZE    JIT_AREA_SIZE - sizeof(uint64_t) - JIT_AREA_MAGIC_SIZE

extern char *exe_filename;

struct target_arch {
    /* loader */
    void (*loader)(int argc, char **argv, void **additionnal_env, void **unset_env, void *target_argv0, uint64_t *entry, uint64_t *stack);
    /* return arch context size */
    int (*get_context_size)(void);
    /* create a target context using given memory. This function return an handle to manipulate target context */
    void *(*create_target_context)(void *memory, struct backend *backend);
    /* delete a target context. */
    void (*delete_target_context)(void *context);
    /* return target structure related to the given context */
    struct target *(*get_target_structure)(void *context);
    /* return runtime target handle from the given context */
    void *(*get_target_runtime)(void *context);
    /* return number of lsb bit that are useless for cache */
    int (*get_nb_of_pc_bit_to_drop)(void);
};

enum memory_profile {
    MEM_PROFILE_2M = 0,
    MEM_PROFILE_4M,
    MEM_PROFILE_8M,
    MEM_PROFILE_16M,
    MEM_PROFILE_NB
};

struct tls_context {
    struct target *target;
    void *target_runtime;
};

union jit_area {
    char buffer[JIT_AREA_SIZE];
    struct {
        uint64_t guest_pc;
        char magic[JIT_AREA_MAGIC_SIZE];
        char jit_buffer[JIT_AREA_JIT_BUFFER_SIZE];
    } cooked;
};

extern enum memory_profile memory_profile;
extern struct target_arch current_target_arch;
extern const char arch_name[];
extern int is_under_proot;

static const int mmap_size[MEM_PROFILE_NB] = {2 * MB, 4 * MB, 8 * MB, 16 * MB};

#endif

#ifdef __cplusplus
}
#endif