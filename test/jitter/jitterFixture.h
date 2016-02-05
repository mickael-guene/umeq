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

#ifndef __JITTER_FIXTURE__
#define __JITTER_FIXTURE__ 1

#include "be_x86_64.h"
#include "be_i386.h"

class jitterFixture : public ::testing::Test {
    protected:
    jitContext handle;
    struct irInstructionAllocator *ir;
    struct backend *backend;
    char contextBuffer[4096];
#ifdef __i386__
    char *beI386Memory[BE_I386_MIN_CONTEXT_SIZE];
#else
    char *beX86_64Memory[BE_X86_64_MIN_CONTEXT_SIZE];
#endif
    char *jitterMemory[JITTER_MIN_CONTEXT_SIZE];

    virtual void SetUp() {
#ifdef __i386__
        backend = createI386Backend(beI386Memory, BE_I386_MIN_CONTEXT_SIZE);
#else
        backend = createX86_64Backend(beX86_64Memory, BE_X86_64_MIN_CONTEXT_SIZE);
#endif
        handle = createJitter(jitterMemory, backend, JITTER_MIN_CONTEXT_SIZE);
        ir = getIrInstructionAllocator(handle);
    }

    virtual uint64_t jitAndExcecute() {
        struct backend_execute_result result;
        uint64_t res = ~0;
        char jitBuffer[4096];

        result.result = ~0UL;
        ir->add_exit(ir, ir->add_mov_const_64(ir, 0));
        //displayIr(handle);
        if (jitCode(handle, jitBuffer, sizeof(jitBuffer)) > 0)
            result = backend->execute(backend, jitBuffer, (uint64_t) contextBuffer);

        return result.result;
    }

    virtual void TearDown() {
#ifdef __i386__
        deleteI386Backend(backend);
#else
        deleteX86_64Backend(backend);
#endif
        deleteJitter(handle);
    }
};

#endif

