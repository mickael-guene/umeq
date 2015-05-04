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

class jitterFixture : public ::testing::Test {
    protected:
    jitContext handle;
    struct irInstructionAllocator *ir;
    struct backend *backend;
    char contextBuffer[4096];
    char *beX86_64Memory[BE_X86_64_MIN_CONTEXT_SIZE];
    char *jitterMemory[JITTER_MIN_CONTEXT_SIZE];

    virtual void SetUp() {
        backend = createX86_64Backend(beX86_64Memory, BE_X86_64_MIN_CONTEXT_SIZE);
        handle = createJitter(jitterMemory, backend, JITTER_MIN_CONTEXT_SIZE);
        ir = getIrInstructionAllocator(handle);
    }

    virtual uint64_t jitAndExcecute() {
        uint64_t res = ~0;
        char jitBuffer[4096];

        ir->add_exit(ir, ir->add_mov_const_64(ir, 0));
        //displayIr(handle);
        if (jitCode(handle, jitBuffer, sizeof(jitBuffer)) > 0)
            res = backend->execute(backend, jitBuffer, (uint64_t) contextBuffer);

        return res;
    }

    virtual void TearDown() {
        deleteX86_64Backend(backend);
        deleteJitter(handle);
    }
};

#endif

