#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>

#include "target.h"
#include "arm.h"
#include "arm_private.h"

#define ARM_CONTEXT_SIZE     (4096)

typedef void *armContext;
const char arch_name[] = "arm";

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
        for(i = 0, dst = (unsigned int *)g_2_h(sp);i < sizeof(return_code)/sizeof(return_code[0]); i++)
            *dst++ = return_code[i];
        context->regs.c13_tls2 = prev_context->regs.c13_tls2;
        context->regs.r[0] = signum;
        context->regs.r[1] = (uint32_t)(long) param; /* siginfo_t * */
        context->regs.r[2] = 0; /* void * */
        context->regs.r[13] = sp;
        context->regs.r[14] = sp;
        context->regs.r[15] = (uint32_t) entry;
        context->regs.reg_itstate = 0;
        context->disa_itstate = 0;
        context->regs.is_in_syscall = 0;
        context->is_in_signal = 1;
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
        context->regs.reg_itstate = parent_context->regs.reg_itstate; //should be zero
        assert(context->regs.reg_itstate == 0);
        context->disa_itstate = 0;
        context->regs.is_in_syscall = 0;
        context->is_in_signal = 0;
    } else if (stack_ptr) {
        /* main thread */
        for(i = 0; i < 15; i++)
            context->regs.r[i] = 0;
        context->regs.r[13] = (uint32_t) stack_ptr;
        context->regs.r[15] = (uint32_t) entry;
        context->regs.cpsr = 0;
        context->sp_init = (uint32_t) stack_ptr;
        context->regs.reg_itstate = 0;
        context->disa_itstate = 0;
        context->is_in_signal = 0;
        /* syscall execve exit sequence */
         /* this will be translated into sysexec exit */
        context->regs.is_in_syscall = 2;
        syscall((long) 313, 1);
         /* this will be translated into SIGTRAP */
        context->regs.is_in_syscall = 0;
        syscall((long) 313, 2);
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

/* gdb stuff */
static struct gdb *gdb(struct target *target)
{
    struct arm_target *context = container_of(target, struct arm_target, target);

    return &context->gdb;
}

static void gdb_read_registers(struct gdb *gdb, char *buf)
{
    struct arm_target *context = container_of(gdb, struct arm_target, gdb);
    int i ,j;
    unsigned int val;
    uint32_t pc = context->regs.r[15];

    for(i=0;i<16;i++) {
        if (i == 15)
            val = context->regs.r[i] & ~1;
        else
            val = context->regs.r[i];
        for(j=0;j<4;j++) {
            unsigned int byte = (val >> (j * 8)) & 0xff;
            unsigned int hnibble = (byte >> 4);
            unsigned int lnibble = (byte & 0xf);

            buf[1] = gdb_tohex(lnibble);
            buf[0] = gdb_tohex(hnibble);

            buf += 2;
        }
    }
    for(i=0;i<25;i++) {
        for(j=0;j<4;j++) {
            buf[0] = buf[1] = gdb_tohex(0);
            buf += 2;
        }
    }

    val = context->regs.cpsr;
    if (pc & 1)
        val |= 0x20;
    for(j=0;j<4;j++) {
        unsigned int byte = (val >> (j * 8)) & 0xff;
        unsigned int hnibble = (byte >> 4);
        unsigned int lnibble = (byte & 0xf);

        buf[1] = gdb_tohex(lnibble);
        buf[0] = gdb_tohex(hnibble);

        buf += 2;
    }
    *buf = '\0';
}

static int getArmContextSize()
{
    return ARM_CONTEXT_SIZE;
}

static armContext createArmContext(void *memory)
{
    struct arm_target *context;

    assert(ARM_CONTEXT_SIZE >= sizeof(*context));
    context = (struct arm_target *) memory;
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

    return (armContext) context;
}

static void deleteArmContext(armContext handle)
{
    ;
}

static struct target *getArmTarget(armContext handle)
{
    struct arm_target *context = (struct arm_target *) handle;

    return &context->target;
}

static void *getArmContext(armContext handle)
{
    struct arm_target *context = (struct arm_target *) handle;

    return &context->regs;
}

static int getArmNbOfPcBitToDrop()
{
    return 1;
}

/* api */
struct target_arch current_target_arch = {
    arm_load_image,
    getArmContextSize,
    createArmContext,
    deleteArmContext,
    getArmTarget,
    getArmContext,
    getArmNbOfPcBitToDrop
};
