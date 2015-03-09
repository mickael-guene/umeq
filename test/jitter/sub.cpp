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

class SubTest : public jitterFixture {
};

TEST_F(SubTest, sub8) {
    uint8_t op1 = 100;
    uint8_t op2 = 5;
    uint8_t out = 0;

    ir->add_store_8(ir,
                    ir->add_sub_8(ir,
                                  ir->add_mov_const_8(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((uint8_t) (op1-op2), out);
}

TEST_F(SubTest, sub8Overflow) {
    uint8_t op1 = 4;
    uint8_t op2 = 255;
    uint8_t out = 0;

    ir->add_store_8(ir,
                    ir->add_sub_8(ir,
                                  ir->add_mov_const_8(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((uint8_t) (op1-op2), out);
}

TEST_F(SubTest, sub16) {
    uint16_t op1 = 258;
    uint16_t op2 = 5;
    uint16_t out = 0;

    ir->add_store_16(ir,
                    ir->add_sub_16(ir,
                                  ir->add_mov_const_16(ir, op1),
                                  ir->add_mov_const_16(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((uint16_t) (op1-op2), out);
}

TEST_F(SubTest, sub16Overflow) {
    uint16_t op1 = 15535;
    uint16_t op2 = 31125;
    uint16_t out = 0;

    ir->add_store_16(ir,
                    ir->add_sub_16(ir,
                                  ir->add_mov_const_16(ir, op1),
                                  ir->add_mov_const_16(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((uint16_t) (op1-op2), out);
}

TEST_F(SubTest, sub32) {
    uint32_t op1 = 131258;
    uint32_t op2 = 1;
    uint32_t out = 0;

    ir->add_store_32(ir,
                    ir->add_sub_32(ir,
                                  ir->add_mov_const_32(ir, op1),
                                  ir->add_mov_const_32(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((uint32_t) (op1-op2), out);
}

TEST_F(SubTest, sub32Overflow) {
    uint32_t op1 = 3000000;
    uint32_t op2 = 3000001;
    uint32_t out = 0;

    ir->add_store_32(ir,
                    ir->add_sub_32(ir,
                                  ir->add_mov_const_32(ir, op1),
                                  ir->add_mov_const_32(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((uint32_t) (op1-op2), out);
}

TEST_F(SubTest, sub64) {
    uint64_t op1 = 131313321;
    uint64_t op2 = 1000000;
    uint64_t out = 0;

    ir->add_store_64(ir,
                    ir->add_sub_64(ir,
                                  ir->add_mov_const_64(ir, op1),
                                  ir->add_mov_const_64(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((uint64_t) (op1-op2), out);
}

TEST_F(SubTest, sub64Overflow) {
    uint64_t op1 = 1;
    uint64_t op2 = 3000001;
    uint64_t out = 0;

    ir->add_store_64(ir,
                    ir->add_sub_64(ir,
                                  ir->add_mov_const_64(ir, op1),
                                  ir->add_mov_const_64(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((uint64_t) (op1-op2), out);
}

