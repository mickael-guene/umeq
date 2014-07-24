#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "gtest/gtest.h"
#include "jitter.h"

#include "jitterFixture.h"

class CastTest : public jitterFixture {
};

TEST_F(CastTest, cast8uto16) {
    uint8_t op = 5;
    uint16_t out = 0;

    ir->add_store_16(ir,
                     ir->add_8U_to_16(ir,
                                      ir->add_mov_const_8(ir, op)),
                     ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op, out);
}

TEST_F(CastTest, cast8uto32) {
    uint8_t op = 129;
    uint32_t out = 0;

    ir->add_store_32(ir,
                     ir->add_8U_to_32(ir,
                                      ir->add_mov_const_8(ir, op)),
                     ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op, out);
}

TEST_F(CastTest, cast8uto64) {
    uint8_t op = 255;
    uint64_t out = 0;

    ir->add_store_64(ir,
                     ir->add_8U_to_64(ir,
                                      ir->add_mov_const_8(ir, op)),
                     ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op, out);
}

TEST_F(CastTest, cast8sto16) {
    uint8_t op = 255;
    uint16_t out = 0;
    int64_t cast = (int64_t) op;
    uint64_t res = ((cast << 56) >> 56) & 0xffff;

    ir->add_store_16(ir,
                     ir->add_8S_to_16(ir,
                                      ir->add_mov_const_8(ir, op)),
                     ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(res, out);
}

TEST_F(CastTest, cast8sto32) {
    uint8_t op = 128;
    uint32_t out = 0;
    int64_t cast = (int64_t) op;
    uint64_t res = ((cast << 56) >> 56) & 0xffffffff;

    ir->add_store_32(ir,
                     ir->add_8S_to_32(ir,
                                      ir->add_mov_const_8(ir, op)),
                     ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(res, out);
}

TEST_F(CastTest, cast8sto64) {
    uint8_t op = 128;
    uint64_t out = 0;
    int64_t cast = (int64_t) op;
    uint64_t res = ((cast << 56) >> 56);

    ir->add_store_64(ir,
                     ir->add_8S_to_64(ir,
                                      ir->add_mov_const_8(ir, op)),
                     ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(res, out);
}

TEST_F(CastTest, cast16sto32) {
    uint16_t op = 39000;
    uint32_t out = 0;
    int64_t cast = (int64_t) op;
    uint64_t res = ((cast << 48) >> 48) & 0xffffffff;

    ir->add_store_32(ir,
                     ir->add_16S_to_32(ir,
                                      ir->add_mov_const_16(ir, op)),
                     ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(res, out);
}

TEST_F(CastTest, cast16sto64) {
    uint16_t op = 128;
    uint64_t out = 0;
    int64_t cast = (int64_t) op;
    uint64_t res = ((cast << 48) >> 48);

    ir->add_store_64(ir,
                     ir->add_16S_to_64(ir,
                                      ir->add_mov_const_16(ir, op)),
                     ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(res, out);
}

TEST_F(CastTest, cast32sto64) {
    uint32_t op = ~0;
    uint64_t out = 0;
    int64_t cast = (int64_t) op;
    uint64_t res = ((cast << 32) >> 32);

    ir->add_store_64(ir,
                     ir->add_32S_to_64(ir,
                                      ir->add_mov_const_32(ir, op)),
                     ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(res, out);
}

TEST_F(CastTest, cast64to8) {
    uint64_t op = 32132416231;
    uint8_t out = 0;

    ir->add_store_8(ir,
                     ir->add_64_to_8(ir,
                                      ir->add_mov_const_64(ir, op)),
                     ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op & 0xff, out);
}

TEST_F(CastTest, cast32to8) {
    uint32_t op = 3000000;
    uint8_t out = 0;

    ir->add_store_8(ir,
                     ir->add_32_to_8(ir,
                                      ir->add_mov_const_64(ir, op)),
                     ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op & 0xff, out);
}

TEST_F(CastTest, cast16to8) {
    uint16_t op = 35000;
    uint8_t out = 0;

    ir->add_store_8(ir,
                     ir->add_16_to_8(ir,
                                      ir->add_mov_const_64(ir, op)),
                     ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op & 0xff, out);
}

TEST_F(CastTest, cast64to16) {
    uint64_t op = 32132416231;
    uint16_t out = 0;

    ir->add_store_16(ir,
                     ir->add_64_to_16(ir,
                                      ir->add_mov_const_64(ir, op)),
                     ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op & 0xffff, out);
}

TEST_F(CastTest, cast32to16) {
    uint32_t op = 3000000;
    uint16_t out = 0;

    ir->add_store_16(ir,
                     ir->add_32_to_16(ir,
                                      ir->add_mov_const_64(ir, op)),
                     ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op & 0xffff, out);
}

TEST_F(CastTest, cast64to32) {
    uint64_t op = 32132416231;
    uint32_t out = 0;

    ir->add_store_32(ir,
                     ir->add_64_to_32(ir,
                                      ir->add_mov_const_64(ir, op)),
                     ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ(op & 0xffffffff, out);
}

