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

#define _GNU_SOURCE        /* or _BSD_SOURCE or _SVID_SOURCE */
#include <unistd.h>
#include <sys/syscall.h>
#include <asm/prctl.h>
#include <sys/prctl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "cache.h"
#include "jitter.h"
#include "target.h"
#include "be_x86_64.h"
#include "runtime.h"
#include "umeq.h"
#include "version.h"

char *exe_filename;
int is_under_proot;

struct memory_config {
    int max_insn;
    int jitter_context_size;
    int be_context_size;
    int cache_size;
};

enum memory_profile memory_profile = MEM_PROFILE_2M;

const struct memory_config nocache_memory_config = {5, 32 * KB, 32 * KB, 0};
const struct memory_config cache_memory_config[MEM_PROFILE_NB] = {
    {10, 64 * KB, 64 * KB, 1 * MB},
    {10, 64 * KB, 64 * KB, 3 * MB},
    {20, 128 * KB, 128 * KB, 6 * MB},
    {40, 256 * KB, 256 * KB, 14 * MB},
};

static void setup_thread_area(struct tls_context *main_thread_tls_context)
{
    main_thread_tls_context->target = NULL;
    main_thread_tls_context->target_runtime = NULL;

    syscall(SYS_arch_prctl, ARCH_SET_FS, main_thread_tls_context);
}

static void setup_jit_area(union jit_area *jit_area)
{
    int i;

    for(i = 0; i < 15; i++)
        jit_area->cooked.magic[i] = 0x66;
    jit_area->cooked.magic[15] = 0x90;
}

/* FIXME: try to factorize loop_nocache and loop_cache */
static int loop_nocache(uint64_t entry, uint64_t stack_entry, uint32_t signum, void *parent_target)
{
    jitContext handle;
    void *targetHandle;
    struct irInstructionAllocator *ir;
    struct backend *backend;
    union jit_area jit_area;
    char *beX86_64Memory = alloca(nocache_memory_config.be_context_size);
    char *jitterMemory = alloca(nocache_memory_config.jitter_context_size);
    char *context_memory = alloca(current_target_arch.get_context_size());
    struct target *target;
    void *target_runtime;
    uint64_t currentPc = entry;
    struct tls_context parent_tls_context;
    struct tls_context *current_tls_context;
    struct backend_execute_result result;

    setup_jit_area(&jit_area);
    /* get current tls context */
    syscall(SYS_arch_prctl, ARCH_GET_FS, &current_tls_context);

    /* allocate jitter and target context */
    backend = createX86_64Backend(beX86_64Memory, nocache_memory_config.be_context_size);
    handle = createJitter(jitterMemory, backend, nocache_memory_config.jitter_context_size);
    ir = getIrInstructionAllocator(handle);
    targetHandle = current_target_arch.create_target_context(context_memory, backend);
    target = current_target_arch.get_target_structure(targetHandle);
    target_runtime = current_target_arch.get_target_runtime(targetHandle);
    /* init target */
    target->init(target, current_tls_context->target, (uint64_t) entry, (uint64_t) stack_entry, signum, parent_target);
    /* now that new context is ready to run, setup as the current one */
    parent_tls_context = *current_tls_context;
    current_tls_context->target = target;
    current_tls_context->target_runtime = target_runtime;

    /* enter main loop */
    while(target->isLooping(target)) {
        int jitSize;

        resetJitter(handle);
        target->disassemble(target, ir, currentPc, nocache_memory_config.max_insn);
        //displayIr(handle);
        jit_area.cooked.guest_pc = currentPc;
        jitSize = jitCode(handle, jit_area.cooked.jit_buffer, JIT_AREA_JIT_BUFFER_SIZE);
        if (jitSize > 0) {
            result = backend->execute(backend, jit_area.cooked.jit_buffer, (uint64_t) target_runtime);
            currentPc = result.result;
        } else
            assert(0);
    }
    /* restore parent tls context */
    *current_tls_context = parent_tls_context;

    return target->getExitStatus(target);
}

static int loop_cache(uint64_t entry, uint64_t stack_entry, uint32_t signum, void *parent_target)
{
    jitContext handle;
    void *targetHandle;
    struct irInstructionAllocator *ir;
    struct backend *backend;
    union jit_area jit_area;
    char *beX86_64Memory = alloca(cache_memory_config[memory_profile].be_context_size);
    char *jitterMemory = alloca(cache_memory_config[memory_profile].jitter_context_size);
    char *context_memory = alloca(current_target_arch.get_context_size());
    struct target *target;
    void *target_runtime;
    uint64_t currentPc = entry;
    struct tls_context parent_tls_context;
    struct tls_context *current_tls_context;
    struct cache *cache = NULL;
    struct backend_execute_result result = {0, 0};
    uint64_t prevCurrentPc = ~0;
    char *cacheMemory = alloca(cache_memory_config[memory_profile].cache_size);

    setup_jit_area(&jit_area);
    /* get current tls context */
    syscall(SYS_arch_prctl, ARCH_GET_FS, &current_tls_context);

    /* allocate jitter and target context */
    backend = createX86_64Backend(beX86_64Memory, cache_memory_config[memory_profile].be_context_size);
    handle = createJitter(jitterMemory, backend, cache_memory_config[memory_profile].jitter_context_size);
    ir = getIrInstructionAllocator(handle);
    targetHandle = current_target_arch.create_target_context(context_memory, backend);
    target = current_target_arch.get_target_structure(targetHandle);
    target_runtime = current_target_arch.get_target_runtime(targetHandle);
    /* init target */
    target->init(target, current_tls_context->target, (uint64_t) entry, (uint64_t) stack_entry, signum, parent_target);
    cache = createCache(cacheMemory, cache_memory_config[memory_profile].cache_size, current_target_arch.get_nb_of_pc_bit_to_drop());
    /* now that new context is ready to run, setup as the current one */
    parent_tls_context = *current_tls_context;
    current_tls_context->target = target;
    current_tls_context->target_runtime = target_runtime;

    while(target->isLooping(target)) {
        void *cache_area;
        int is_cache_was_cleaned = 0;

        cache_area = cache->lookup(cache, currentPc, &is_cache_was_cleaned);
        if (cache_area) {
            prevCurrentPc = currentPc;
            result = backend->execute(backend, cache_area + sizeof(uint64_t) + JIT_AREA_MAGIC_SIZE, (uint64_t) target_runtime);
            currentPc = result.result;
        } else {
            int jitSize;

            resetJitter(handle);
            target->disassemble(target, ir, currentPc, cache_memory_config[memory_profile].max_insn/*10*/);
            //displayIr(handle);
            jit_area.cooked.guest_pc = currentPc;
            jitSize = jitCode(handle, jit_area.cooked.jit_buffer, JIT_AREA_JIT_BUFFER_SIZE);
            if (jitSize > 0) {
                cache_area = cache->append(cache, currentPc, jit_area.buffer, jitSize + sizeof(uint64_t) + JIT_AREA_MAGIC_SIZE, &is_cache_was_cleaned);
                /* only link forward to avoid loop */
                if (prevCurrentPc < currentPc && result.link_patch_area && is_cache_was_cleaned == 0) {
                    backend->patch(backend, result.link_patch_area, cache_area + sizeof(uint64_t) + JIT_AREA_MAGIC_SIZE);
                }
                prevCurrentPc = currentPc;
                result = backend->execute(backend, cache_area + sizeof(uint64_t) + JIT_AREA_MAGIC_SIZE, (uint64_t) target_runtime);
                currentPc = result.result;
            } else
                assert(0);
        }
    }
    removeCache(cache);
    /* restore parent tls context */
    *current_tls_context = parent_tls_context;

    return target->getExitStatus(target);
}

int loop(uint64_t entry, uint64_t stack_entry, uint32_t signum, void *parent_target)
{
    if (signum)
        return loop_nocache(entry, stack_entry, signum, parent_target);
    else
        return loop_cache(entry, stack_entry, signum, parent_target);
}

static void display_version_and_exit()
{
    fprintf(stderr, "umeq-%s version %s (%s)\n", arch_name, GIT_VERSION, GIT_DESCRIBE);
    exit(0);
}

/* TODO: Remove limits on -E and -U options */
int main(int argc, char **argv)
{
    int target_argv0_index = 1;
    void *additionnal_env[16] = {NULL};
    void *unset_env[16] = {NULL};
    void *target_argv0 = NULL;
    int additionnal_env_index = 0;
    int unset_env_index = 0;
    int res = 0;
    uint64_t entry = 0;
    uint64_t stack;
    struct tls_context main_thread_tls_context;

    /* capture umeq arguments.
        This consist on -E, -U and -0 option of qemu.
        These options must be set first.
    */
    while(argv[target_argv0_index]) {
        if (strcmp("-E", argv[target_argv0_index]) == 0) {
            additionnal_env[additionnal_env_index++] = argv[target_argv0_index + 1];
            target_argv0_index += 2;
            is_under_proot = 1;
        } else if (strcmp("-U", argv[target_argv0_index]) == 0) {
            unset_env[unset_env_index++] = argv[target_argv0_index + 1];
            target_argv0_index += 2;
            is_under_proot = 1;
        } else if (strcmp("-0", argv[target_argv0_index]) == 0) {
            target_argv0 = argv[target_argv0_index + 1];
            target_argv0_index += 2;
            is_under_proot = 1;
        } else if (strcmp("-version", argv[target_argv0_index]) == 0) {
            target_argv0_index++;
            display_version_and_exit();
        } else
            break;
    }
    assert(additionnal_env_index < 16);
    assert(unset_env_index < 16);

    exe_filename = argv[target_argv0_index];
    /* load program in memory */
    current_target_arch.loader(argc - target_argv0_index, argv + target_argv0_index,
                                additionnal_env, unset_env, target_argv0, &entry, &stack);
    if (entry) {
        setup_thread_area(&main_thread_tls_context);
        res = loop(entry, stack, 0, NULL);
    } else {
        info("Unable to open %s\n", argv[target_argv0_index]);
        res = -ENOEXEC;
    }

    return res;
}

/* try to use maximum memory profile for maximum performance */
void setup_memory_profile()
{
    struct rlimit limit;
    rlim_t current_soft_limit;
    rlim_t new_soft_limit = 0;
    int res;

    /* get current limit */
    res = getrlimit(RLIMIT_STACK, &limit);
    if (res) {
        //warning("getrlimit return res = %d / %d\n", res, errno);
        return;
    }

    //debug("current soft limit is %d\n", limit.rlim_cur);
    current_soft_limit = limit.rlim_cur;
    /* check if we can select an upper limit that current one */
    if (limit.rlim_max >= 16 * MB && limit.rlim_max > limit.rlim_cur)
        new_soft_limit = 16 * MB;
    else if (limit.rlim_max >= 8 * MB && limit.rlim_max > limit.rlim_cur)
        new_soft_limit = 8 * MB;
    else if (limit.rlim_max >= 4 * MB && limit.rlim_max > limit.rlim_cur)
        new_soft_limit = 4 * MB;
    else if (limit.rlim_max >= 2 * MB && limit.rlim_max > limit.rlim_cur)
        new_soft_limit = 2 * MB;
    if (new_soft_limit > current_soft_limit) {
        /* yes we can !!! so just to it */
        struct rlimit new_limit = limit;

        new_limit.rlim_cur = new_soft_limit;
        res = setrlimit(RLIMIT_STACK, &new_limit);
        if (!res)
            current_soft_limit = new_soft_limit;
    }
    /* now setup memory profile */
    if (current_soft_limit >= 16 * MB) {
        //debug("MEM_PROFILE_16M\n");
        memory_profile = MEM_PROFILE_16M;;
    } else if (current_soft_limit >= 8 * MB) {
        //debug("MEM_PROFILE_8M\n");
        memory_profile = MEM_PROFILE_8M;
    } else if (current_soft_limit >= 4 * MB) {
        //debug("MEM_PROFILE_4M\n");
        memory_profile = MEM_PROFILE_4M;
    } else if (current_soft_limit >= 2 * MB) {
        //debug("MEM_PROFILE_2M\n");
        memory_profile = MEM_PROFILE_2M;
    } else
        fatal("umeq need at least stack size limit to be 2M bytes to run\n");
}

/* public api */
void main_wrapper(void *sp)
{
    unsigned int argc = *(unsigned int *)sp;
    char **argv = (char **)(sp + 8);
    int res;

    setup_memory_profile();
    res = main(argc, argv);

    exit(res);
}
