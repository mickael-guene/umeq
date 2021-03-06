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

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "gtest/gtest.h"
#include "jitter.h"

#include "jitterFixture.h"

class ContextTest : public jitterFixture {
};

TEST_F(ContextTest, read8) {
    uint8_t in = 4;
    int32_t offset = 1;
    uint8_t out = 0;

    memcpy(&contextBuffer[offset], &in, sizeof(in));

    ir->add_store_8(ir,
                    ir->add_read_context_8(ir, offset),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(in, out);
}

TEST_F(ContextTest, read16) {
    uint16_t in = 0x1234;
    int32_t offset = 64;
    uint16_t out = 0;

    memcpy(&contextBuffer[offset], &in, sizeof(in));

    ir->add_store_16(ir,
                    ir->add_read_context_16(ir, offset),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(in, out);
}

TEST_F(ContextTest, read32) {
    uint32_t in = 0xdeadbeef;
    int32_t offset = 16;
    uint32_t out = 0;

    memcpy(&contextBuffer[offset], &in, sizeof(in));

    ir->add_store_32(ir,
                    ir->add_read_context_32(ir, offset),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(in, out);
}

TEST_F(ContextTest, read64) {
    uint64_t in = 0x12345678deadbeefUL;
    int32_t offset = 256;
    uint64_t out = 0;

    memcpy(&contextBuffer[offset], &in, sizeof(in));

    ir->add_store_64(ir,
                    ir->add_read_context_64(ir, offset),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(in, out);
}

TEST_F(ContextTest, write8) {
    uint8_t in = 9;
    int32_t offset = 121;
    uint8_t out = 0;

    ir->add_write_context_8(ir,
                            ir->add_mov_const_8(ir, in),
                            offset);
    jitAndExcecute();

    memcpy(&out, &contextBuffer[offset], sizeof(out));

    EXPECT_EQ(in, out);
}

TEST_F(ContextTest, write16) {
    uint16_t in = 0x4321;
    int32_t offset = 6;
    uint16_t out = 0;

    ir->add_write_context_16(ir,
                            ir->add_mov_const_16(ir, in),
                            offset);
    jitAndExcecute();

    memcpy(&out, &contextBuffer[offset], sizeof(out));

    EXPECT_EQ(in, out);
}

TEST_F(ContextTest, write32) {
    uint32_t in = 0xabcdef12;
    int32_t offset = 48;
    uint32_t out = 0;

    ir->add_write_context_32(ir,
                            ir->add_mov_const_32(ir, in),
                            offset);
    jitAndExcecute();

    memcpy(&out, &contextBuffer[offset], sizeof(out));

    EXPECT_EQ(in, out);
}

TEST_F(ContextTest, write64) {
    uint64_t in = 0x12345678abcdefUL;
    int32_t offset = 48;
    uint64_t out = 0;

    ir->add_write_context_64(ir,
                            ir->add_mov_const_64(ir, in),
                            offset);
    jitAndExcecute();

    memcpy(&out, &contextBuffer[offset], sizeof(out));

    EXPECT_EQ(in, out);
}

