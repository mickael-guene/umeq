#include <stdio.h>
#include <assert.h>
#include <stddef.h>

#include "target.h"
#include "arm.h"
#include "arm_private.h"


/* backend implementation */
static void init(struct target *target, struct target *prev_target, uint64_t entry, uint64_t stack_ptr, uint32_t signum, void *param)
{
    struct arm_target *context = container_of(target, struct arm_target, target);
    struct arm_target *prev_context = container_of(prev_target, struct arm_target, target);
    int i;

    context-> isLooping = 1;
    if (signum) {
        /* new signal */
        uint32_t *dst;
        uint32_t sp;
        unsigned int return_code[] = {0xe3a07077, //mov     r0, #119
                                      0xef000000, //svc     0x00000000
                                      0xe320f000, //nop
                                      0xeafffffd};//b svc

        sp = (prev_context->regs.r[13] - 4 * 16 - sizeof(return_code)) & ~7;
        for(i = 0, dst = (unsigned int *)(long)sp;i < sizeof(return_code)/sizeof(return_code[0]); i++)
            *dst++ = return_code[i];
        context->regs.c13_tls2 = prev_context->regs.c13_tls2;
        context->regs.r[0] = signum;
        context->regs.r[1] = (uint32_t)(long) param; /* siginfo_t * */
        context->regs.r[2] = 0; /* void * */
        context->regs.r[13] = sp;
        context->regs.r[14] = sp;
        context->regs.r[15] = (uint32_t) entry;
    } else if (param) {
        /* new thread */
        struct arm_target *parent_context = container_of(param, struct arm_target, target);

        for(i = 0; i < 15; i++)
            context->regs.r[i] = parent_context->regs.r[i];
        context->regs.c13_tls2 = parent_context->regs.r[3];
        context->regs.r[0] = 0;
        context->regs.r[13] = (uint32_t) stack_ptr;
        context->regs.r[15] = (uint32_t) entry;
        context->regs.cpsr = 0;
    } else if (stack_ptr) {
        /* main thread */
        for(i = 0; i < 15; i++)
            context->regs.r[i] = 0;
        context->regs.r[13] = (uint32_t) stack_ptr;
        context->regs.r[15] = (uint32_t) entry;
        context->regs.cpsr = 0;
        context->sp_init = (uint32_t) stack_ptr;
    } else {
        //fork;
        //nothing to do
    }
}

static void disassemble(struct target *target, struct irInstructionAllocator *ir, uint64_t pc, int maxInsn)
{
    if (pc & 1)
        disassemble_thumb(target, ir, pc, maxInsn);
    else
        disassemble_arm(target, ir, pc, maxInsn);
}

static uint32_t isLooping(struct target *target)
{
    struct arm_target *context = container_of(target, struct arm_target, target);

    return context->isLooping;
}

static uint32_t getExitStatus(struct target *target)
{
    struct arm_target *context = container_of(target, struct arm_target, target);

    return context->exitStatus;
}

/* api */
armContext createArmContext(void *memory)
{
    struct arm_target *context;

    assert(ARM_CONTEXT_SIZE >= sizeof(*context));
    context = (struct arm_target *) memory;
    if (context) {
        context->target.init = init;
        context->target.disassemble = disassemble;
        context->target.isLooping = isLooping;
        context->target.getExitStatus = getExitStatus;
    }

    return (armContext) context;
}

void deleteArmContext(armContext handle)
{
    ;
}

struct target *getArmTarget(armContext handle)
{
    struct arm_target *context = (struct arm_target *) handle;

    return &context->target;
}

void *getArmContext(armContext handle)
{
    struct arm_target *context = (struct arm_target *) handle;

    return &context->regs;
}
