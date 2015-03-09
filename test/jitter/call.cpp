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

class CallTest : public jitterFixture {
};

#define P032                    0x12341230
#define P132                    0x12341231

#define P064                    0xDEADBEEF00003210UL
#define P164                    0xDEADBEEF00003211UL
#define P264                    0xDEADBEEF00003212UL
#define P364                    0xDEADBEEF00003213UL

#define CALL_VOID_MARKER            0x11
#define CALL_VOID_MARKER_P032       0x12
#define CALL_VOID_MARKER_P064       0x13
#define CALL_32_MARKER              0x21
#define CALL_32_MARKER_P032         0x22
#define CALL_32_MARKER_P032_P132    0x23
#define CALL_64_MARKER              0x31
#define CALL_64_MARKER_P064         0x32
#define CALL_64_MARKER_P064_P164    0x33
#define CALL_64_MARKER_P064_P164_P264       0x34
#define CALL_64_MARKER_P064_P164_P264_P364  0x35

/* call helpers */
extern "C" void call_void_helper(uint64_t context)
{
    char *res = (char *) context;
    
    res[0] = CALL_VOID_MARKER;
}

extern "C" void call_void_p032_helper(uint64_t context, uint32_t p0)
{
    char *res = (char *) context;
    
    if (p0 == P032)
        res[0] = CALL_VOID_MARKER_P032;
}

extern "C" void call_void_p064_helper(uint64_t context, uint64_t p0)
{
    char *res = (char *) context;
    
    if (p0 == P064)
        res[0] = CALL_VOID_MARKER_P064;
}

extern "C" uint32_t call_32_helper(uint64_t context)
{
    return CALL_32_MARKER;
}

extern "C" uint32_t call_32_p032_helper(uint64_t context, uint32_t p0)
{
    if (p0 == P032)
        return CALL_32_MARKER_P032;

    return 0;
}

extern "C" uint32_t call_32_p032_p132_helper(uint64_t context, uint32_t p0, uint32_t p1)
{
    if (p0 == P032 && p1 == P132)
        return CALL_32_MARKER_P032_P132;

    return 0;
}

extern "C" uint32_t call_64_helper(uint64_t context)
{
    return CALL_64_MARKER;
}

extern "C" uint64_t call_64_p064_helper(uint64_t context, uint64_t p0)
{
    if (p0 == P064)
        return CALL_64_MARKER_P064;

    return 0;
}

extern "C" uint64_t call_64_p064_p164_helper(uint64_t context, uint64_t p0, uint64_t p1)
{
    if (p0 == P064 && p1 == P164)
        return CALL_64_MARKER_P064_P164;

    return 0;
}

extern "C" uint64_t call_64_p064_p164_p264_helper(uint64_t context, uint64_t p0, uint64_t p1, uint64_t p2)
{
    if (p0 == P064 && p1 == P164 && p2 == P264)
        return CALL_64_MARKER_P064_P164_P264;

    return 0;
}

extern "C" uint64_t call_64_p064_p164_p264_p364_helper(uint64_t context, uint64_t p0, uint64_t p1, uint64_t p2, uint64_t p3)
{
    if (p0 == P064 && p1 == P164 && p2 == P264 && p3 == P364)
        return CALL_64_MARKER_P064_P164_P264_P364;

    return 0;
}

/* testes */
TEST_F(CallTest, call_void) {
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};

    contextBuffer[0] = 0;

    ir->add_call_void(ir, (char *) "call_void_0",
                      ir->add_mov_const_64(ir, (uint64_t) call_void_helper),
                      param);
    jitAndExcecute();

    EXPECT_EQ(contextBuffer[0], CALL_VOID_MARKER);
}

TEST_F(CallTest, call_void_p032) {
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};

    contextBuffer[0] = 0;

    param[0] = ir->add_mov_const_32(ir, P032);
    ir->add_call_void(ir, (char *) "call_void_p032",
                      ir->add_mov_const_64(ir, (uint64_t) call_void_p032_helper),
                      param);
    jitAndExcecute();

    EXPECT_EQ(contextBuffer[0], CALL_VOID_MARKER_P032);
}

TEST_F(CallTest, call_void_p064) {
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};

    contextBuffer[0] = 0;

    param[0] = ir->add_mov_const_64(ir, P064);
    ir->add_call_void(ir, (char *) "call_void_p064",
                      ir->add_mov_const_64(ir, (uint64_t) call_void_p064_helper),
                      param);
    jitAndExcecute();

    EXPECT_EQ(contextBuffer[0], CALL_VOID_MARKER_P064);
}

/*TEST_F(CallTest, call_32) {
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};
    uint32_t out;

    ir->add_store_32(ir,
                    ir->add_call_32(ir, (char *) "call_32_0",
                                    ir->add_mov_const_64(ir, (uint64_t) call_32_helper),
                                    param),
                    ir->add_mov_const_64(ir, (uint64_t) &out));

    jitAndExcecute();

    EXPECT_EQ(out, CALL_32_MARKER);
}*/

TEST_F(CallTest, call_32_p032) {
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};
    uint32_t out;

    param[0] = ir->add_mov_const_32(ir, P032);
    ir->add_store_32(ir,
                    ir->add_call_32(ir, (char *) "call_32_p032",
                                    ir->add_mov_const_64(ir, (uint64_t) call_32_p032_helper),
                                    param),
                    ir->add_mov_const_64(ir, (uint64_t) &out));

    jitAndExcecute();

    EXPECT_EQ(out, CALL_32_MARKER_P032);
}

TEST_F(CallTest, call_32_p032_p132) {
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};
    uint32_t out;

    param[0] = ir->add_mov_const_32(ir, P032);
    param[1] = ir->add_mov_const_32(ir, P132);
    ir->add_store_32(ir,
                    ir->add_call_32(ir, (char *) "call_32_p032_p132",
                                    ir->add_mov_const_64(ir, (uint64_t) call_32_p032_p132_helper),
                                    param),
                    ir->add_mov_const_64(ir, (uint64_t) &out));

    jitAndExcecute();

    EXPECT_EQ(out, CALL_32_MARKER_P032_P132);
}

/*TEST_F(CallTest, call_64) {
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};
    uint64_t out;

    ir->add_store_64(ir,
                    ir->add_call_64(ir, (char *) "call_64_0",
                                    ir->add_mov_const_64(ir, (uint64_t) call_64_helper),
                                    param),
                    ir->add_mov_const_64(ir, (uint64_t) &out));

    jitAndExcecute();

    EXPECT_EQ(out, CALL_64_MARKER);
}*/

TEST_F(CallTest, call_64_p064) {
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};
    uint64_t out;

    param[0] = ir->add_mov_const_64(ir, P064);
    ir->add_store_64(ir,
                    ir->add_call_64(ir, (char *) "call_64_p064",
                                    ir->add_mov_const_64(ir, (uint64_t) call_64_p064_helper),
                                    param),
                    ir->add_mov_const_64(ir, (uint64_t) &out));

    jitAndExcecute();

    EXPECT_EQ(out, CALL_64_MARKER_P064);
}

TEST_F(CallTest, call_64_p064_p164) {
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};
    uint64_t out;

    param[0] = ir->add_mov_const_64(ir, P064);
    param[1] = ir->add_mov_const_64(ir, P164);
    ir->add_store_64(ir,
                    ir->add_call_64(ir, (char *) "call_64_p064_p164",
                                    ir->add_mov_const_64(ir, (uint64_t) call_64_p064_p164_helper),
                                    param),
                    ir->add_mov_const_64(ir, (uint64_t) &out));

    jitAndExcecute();

    EXPECT_EQ(out, CALL_64_MARKER_P064_P164);
}

TEST_F(CallTest, call_64_p064_p164_p264) {
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};
    uint64_t out;

    param[0] = ir->add_mov_const_64(ir, P064);
    param[1] = ir->add_mov_const_64(ir, P164);
    param[2] = ir->add_mov_const_64(ir, P264);
    ir->add_store_64(ir,
                    ir->add_call_64(ir, (char *) "call_64_p064_p164_p264",
                                    ir->add_mov_const_64(ir, (uint64_t) call_64_p064_p164_p264_helper),
                                    param),
                    ir->add_mov_const_64(ir, (uint64_t) &out));

    jitAndExcecute();

    EXPECT_EQ(out, CALL_64_MARKER_P064_P164_P264);
}

TEST_F(CallTest, call_64_p064_p164_p264_p364) {
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};
    uint64_t out;

    param[0] = ir->add_mov_const_64(ir, P064);
    param[1] = ir->add_mov_const_64(ir, P164);
    param[2] = ir->add_mov_const_64(ir, P264);
    param[3] = ir->add_mov_const_64(ir, P364);
    ir->add_store_64(ir,
                    ir->add_call_64(ir, (char *) "call_64_p064_p164_p264_p364",
                                    ir->add_mov_const_64(ir, (uint64_t) call_64_p064_p164_p264_p364_helper),
                                    param),
                    ir->add_mov_const_64(ir, (uint64_t) &out));

    jitAndExcecute();

    EXPECT_EQ(out, CALL_64_MARKER_P064_P164_P264_P364);
}

