#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include "gtest/gtest.h"
#include "jitter.h"

#include "jitterFixture.h"

class CmpneTest : public jitterFixture {
};

TEST_F(CmpneTest, cmpne8Eq) {
    uint8_t op1 = 5;
    uint8_t op2 = 5;
    uint8_t out = 0;

    ir->add_store_8(ir,
                    ir->add_cmpne_8(ir,
                                  ir->add_mov_const_8(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((op1!=op2?~0:0) & 0xff, out);
}

TEST_F(CmpneTest, cmpne8Ne) {
    uint8_t op1 = 4;
    uint8_t op2 = 5;
    uint8_t out = 0;

    ir->add_store_8(ir,
                    ir->add_cmpne_8(ir,
                                  ir->add_mov_const_8(ir, op1),
                                  ir->add_mov_const_8(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((op1!=op2?~0:0) & 0xff, out);
}

TEST_F(CmpneTest, cmpne16Eq) {
    uint16_t op1 = 258;
    uint16_t op2 = 258;
    uint16_t out = 0;

    ir->add_store_16(ir,
                    ir->add_cmpne_16(ir,
                                  ir->add_mov_const_16(ir, op1),
                                  ir->add_mov_const_16(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((op1!=op2?~0:0) & 0xffff, out);
}

TEST_F(CmpneTest, cmpne16Ne) {
    uint16_t op1 = 258;
    uint16_t op2 = 6;
    uint16_t out = 0;

    ir->add_store_16(ir,
                    ir->add_cmpne_16(ir,
                                  ir->add_mov_const_16(ir, op1),
                                  ir->add_mov_const_16(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((op1!=op2?~0:0) & 0xffff, out);
}

TEST_F(CmpneTest, cmpne32Eq) {
    uint32_t op1 = 1000000;
    uint32_t op2 = 1000000;
    uint32_t out = 0;

    ir->add_store_32(ir,
                    ir->add_cmpne_32(ir,
                                  ir->add_mov_const_32(ir, op1),
                                  ir->add_mov_const_32(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((op1!=op2?~0:0) & 0xffffffff, out);
}

TEST_F(CmpneTest, cmpne32Ne) {
    uint32_t op1 = 258;
    uint32_t op2 = 1000000;
    uint32_t out = 0;

    ir->add_store_32(ir,
                    ir->add_cmpne_32(ir,
                                  ir->add_mov_const_32(ir, op1),
                                  ir->add_mov_const_32(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((op1!=op2?~0:0) & 0xffffffff, out);
}

TEST_F(CmpneTest, cmpne64Eq) {
    uint64_t op1 = ~0UL;
    uint64_t op2 = ~0UL;
    uint64_t out = 0;

    ir->add_store_64(ir,
                    ir->add_cmpne_64(ir,
                                  ir->add_mov_const_64(ir, op1),
                                  ir->add_mov_const_64(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((op1!=op2?~0:0), out);
}

TEST_F(CmpneTest, cmpne64Ne) {
    uint64_t op1 = 131313321;
    uint64_t op2 = 1000000;
    uint64_t out = 0;

    ir->add_store_64(ir,
                    ir->add_cmpne_64(ir,
                                  ir->add_mov_const_64(ir, op1),
                                  ir->add_mov_const_64(ir, op2)),
                    ir->add_mov_const_64(ir, (uint64_t) &out));
    jitAndExcecute();

    EXPECT_EQ((op1!=op2?~0:0), out);
}

