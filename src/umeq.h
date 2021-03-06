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
#include "cache.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __UMEQ__
#define __UMEQ__ 1

#define KB  (1024)
#define MB  (KB * KB)
#define FAST_MATH_ALLOW     1

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
    struct cache *cache;
};

#if UMEQ_ARCH_HOST_SIZE == 32
    #define ptr_2_int(ptr)  ((uint32_t)(ptr))
    #define int_2_ptr(integer)  ((void *)((uint32_t)(integer)))
#else
    #define ptr_2_int(ptr)  ((uint64_t)(ptr))
    #define int_2_ptr(integer)  ((void *)((uint64_t)(integer)))
#endif

extern enum memory_profile memory_profile;
extern struct target_arch current_target_arch;
extern const char arch_name[];
extern int is_under_proot;
extern int maybe_ptraced;
extern int is_umeq_call_in_execve;
extern char *umeq_filename;

static const int mmap_size[MEM_PROFILE_NB] = {2 * MB, 4 * MB, 8 * MB, 16 * MB};

extern void setup_thread_area(struct tls_context *main_thread_tls_context);
extern struct tls_context *get_tls_context(void);

#endif

#ifdef __cplusplus
}
#endif