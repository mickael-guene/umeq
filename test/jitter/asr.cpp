#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "gtest/gtest.h"
#include "jitter.h"

#include "jitterFixture.h"

class AsrTest : public jitterFixture {
};

TEST_F(AsrTest, asr8) {
    uint8_t op1 = 128;
    uint8_t op2 = 2;
    uint8_t out = 0;

    ir->add_store_8(ir,
                    ir->add_asr_8(ir,
                                  ir->add_mov_const_8(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((uint8_t)((int8_t)op1>>op2), out);
}

TEST_F(AsrTest, asr16) {
    uint16_t op1 = 48523;
    uint8_t op2 = 6;
    uint16_t out = 0;

    ir->add_store_16(ir,
                    ir->add_asr_16(ir,
                                  ir->add_mov_const_16(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((uint16_t)((int16_t)op1>>op2), out);
}

TEST_F(AsrTest, asr32) {
    uint32_t op1 = 3000000000;
    uint8_t op2 = 11;
    uint32_t out = 0;

    ir->add_store_32(ir,
                    ir->add_asr_32(ir,
                                  ir->add_mov_const_32(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((uint32_t)((int32_t)op1>>op2), out);
}

TEST_F(AsrTest, asr32_overflow) {
    uint32_t op1 = 0x4010500f;
    uint8_t op2 = 64;
    uint32_t out = 0;

    ir->add_store_32(ir,
                    ir->add_asr_32(ir,
                                  ir->add_mov_const_32(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(0, out);
}

TEST_F(AsrTest, asr32_sign_overflow) {
    uint32_t op1 = 0x8010500f;
    uint8_t op2 = 64;
    uint32_t out = 0;

    ir->add_store_32(ir,
                    ir->add_asr_32(ir,
                                  ir->add_mov_const_32(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(0xffffffff, out);
}

TEST_F(AsrTest, asr64) {
    uint64_t op1 = ~0;
    uint8_t op2 = 2;
    uint64_t out = 0;

    ir->add_store_64(ir,
                    ir->add_asr_64(ir,
                                  ir->add_mov_const_64(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((uint64_t)((int64_t)op1>>op2), out);
}

