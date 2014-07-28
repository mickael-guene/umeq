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

void *arm_load_program(const char *file, struct load_auxv_info *auxv_info);
void * arm_allocate_and_populate_stack(int argc, char **argv, struct load_auxv_info *auxv_info, void **additionnal_env, void **unset_env, void *target_argv0);

#endif

#ifdef __cplusplus
}
#endif
