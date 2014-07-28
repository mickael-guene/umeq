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

#include "loader.h"
#include "arm.h"
#include "cache.h"
#include "jitter.h"
#include "target.h"
#include "be_x86_64.h"

static int loop_nocache(uint32_t entry, uint32_t stack_entry, uint32_t signum, void *parent_target)
{
    assert(0);

    return 0;
}

static int loop_cache(uint32_t entry, uint32_t stack_entry, uint32_t signum, void *parent_target)
{
    struct target **prev_target;
    jitContext handle;
    armContext armHandle;
    struct irInstructionAllocator *ir;
    struct backend *backend;
    char jitBuffer[16 * 1024];
    char beX86_64Memory[BE_X86_64_CONTEXT_SIZE];
    char jitterMemory[JITTER_CONTEXT_SIZE];
    char armTargetMemory[ARM_CONTEXT_SIZE];
    struct target *target;
    void *arm_context;
    void *prev_arm_context;
    uint64_t currentPc = entry;
    struct target *saved_prev_target;
    char cacheMemory[CACHE_SIZE];
    struct cache *cache = NULL;
    unsigned long tlsarea[16];
    unsigned long tlsareaAddress;

    /* setup tls area if not yet set */
    syscall(SYS_arch_prctl, ARCH_GET_FS, &tlsareaAddress);
    if (!tlsareaAddress) {
        syscall(SYS_arch_prctl, ARCH_SET_FS, tlsarea);
        tlsareaAddress = (unsigned long) tlsarea;
    }

    /* prev_target point to tlsarea */
    prev_target = (struct target **) tlsareaAddress;
    backend = createX86_64Backend(beX86_64Memory);
    handle = createJitter(jitterMemory, backend);
    ir = getIrInstructionAllocator(handle);
    armHandle = createArmContext(armTargetMemory);
    target = getArmTarget(armHandle);
    arm_context = getArmContext(armHandle);
    prev_arm_context = (void *) tlsarea[1];
    tlsarea[1] = (unsigned long) arm_context;

    saved_prev_target = *prev_target;
    *prev_target = target;
    target->init(target, saved_prev_target, (uint64_t) entry, (uint64_t) stack_entry, signum, parent_target);
    cache = createCache(cacheMemory);
    //if (signum == 0 && parent_target == NULL)
    //    proot_inform_exec();

    while(target->isLooping(target)) {
        void *cache_area;

        cache_area = cache->lookup(cache, currentPc);
        if (cache_area) {
            currentPc = backend->execute(cache_area, (uint64_t) arm_context);
        } else {
            int jitSize;

            resetJitter(handle);
            target->disassemble(target, ir, currentPc, 10);
            //displayIr(handle);
            jitSize = jitCode(handle, jitBuffer, sizeof(jitBuffer));
            if (jitSize > 0) {
                cache->append(cache, currentPc, jitBuffer, jitSize);
                currentPc = backend->execute(jitBuffer, (uint64_t) arm_context);
            } else
                assert(0);
        }
    }

    removeCache(cache);

    /* restore previous tls context */
    *prev_target = saved_prev_target;
    tlsarea[1] = (unsigned long) prev_arm_context;

    return target->getExitStatus(target);
}

int loop(uint32_t entry, uint32_t stack_entry, uint32_t signum, void *parent_target)
{
    if (signum)
        return loop_nocache(entry, stack_entry, signum, parent_target);
    else
        return loop_cache(entry, stack_entry, signum, parent_target);
}

/* TODO: Remove limits on -E and -U options */
static int main(int argc, char **argv)
{
    int target_argv0_index = 1;
    void *additionnal_env[16] = {NULL};
    void *unset_env[16] = {NULL};
    void *target_argv0 = NULL;
    int additionnal_env_index = 0;
    int unset_env_index = 0;
    int res = 0;
    void *entry;
    struct load_auxv_info auxv_info;

    /* capture umeq arguments.
        This consist on -E, -U and -0 option of qemu.
        These options must be set first.
    */
    while(argv[target_argv0_index]) {
        if (strcmp("-E", argv[target_argv0_index]) == 0) {
            additionnal_env[additionnal_env_index++] = argv[target_argv0_index + 1];
            target_argv0_index += 2;
        } else if (strcmp("-U", argv[target_argv0_index]) == 0) {
            unset_env[unset_env_index++] = argv[target_argv0_index + 1];
            target_argv0_index += 2;
        } else if (strcmp("-0", argv[target_argv0_index]) == 0) {
            target_argv0 = argv[target_argv0_index + 1];
            target_argv0_index += 2;
        } else
            break;
    }
    assert(additionnal_env_index < 16);
    assert(unset_env_index < 16);

    /* load program in memory */
    entry = arm_load_program(argv[target_argv0_index], &auxv_info);
    if (entry) {
        void *stack_entry = arm_allocate_and_populate_stack(argc - target_argv0_index, argv + target_argv0_index, &auxv_info, additionnal_env, unset_env, target_argv0);
        loop((uint64_t) entry, (uint64_t) stack_entry, 0, NULL);
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
