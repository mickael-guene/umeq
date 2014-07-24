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

