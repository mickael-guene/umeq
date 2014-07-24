#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "gtest/gtest.h"
#include "jitter.h"

#include "jitterFixture.h"

class AddTest : public jitterFixture {
};

TEST_F(AddTest, add8) {
    uint8_t op1 = 4;
    uint8_t op2 = 5;
    uint8_t out = 0;

    ir->add_store_8(ir,
                    ir->add_add_8(ir,
                                  ir->add_mov_const_8(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op1+op2, out);
}

TEST_F(AddTest, add8Overflow) {
    uint8_t op1 = 4;
    uint8_t op2 = 255;
    uint8_t out = 0;

    ir->add_store_8(ir,
                    ir->add_add_8(ir,
                                  ir->add_mov_const_8(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((uint8_t) (op1+op2), out);
}

TEST_F(AddTest, add16) {
    uint16_t op1 = 258;
    uint16_t op2 = 5;
    uint16_t out = 0;

    ir->add_store_16(ir,
                    ir->add_add_16(ir,
                                  ir->add_mov_const_16(ir, op1),
                                  ir->add_mov_const_16(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op1+op2, out);
}

TEST_F(AddTest, add16Overflow) {
    uint16_t op1 = 65535;
    uint16_t op2 = 31125;
    uint16_t out = 0;

    ir->add_store_16(ir,
                    ir->add_add_16(ir,
                                  ir->add_mov_const_16(ir, op1),
                                  ir->add_mov_const_16(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((uint16_t) (op1+op2), out);
}

TEST_F(AddTest, add32) {
    uint32_t op1 = 258;
    uint32_t op2 = 1000000;
    uint32_t out = 0;

    ir->add_store_32(ir,
                    ir->add_add_32(ir,
                                  ir->add_mov_const_32(ir, op1),
                                  ir->add_mov_const_32(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op1+op2, out);
}

TEST_F(AddTest, add32Overflow) {
    uint32_t op1 = 3000000;
    uint32_t op2 = 3000001;
    uint32_t out = 0;

    ir->add_store_32(ir,
                    ir->add_add_32(ir,
                                  ir->add_mov_const_32(ir, op1),
                                  ir->add_mov_const_32(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((uint32_t) (op1+op2), out);
}

TEST_F(AddTest, add64) {
    uint64_t op1 = 131313321;
    uint64_t op2 = 1000000;
    uint64_t out = 0;

    ir->add_store_64(ir,
                    ir->add_add_64(ir,
                                  ir->add_mov_const_64(ir, op1),
                                  ir->add_mov_const_64(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op1+op2, out);
}

TEST_F(AddTest, add64Overflow) {
    uint64_t op1 = ~0;
    uint64_t op2 = 3000001;
    uint64_t out = 0;

    ir->add_store_64(ir,
                    ir->add_add_64(ir,
                                  ir->add_mov_const_64(ir, op1),
                                  ir->add_mov_const_64(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((uint64_t) (op1+op2), out);
}

