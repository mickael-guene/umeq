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

class ShlTest : public jitterFixture {
};

TEST_F(ShlTest, shl8) {
    uint8_t op1 = 4;
    uint8_t op2 = 2;
    uint8_t out = 0;

    ir->add_store_8(ir,
                    ir->add_shl_8(ir,
                                  ir->add_mov_const_8(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op1<<op2, out);
}

TEST_F(ShlTest, shl16) {
    uint16_t op1 = 258;
    uint8_t op2 = 6;
    uint16_t out = 0;

    ir->add_store_16(ir,
                    ir->add_shl_16(ir,
                                  ir->add_mov_const_16(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op1<<op2, out);
}

TEST_F(ShlTest, shl32) {
    uint32_t op1 = 258;
    uint8_t op2 = 15;
    uint32_t out = 0;

    ir->add_store_32(ir,
                    ir->add_shl_32(ir,
                                  ir->add_mov_const_32(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op1<<op2, out);
}

TEST_F(ShlTest, shl64) {
    uint64_t op1 = 131313321;
    uint8_t op2 = 21;
    uint64_t out = 0;

    ir->add_store_64(ir,
                    ir->add_shl_64(ir,
                                  ir->add_mov_const_64(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op1<<op2, out);
}

