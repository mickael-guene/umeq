#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "gtest/gtest.h"
#include "jitter.h"

#include "jitterFixture.h"

class ConstTest : public jitterFixture {
};

TEST_F(ConstTest, const8) {
    uint8_t in = 4;
    uint8_t out = 0;

    ir->add_store_8(ir,
                    ir->add_mov_const_8(ir, in),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(in, out);
}

TEST_F(ConstTest, const16) {
    uint16_t in = 1256;
    uint16_t out = 0;

    ir->add_store_16(ir,
                    ir->add_mov_const_16(ir, in),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(in, out);
}

TEST_F(ConstTest, const32) {
    uint32_t in = 72516;
    uint32_t out = 0;

    ir->add_store_32(ir,
                    ir->add_mov_const_32(ir, in),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(in, out);
}

TEST_F(ConstTest, const64) {
    uint64_t in = 0x1285ff123591;
    uint64_t out = 0;

    ir->add_store_64(ir,
                    ir->add_mov_const_64(ir, in),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(in, out);
}

