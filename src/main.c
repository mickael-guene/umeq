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

#include "cache.h"
#include "jitter.h"
#include "target.h"
#include "be_x86_64.h"
#include "runtime.h"
#include "umeq.h"
#include "version.h"

int isGdb = 0;

struct tls_context {
    struct target *target;
    void *target_runtime;
};


/* FIXME: try to factorize loop_nocache and loop_cache */
static int loop_nocache(uint64_t entry, uint64_t stack_entry, uint32_t signum, void *parent_target)
{
    jitContext handle;
    void *targetHandle;
    struct irInstructionAllocator *ir;
    struct backend *backend;
    char jitBuffer[16 * 1024];
    char beX86_64Memory[BE_X86_64_CONTEXT_SIZE];
    char *jitterMemory = alloca(JITTER_CONTEXT_SIZE);
    char *context_memory = alloca(current_target_arch.get_context_size());
    struct target *target;
    void *target_runtime;
    uint64_t currentPc = entry;
    unsigned long tlsareaAddress;
    struct tls_context parent_tls_context;
    struct tls_context *current_tls_context;

    /* setup tls area if not yet set */
    syscall(SYS_arch_prctl, ARCH_GET_FS, &tlsareaAddress);
    if (!tlsareaAddress) {
        syscall(SYS_arch_prctl, ARCH_SET_FS, alloca(sizeof(struct tls_context)));
        syscall(SYS_arch_prctl, ARCH_GET_FS, &tlsareaAddress);
        assert(tlsareaAddress != 0);
    }
    current_tls_context =  (struct tls_context *) tlsareaAddress;

    /* allocate jitter and target context */
    backend = createX86_64Backend(beX86_64Memory);
    handle = createJitter(jitterMemory, backend);
    ir = getIrInstructionAllocator(handle);
    targetHandle = current_target_arch.create_target_context(context_memory);
    target = current_target_arch.get_target_structure(targetHandle);
    target_runtime = current_target_arch.get_target_runtime(targetHandle);
    /* save parent tls context content and setup current one */
    parent_tls_context = *current_tls_context;
    current_tls_context->target = target;
    current_tls_context->target_runtime = target_runtime;
    /* init target */
    target->init(target, parent_tls_context.target, (uint64_t) entry, (uint64_t) stack_entry, signum, parent_target);
    /* enter main loop */
    while(target->isLooping(target)) {
        int jitSize;

        resetJitter(handle);
        if (isGdb && target->gdb(target)->isSingleStepping)
            gdb_handle_breakpoint(target->gdb(target));
        target->disassemble(target, ir, currentPc, (isGdb && target->gdb(target)->isSingleStepping)?1:10/*10*/);
        //displayIr(handle);
        jitSize = jitCode(handle, jitBuffer, sizeof(jitBuffer));
        if (jitSize > 0) {
            currentPc = backend->execute(jitBuffer, (uint64_t) target_runtime);
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
    char jitBuffer[16 * 1024];
    char beX86_64Memory[BE_X86_64_CONTEXT_SIZE];
    char *jitterMemory = alloca(JITTER_CONTEXT_SIZE);
    char *context_memory = alloca(current_target_arch.get_context_size());
    struct target *target;
    void *target_runtime;
    uint64_t currentPc = entry;
    unsigned long tlsareaAddress;
    struct tls_context parent_tls_context;
    struct tls_context *current_tls_context;
    struct cache *cache = NULL;
    char cacheMemory[CACHE_SIZE];

    /* setup tls area if not yet set */
    syscall(SYS_arch_prctl, ARCH_GET_FS, &tlsareaAddress);
    if (!tlsareaAddress) {
        syscall(SYS_arch_prctl, ARCH_SET_FS, alloca(sizeof(struct tls_context)));
        syscall(SYS_arch_prctl, ARCH_GET_FS, &tlsareaAddress);
        assert(tlsareaAddress != 0);
    }
    current_tls_context =  (struct tls_context *) tlsareaAddress;

    /* allocate jitter and target context */
    backend = createX86_64Backend(beX86_64Memory);
    handle = createJitter(jitterMemory, backend);
    ir = getIrInstructionAllocator(handle);
    targetHandle = current_target_arch.create_target_context(context_memory);
    target = current_target_arch.get_target_structure(targetHandle);
    target_runtime = current_target_arch.get_target_runtime(targetHandle);
    /* save parent tls context content and setup current one */
    parent_tls_context = *current_tls_context;
    current_tls_context->target = target;
    current_tls_context->target_runtime = target_runtime;
    /* init target */
    target->init(target, parent_tls_context.target, (uint64_t) entry, (uint64_t) stack_entry, signum, parent_target);
    cache = createCache(cacheMemory);

    while(target->isLooping(target)) {
        void *cache_area;

        cache_area = cache->lookup(cache, currentPc);
        if (cache_area) {
            currentPc = backend->execute(cache_area, (uint64_t) target_runtime);
        } else {
            int jitSize;

            resetJitter(handle);
            target->disassemble(target, ir, currentPc, 10/*10*/);
            //displayIr(handle);
            jitSize = jitCode(handle, jitBuffer, sizeof(jitBuffer));
            if (jitSize > 0) {
                cache->append(cache, currentPc, jitBuffer, jitSize);
                currentPc = backend->execute(jitBuffer, (uint64_t) target_runtime);
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
    if (signum || isGdb)
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

    /* capture umeq arguments.
        This consist on -E, -U and -0 option of qemu.
        These options must be set first.
    */
    while(argv[target_argv0_index]) {
        if (strcmp("-g", argv[target_argv0_index]) == 0) {
            isGdb = 1;
            target_argv0_index++;
        } else if (strcmp("-E", argv[target_argv0_index]) == 0) {
            additionnal_env[additionnal_env_index++] = argv[target_argv0_index + 1];
            target_argv0_index += 2;
        } else if (strcmp("-U", argv[target_argv0_index]) == 0) {
            unset_env[unset_env_index++] = argv[target_argv0_index + 1];
            target_argv0_index += 2;
        } else if (strcmp("-0", argv[target_argv0_index]) == 0) {
            target_argv0 = argv[target_argv0_index + 1];
            target_argv0_index += 2;
        } else if (strcmp("-version", argv[target_argv0_index]) == 0) {
            target_argv0_index++;
            display_version_and_exit();
        } else
            break;
    }
    assert(additionnal_env_index < 16);
    assert(unset_env_index < 16);

    /* load program in memory */
    current_target_arch.loader(argc - target_argv0_index, argv + target_argv0_index,
                                additionnal_env, unset_env, target_argv0, &entry, &stack);
    if (entry) {
        loop(entry, stack, 0, NULL);
    } else {
        info("Unable to open %s\n", argv[target_argv0_index]);
        res = -ENOEXEC;
    }

    return res;
}

/* public api */
void main_wrapper(void *sp)
{
    unsigned int argc = *(unsigned int *)sp;
    char **argv = (char **)(sp + 8);
    int res;

    res = main(argc, argv);

    exit(res);
}
