#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "gtest/gtest.h"
#include "jitter.h"

#include "jitterFixture.h"

class ShrTest : public jitterFixture {
};

TEST_F(ShrTest, shr8) {
    uint8_t op1 = 4;
    uint8_t op2 = 2;
    uint8_t out = 0;

    ir->add_store_8(ir,
                    ir->add_shr_8(ir,
                                  ir->add_mov_const_8(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op1>>op2, out);
}

TEST_F(ShrTest, shr16) {
    uint16_t op1 = 258;
    uint8_t op2 = 6;
    uint16_t out = 0;

    ir->add_store_16(ir,
                    ir->add_shr_16(ir,
                                  ir->add_mov_const_16(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op1>>op2, out);
}

TEST_F(ShrTest, shr32) {
    uint32_t op1 = 25818;
    uint8_t op2 = 11;
    uint32_t out = 0;

    ir->add_store_32(ir,
                    ir->add_shr_32(ir,
                                  ir->add_mov_const_32(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op1>>op2, out);
}

TEST_F(ShrTest, shr64) {
    uint64_t op1 = 131313321;
    uint8_t op2 = 2;
    uint64_t out = 0;

    ir->add_store_64(ir,
                    ir->add_shr_64(ir,
                                  ir->add_mov_const_64(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op1>>op2, out);
}

