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

#define ROR8(a, b)  (uint8_t)((((a)>>(b))&(0xff >> b)) | ((a)<<(8-(b)))&(0xff<<(8-(b))))
#define ROR16(a, b)  (uint16_t)((((a)>>(b))&(0xffff >> b)) | ((a)<<(16-(b)))&(0xffff<<(16-(b))))
#define ROR32(a, b)  (uint32_t)((((a)>>(b))&(0xffffffff >> b)) | ((a)<<(32-(b)))&(0xffffffff<<(32-(b))))
#define ROR64(a, b)  (uint64_t)((((a)>>(b))&(~0ULL >> b)) | ((a)<<(64-(b)))&(~0ULL<<(64-(b))))

class RorTest : public jitterFixture {
};

TEST_F(RorTest, ror8) {
    uint8_t op1 = 5;
    uint8_t op2 = 2;
    uint8_t out = 0;

    ir->add_store_8(ir,
                    ir->add_ror_8(ir,
                                  ir->add_mov_const_8(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(ROR8(op1,op2), out);
}

TEST_F(RorTest, ror16) {
    uint16_t op1 = 258;
    uint8_t op2 = 6;
    uint16_t out = 0;

    ir->add_store_16(ir,
                    ir->add_ror_16(ir,
                                  ir->add_mov_const_16(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(ROR16(op1,op2), out);
}

TEST_F(RorTest, ror32) {
    uint32_t op1 = 25818;
    uint8_t op2 = 11;
    uint32_t out = 0;

    ir->add_store_32(ir,
                    ir->add_ror_32(ir,
                                  ir->add_mov_const_32(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(ROR32(op1,op2), out);
}

TEST_F(RorTest, ror64) {
    uint64_t op1 = 131313321;
    uint8_t op2 = 2;
    uint64_t out = 0;

    ir->add_store_64(ir,
                    ir->add_ror_64(ir,
                                  ir->add_mov_const_64(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(ROR64(op1,op2), out);
}
