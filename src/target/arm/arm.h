#include <stdlib.h>
#include "loader.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM_TARGET__
#define __ARM_TARGET__ 1

#define ARM_CONTEXT_SIZE     (4096)

typedef void *armContext;

armContext createArmContext(void *memory);
void deleteArmContext(armContext);
struct target *getArmTarget(armContext);
void *getArmContext(armContext);

/* FIXME: not good to have this stuff here ..... */
void arm_load_image(int argc, char **argv, void **additionnal_env, void **unset_env, void *target_argv0,
                    uint64_t *entry, uint64_t *stack);

#endif

#ifdef __cplusplus
}
#endif
