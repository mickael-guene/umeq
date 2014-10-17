#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>

#include "target.h"
#include "arm64.h"
#include "arm64_private.h"
#include "runtime.h"

#define ARM64_CONTEXT_SIZE     (4096)

typedef void *arm64Context;

/* backend implementation */
static void init(struct target *target, struct target *prev_target, uint64_t entry, uint64_t stack_ptr, uint32_t signum, void *param)
{
    struct arm64_target *context = container_of(target, struct arm64_target, target);
    struct arm64_target *prev_context = container_of(prev_target, struct arm64_target, target);
    int i;

    context-> isLooping = 1;
    if (signum) {
        fatal("Implement me\n");
    } else if (param) {
        fatal("Implement me\n");
    } else if (stack_ptr) {
        for(i = 0; i < 32; i++) {
            context->regs.r[i] = 0;
            context->regs.v[i].v128 = 0;
        }
       	context->regs.r[31] = stack_ptr;
       	context->regs.pc = entry;
        context->regs.tpidr_el0 = 0;
        context->regs.nzcv = 0;
        context->sp_init = stack_ptr;
        context->pc = entry;
    } else {
        //fork;
        //nothing to do
        fatal("Should not come here\n");
    }
}

static void disassemble(struct target *target, struct irInstructionAllocator *ir, uint64_t pc, int maxInsn)
{
    disassemble_arm64(target, ir, pc, maxInsn);
}

static uint32_t isLooping(struct target *target)
{
    struct arm64_target *context = container_of(target, struct arm64_target, target);

    return context->isLooping;
}

static uint32_t getExitStatus(struct target *target)
{
    struct arm64_target *context = container_of(target, struct arm64_target, target);

    return context->exitStatus;
}

/* gdb stuff */
static struct gdb *gdb(struct target *target)
{
    struct arm64_target *context = container_of(target, struct arm64_target, target);

    return &context->gdb;
}

static void gdb_read_registers(struct gdb *gdb, char *buf)
{
    struct arm64_target *context = container_of(gdb, struct arm64_target, gdb);
    int i ,j;
    unsigned long val;
    uint32_t pc = context->regs.pc;

    for(i=0;i<32;i++) {
        val = context->regs.r[i];
        for(j=0;j<8;j++) {
            unsigned int byte = (val >> (j * 8)) & 0xff;
            unsigned int hnibble = (byte >> 4);
            unsigned int lnibble = (byte & 0xf);

            buf[1] = gdb_tohex(lnibble);
            buf[0] = gdb_tohex(hnibble);

            buf += 2;
        }
    }
    val = context->regs.pc;
    for(j=0;j<8;j++) {
        unsigned int byte = (val >> (j * 8)) & 0xff;
        unsigned int hnibble = (byte >> 4);
        unsigned int lnibble = (byte & 0xf);

        buf[1] = gdb_tohex(lnibble);
        buf[0] = gdb_tohex(hnibble);

        buf += 2;
    }
    val = context->regs.nzcv;
    for(j=0;j<8;j++) {
        unsigned int byte = (val >> (j * 8)) & 0xff;
        unsigned int hnibble = (byte >> 4);
        unsigned int lnibble = (byte & 0xf);

        buf[1] = gdb_tohex(lnibble);
        buf[0] = gdb_tohex(hnibble);

        buf += 2;
    }

    *buf = '\0';
}

/* context handling code */
static int getArm64ContextSize()
{
    return ARM64_CONTEXT_SIZE;
}

static arm64Context createArm64Context(void *memory)
{
    struct arm64_target *context;

    assert(ARM64_CONTEXT_SIZE >= sizeof(*context));
    context = (struct arm64_target *) memory;
    if (context) {
        context->target.init = init;
        context->target.disassemble = disassemble;
        context->target.isLooping = isLooping;
        context->target.getExitStatus = getExitStatus;
        context->target.gdb = gdb;
        context->gdb.state = GDB_STATE_SYNCHRO;
        context->gdb.commandPos = 0;
        context->gdb.isContinue = 0;
        context->gdb.isSingleStepping = 1;
        context->gdb.read_registers = gdb_read_registers;
    }

    return (arm64Context) context;
}

static void deleteArm64Context(arm64Context handle)
{
    ;
}

static struct target *getArm64Target(arm64Context handle)
{
    struct arm64_target *context = (struct arm64_target *) handle;

    return &context->target;
}

static void *getArm64Context(arm64Context handle)
{
    struct arm64_target *context = (struct arm64_target *) handle;

    return &context->regs;
}

/* api */
struct target_arch arm64_arch = {
    arm64_load_image,
    getArm64ContextSize,
    createArm64Context,
    deleteArm64Context,
    getArm64Target,
    getArm64Context
};
