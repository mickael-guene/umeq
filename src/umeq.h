#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __UMEQ__
#define __UMEQ__ 1

struct target_arch {
    /* loader */
    void (*loader)(int argc, char **argv, void **additionnal_env, void **unset_env, void *target_argv0, uint64_t *entry, uint64_t *stack);
    /* return arch context size */
    int (*get_context_size)(void);
    /* create a target context using given memory. This function return an handle to manipulate target context */
    void *(*create_target_context)(void *memory);
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

extern enum memory_profile memory_profile;
extern struct target_arch current_target_arch;
extern const char arch_name[];

#endif

#ifdef __cplusplus
}
#endif