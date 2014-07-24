#ifndef __JITTER_FIXTURE__
#define __JITTER_FIXTURE__ 1

#include "be_x86_64.h"

//#define USE_INTERPRETED

class jitterFixture : public ::testing::Test {
    protected:
    jitContext handle;
    struct irInstructionAllocator *ir;
    struct backend *backend;
    char jitBuffer[4096];
    char contextBuffer[4096];
#ifdef USE_INTERPRETED
    char *beInterpretedMemory[BE_INTERPRETED_CONTEXT_SIZE];
#else
    char *beX86_64Memory[BE_X86_64_CONTEXT_SIZE];
#endif
    char *jitterMemory[JITTER_CONTEXT_SIZE];

    virtual void SetUp() {
        backend = createX86_64Backend(beX86_64Memory);
        handle = createJitter(jitterMemory, backend);
        ir = getIrInstructionAllocator(handle);
    }

    virtual uint64_t jitAndExcecute() {
        uint64_t res = ~0;

        ir->add_exit(ir, ir->add_mov_const_64(ir, 0));
        //displayIr(handle);
        if (jitCode(handle, jitBuffer, sizeof(jitBuffer)) > 0)
            res = backend->execute(jitBuffer, (uint64_t) contextBuffer);

        return res;
    }

    virtual void TearDown() {
        deleteX86_64Backend(backend);
        deleteJitter(handle);
    }
};

#endif

