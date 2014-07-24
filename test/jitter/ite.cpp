#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "gtest/gtest.h"
#include "jitter.h"

#include "jitterFixture.h"

class IteTest : public jitterFixture {
};

TEST_F(IteTest, ite8True) {
    uint8_t pred = 1;
    uint8_t opTrue = 128;
    uint8_t opFalse = 2;
    uint8_t out = 0;

    ir->add_store_8(ir,
                    ir->add_ite_8(ir,
                                  ir->add_mov_const_8(ir, pred),
                                  ir->add_mov_const_8(ir, opTrue),
                                  ir->add_mov_const_8(ir, opFalse)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(opTrue, out);
}

TEST_F(IteTest, ite8False) {
    uint8_t pred = 0;
    uint8_t opTrue = 128;
    uint8_t opFalse = 2;
    uint8_t out = 0;

    ir->add_store_8(ir,
                    ir->add_ite_8(ir,
                                  ir->add_mov_const_8(ir, pred),
                                  ir->add_mov_const_8(ir, opTrue),
                                  ir->add_mov_const_8(ir, opFalse)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(opFalse, out);
}

TEST_F(IteTest, ite16True) {
    uint16_t pred = 1;
    uint16_t opTrue = 128;
    uint16_t opFalse = 2;
    uint16_t out = 0;

    ir->add_store_16(ir,
                    ir->add_ite_16(ir,
                                  ir->add_mov_const_16(ir, pred),
                                  ir->add_mov_const_16(ir, opTrue),
                                  ir->add_mov_const_16(ir, opFalse)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(opTrue, out);
}

TEST_F(IteTest, ite16False) {
    uint16_t pred = 0;
    uint16_t opTrue = 128;
    uint16_t opFalse = 2;
    uint16_t out = 0;

    ir->add_store_16(ir,
                    ir->add_ite_16(ir,
                                  ir->add_mov_const_16(ir, pred),
                                  ir->add_mov_const_16(ir, opTrue),
                                  ir->add_mov_const_16(ir, opFalse)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(opFalse, out);
}

TEST_F(IteTest, ite32True) {
    uint32_t pred = 12;
    uint32_t opTrue = 35000;
    uint32_t opFalse = 12132;
    uint32_t out = 0;

    ir->add_store_32(ir,
                    ir->add_ite_32(ir,
                                  ir->add_mov_const_32(ir, pred),
                                  ir->add_mov_const_32(ir, opTrue),
                                  ir->add_mov_const_32(ir, opFalse)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(opTrue, out);
}

TEST_F(IteTest, ite32False) {
    uint32_t pred = 0;
    uint32_t opTrue = 131321;
    uint32_t opFalse = 1213;
    uint32_t out = 0;

    ir->add_store_32(ir,
                    ir->add_ite_32(ir,
                                  ir->add_mov_const_32(ir, pred),
                                  ir->add_mov_const_32(ir, opTrue),
                                  ir->add_mov_const_32(ir, opFalse)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(opFalse, out);
}

TEST_F(IteTest, ite64True) {
    uint64_t pred = 1;
    uint64_t opTrue = ~0;
    uint64_t opFalse = 12;
    uint64_t out = 0;

    ir->add_store_64(ir,
                    ir->add_ite_64(ir,
                                  ir->add_mov_const_64(ir, pred),
                                  ir->add_mov_const_64(ir, opTrue),
                                  ir->add_mov_const_64(ir, opFalse)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(opTrue, out);
}

TEST_F(IteTest, ite64False) {
    uint64_t pred = 0;
    uint64_t opTrue = 121316313;
    uint64_t opFalse = 1212;
    uint64_t out = 0;

    ir->add_store_64(ir,
                    ir->add_ite_64(ir,
                                  ir->add_mov_const_64(ir, pred),
                                  ir->add_mov_const_64(ir, opTrue),
                                  ir->add_mov_const_64(ir, opFalse)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(opFalse, out);
}

