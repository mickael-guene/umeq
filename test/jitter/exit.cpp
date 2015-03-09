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

class ExitTest : public jitterFixture {
};

TEST_F(ExitTest, exit) {
    uint64_t res;

    ir->add_exit(ir, ir->add_mov_const_64(ir, 0x100000002UL));
    res = jitAndExcecute();

    EXPECT_EQ(res, 0x100000002UL);
}

TEST_F(ExitTest, exitCondFalse) {
    uint64_t res;

    ir->add_exit_cond(ir,
                      ir->add_mov_const_64(ir, 0x100000003UL),
                      ir->add_mov_const_32(ir, 0));
    ir->add_exit(ir, ir->add_mov_const_64(ir, 0x100000002UL));
    res = jitAndExcecute();

    EXPECT_EQ(res, 0x100000002UL);
}

TEST_F(ExitTest, exitCondTrue) {
    uint64_t res;

    ir->add_exit_cond(ir,
                      ir->add_mov_const_64(ir, 0x100000004UL),
                      ir->add_mov_const_8(ir, 1));
    ir->add_exit(ir, ir->add_mov_const_64(ir, 0x100000002UL));
    res = jitAndExcecute();

    EXPECT_EQ(res, 0x100000004UL);
}

