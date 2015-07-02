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
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __JITTER_TYPES__
#define __JITTER_TYPES__ 1

/* define register type and size */
enum irRegisterType {
    IR_REG_8,
    IR_REG_16,
    IR_REG_32,
    IR_REG_64,
    IR_LAST_REG_TYPE,
};

/* virtual register structure */
struct irRegister {
    enum irRegisterType type;
    int index;
    int firstWriteIndex;
    int lastReadIndex;
    void *backend;
};

/* cast type info for cast instruction */
enum irCastType {
    IR_CAST_8U_TO_16, IR_CAST_8U_TO_32, IR_CAST_8U_TO_64,
    IR_CAST_8S_TO_16, IR_CAST_8S_TO_32, IR_CAST_8S_TO_64,
    IR_CAST_16U_TO_32, IR_CAST_16U_TO_64,
    IR_CAST_16S_TO_32, IR_CAST_16S_TO_64,
    IR_CAST_32U_TO_64,
    IR_CAST_32S_TO_64,
    IR_CAST_64_TO_8, IR_CAST_32_TO_8, IR_CAST_16_TO_8,
    IR_CAST_64_TO_16, IR_CAST_32_TO_16,
    IR_CAST_64_TO_32,
};

/* binary type info for binary instruction */
enum irBinopType {
    IR_BINOP_ADD_8, IR_BINOP_ADD_16, IR_BINOP_ADD_32, IR_BINOP_ADD_64,
    IR_BINOP_SUB_8, IR_BINOP_SUB_16, IR_BINOP_SUB_32, IR_BINOP_SUB_64,
    IR_BINOP_XOR_8, IR_BINOP_XOR_16, IR_BINOP_XOR_32, IR_BINOP_XOR_64,
    IR_BINOP_AND_8, IR_BINOP_AND_16, IR_BINOP_AND_32, IR_BINOP_AND_64,
    IR_BINOP_OR_8, IR_BINOP_OR_16, IR_BINOP_OR_32, IR_BINOP_OR_64,
    IR_BINOP_SHL_8, IR_BINOP_SHL_16, IR_BINOP_SHL_32, IR_BINOP_SHL_64,
    IR_BINOP_SHR_8, IR_BINOP_SHR_16, IR_BINOP_SHR_32, IR_BINOP_SHR_64,
    IR_BINOP_ASR_8, IR_BINOP_ASR_16, IR_BINOP_ASR_32, IR_BINOP_ASR_64,
    IR_BINOP_ROR_8, IR_BINOP_ROR_16, IR_BINOP_ROR_32, IR_BINOP_ROR_64,
    IR_BINOP_CMPEQ_8, IR_BINOP_CMPEQ_16, IR_BINOP_CMPEQ_32, IR_BINOP_CMPEQ_64,
    IR_BINOP_CMPNE_8, IR_BINOP_CMPNE_16, IR_BINOP_CMPNE_32, IR_BINOP_CMPNE_64,
};

/* list of supported instructions */
enum irInstructionType {
    IR_MOV_CONST_8, IR_MOV_CONST_16, IR_MOV_CONST_32, IR_MOV_CONST_64,
    IR_LOAD_8, IR_LOAD_16, IR_LOAD_32, IR_LOAD_64,
    IR_STORE_8, IR_STORE_16, IR_STORE_32, IR_STORE_64,
    IR_ITE_8, IR_ITE_16, IR_ITE_32, IR_ITE_64,
    IR_READ_8, IR_READ_16, IR_READ_32, IR_READ_64,
    IR_WRITE_8, IR_WRITE_16, IR_WRITE_32, IR_WRITE_64,
    IR_CALL_VOID, IR_CALL_8, IR_CALL_16, IR_CALL_32, IR_CALL_64,
    IR_BINOP, IR_CAST, IR_EXIT,
    IR_LAST_INTRUCTION_TYPE,
};

/* hold an instruction description */
struct irInstruction {
    enum irInstructionType type;
    union {
        struct {
            struct irRegister *dst;
            uint64_t value;
        } mov;
        struct {
            struct irRegister *dst;
            struct irRegister *address;
        } load;
        struct {
            struct irRegister *src;
            struct irRegister *address;
        } store;
        struct {
            enum irBinopType type;
            struct irRegister *dst;
            struct irRegister *op1;
            struct irRegister *op2;
        } binop;
        struct {
            struct irRegister *address;
            char *name;
            struct irRegister *param[4];
            struct irRegister *result;
        } call;
        struct {
            struct irRegister *dst;
            struct irRegister *pred;
            struct irRegister *trueOp;
            struct irRegister *falseOp;
        } ite;
        struct {
            enum irCastType type;
            struct irRegister *dst;
            struct irRegister *op;
        } cast;
        /* if predicate is true then sequence of insn will return value */
        struct {
            struct irRegister *value;
            struct irRegister *pred;
        } exit;
        struct {
            struct irRegister *dst;
            int32_t offset;
        } read_context;
        struct {
            struct irRegister *src;
            int32_t offset;
        } write_context;
    } u;
};

#endif

#ifdef __cplusplus
}
#endif
