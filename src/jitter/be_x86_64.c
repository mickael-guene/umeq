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

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <ucontext.h>
#include "be_x86_64.h"
#include "be_x86_64_private.h"

//#define DEBUG_REG_ALLOC 1

#define container_of(ptr, type, member) ({          \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type,member) );})

#define REG_NUMBER  8

struct x86Register {
    int isConstant;
    int index;
    int firstWriteIndex;
    int lastReadIndex;
};

enum x86BinopType {
    X86_BINOP_ADD,
    X86_BINOP_SUB,
    X86_BINOP_XOR,
    X86_BINOP_AND,
    X86_BINOP_OR,
    X86_BINOP_SHL,
    X86_BINOP_SHR,
    X86_BINOP_ASR,
    X86_BINOP_ROR,
    X86_BINOP_CMPEQ,
    X86_BINOP_CMPNE
};

enum x86InstructionType {
    X86_MOV_CONST,
    X86_LOAD_8, X86_LOAD_16, X86_LOAD_32, X86_LOAD_64,
    X86_STORE_8, X86_STORE_16, X86_STORE_32, X86_STORE_64,
    X86_BINOP_8, X86_BINOP_16, X86_BINOP_32, X86_BINOP_64,
    X86_ITE,
    X86_CAST,
    X86_EXIT,
    X86_CALL,
    X86_READ_8, X86_READ_16, X86_READ_32, X86_READ_64,
    X86_WRITE_8, X86_WRITE_16, X86_WRITE_32, X86_WRITE_64,
    X86_INSN_MARKER
};

struct x86Instruction {
    enum x86InstructionType type;
    union {
        struct {
            struct x86Register *dst;
            uint64_t value;
        } mov;
        struct {
            struct x86Register *dst;
            struct x86Register *address;
        } load;
        struct {
            struct x86Register *src;
            struct x86Register *address;
        } store;
        struct {
            struct x86Register *value;
            struct x86Register *pred;
            int is_patchable;
        } exit;
        struct {
            enum x86BinopType type;
            struct x86Register *dst;
            struct x86Register *op1;
            struct x86Register *op2;
        } binop;
        struct {
            struct x86Register *dst;
            struct x86Register *pred;
            struct x86Register *trueOp;
            struct x86Register *falseOp;
        } ite;
        struct {
            enum irCastType type;
            struct x86Register *dst;
            struct x86Register *op;
        } cast;
        struct {
            struct x86Register *address;
            struct x86Register *param[4];
            struct x86Register *result;
        } call;
        struct {
            struct x86Register *dst;
            int32_t offset;
        } read_context;
        struct {
            struct x86Register *src;
            int32_t offset;
        } write_context;
        struct {
            uint32_t value;
        } marker;
    } u;
};

struct memoryPool {
    void *(*alloc)(struct memoryPool *pool, int size);
    void *(*reset)(struct memoryPool *pool);
    char *buffer;
    int index;
    int size;
};

struct inter {
    uint64_t restore_sp;
    struct backend backend;
    struct memoryPool registerPoolAllocator;
    struct memoryPool instructionPoolAllocator;
    int regIndex;
    int instructionIndex;
};

/* pool */
static void *memoryPoolAlloc(struct memoryPool *pool, int size)
{
    void *res = NULL;

    if (size + pool->index <= pool->size) {
        res = &pool->buffer[pool->index];
        pool->index += size;
    }
    assert(res != NULL);

    return res;
}

static void *memoryPoolReset(struct memoryPool *pool)
{
    pool->index = 0;

    return NULL;
}

static void memoryPoolInit(struct memoryPool *pool, char *buffer, int size)
{
    pool->buffer = buffer;
    pool->size = size;
}

/* register allocation */
static struct x86Register *allocateRegister(struct inter *inter, struct irRegister *irReg)
{
    struct x86Register *res;
    struct memoryPool *pool = (struct memoryPool *) &inter->registerPoolAllocator;

    if (irReg && irReg->backend) {
        res = irReg->backend;
    } else {
        res = (struct x86Register *) pool->alloc(pool, sizeof(struct x86Register));
        res->isConstant = 0;
        res->index = inter->regIndex++;
        res->firstWriteIndex = inter->instructionIndex;
        res->lastReadIndex = -1;
        if (irReg)
            irReg->backend = res;
    }

    return res;
}

/* instructions allocation */
static void add_mov_const(struct inter *inter, struct x86Register *dst, uint64_t value)
{
    struct memoryPool *pool = &inter->instructionPoolAllocator;
    struct x86Instruction *insn = (struct x86Instruction *) pool->alloc(pool, sizeof(struct x86Instruction));

    insn->type = X86_MOV_CONST;
    insn->u.mov.dst = dst;
    insn->u.mov.dst->isConstant = 1;
    insn->u.mov.value = value;

    inter->instructionIndex++;
}

static void add_load(struct inter *inter, enum x86InstructionType type, struct x86Register *dst, struct x86Register *address)
{
    struct memoryPool *pool = &inter->instructionPoolAllocator;
    struct x86Instruction *insn = (struct x86Instruction *) pool->alloc(pool, sizeof(struct x86Instruction));

    address->lastReadIndex = inter->instructionIndex;

    insn->type = type;
    insn->u.load.dst = dst;
    insn->u.load.address = address;

    inter->instructionIndex++;
}

static void add_store(struct inter *inter, enum x86InstructionType type, struct x86Register *src, struct x86Register *address)
{
    struct memoryPool *pool = &inter->instructionPoolAllocator;
    struct x86Instruction *insn = (struct x86Instruction *) pool->alloc(pool, sizeof(struct x86Instruction));

    address->lastReadIndex = inter->instructionIndex;
    src->lastReadIndex = inter->instructionIndex;

    insn->type = type;
    insn->u.store.src = src;
    insn->u.store.address = address;

    inter->instructionIndex++;
}

static void add_exit(struct inter *inter, struct x86Register *value, struct x86Register *pred)
{
    struct memoryPool *pool = &inter->instructionPoolAllocator;
    struct x86Instruction *insn = (struct x86Instruction *) pool->alloc(pool, sizeof(struct x86Instruction));

    value->lastReadIndex = inter->instructionIndex;
    if (pred)
        pred->lastReadIndex = inter->instructionIndex;

    insn->type = X86_EXIT;
    insn->u.exit.value = value;
    insn->u.exit.pred = pred;
    insn->u.exit.is_patchable = value->isConstant;

    inter->instructionIndex++;
}

static void add_binop(struct inter *inter, enum x86InstructionType type, enum x86BinopType subType, struct x86Register *dst, struct x86Register *op1, struct x86Register *op2)
{
    struct memoryPool *pool = &inter->instructionPoolAllocator;
    struct x86Instruction *insn = (struct x86Instruction *) pool->alloc(pool, sizeof(struct x86Instruction));

    op1->lastReadIndex = inter->instructionIndex;
    op2->lastReadIndex = inter->instructionIndex;

    insn->type = type;
    insn->u.binop.type = subType;
    insn->u.binop.dst = dst;
    insn->u.binop.op1 = op1;
    insn->u.binop.op2 = op2;

    inter->instructionIndex++;
}

static void add_ite(struct inter *inter, struct x86Register *dst, struct x86Register *pred, struct x86Register *trueOp, struct x86Register *falseOp)
{
    struct memoryPool *pool = &inter->instructionPoolAllocator;
    struct x86Instruction *insn = (struct x86Instruction *) pool->alloc(pool, sizeof(struct x86Instruction));

    pred->lastReadIndex = inter->instructionIndex;
    trueOp->lastReadIndex = inter->instructionIndex;
    falseOp->lastReadIndex = inter->instructionIndex;

    insn->type = X86_ITE;
    insn->u.ite.dst = dst;
    insn->u.ite.pred = pred;
    insn->u.ite.trueOp = trueOp;
    insn->u.ite.falseOp = falseOp;

    inter->instructionIndex++;
}

static void add_cast(struct inter *inter, enum irCastType type, struct x86Register *dst, struct x86Register *op)
{
    struct memoryPool *pool = &inter->instructionPoolAllocator;
    struct x86Instruction *insn = (struct x86Instruction *) pool->alloc(pool, sizeof(struct x86Instruction));

    op->lastReadIndex = inter->instructionIndex;

    insn->type = X86_CAST;
    insn->u.cast.type = type;
    insn->u.cast.dst = dst;
    insn->u.cast.op = op;

    inter->instructionIndex++;
}

static void add_call(struct inter *inter, struct x86Register *address, struct x86Register *params[4], struct x86Register *result)
{
    struct memoryPool *pool = &inter->instructionPoolAllocator;
    struct x86Instruction *insn = (struct x86Instruction *) pool->alloc(pool, sizeof(struct x86Instruction));
    int i;

    address->lastReadIndex = inter->instructionIndex;
    for(i = 0; i < 4; i++) {
        if (params[i])
            params[i]->lastReadIndex = inter->instructionIndex;
    }

    insn->type = X86_CALL;
    insn->u.call.address = address;
    for(i = 0; i < 4; i++)
        insn->u.call.param[i] = params[i];
    insn->u.call.result = result;

    inter->instructionIndex++;
}

static void add_read(struct inter *inter, enum x86InstructionType type, struct x86Register *dst, int32_t offset)
{
    struct memoryPool *pool = &inter->instructionPoolAllocator;
    struct x86Instruction *insn = (struct x86Instruction *) pool->alloc(pool, sizeof(struct x86Instruction));

    insn->type = type;
    insn->u.read_context.dst = dst;
    insn->u.read_context.offset = offset;

    inter->instructionIndex++;
}

static void add_write(struct inter *inter, enum x86InstructionType type, struct x86Register *src, int32_t offset)
{
    struct memoryPool *pool = &inter->instructionPoolAllocator;
    struct x86Instruction *insn = (struct x86Instruction *) pool->alloc(pool, sizeof(struct x86Instruction));

    src->lastReadIndex = inter->instructionIndex;
    insn->type = type;
    insn->u.write_context.src = src;
    insn->u.write_context.offset = offset;

    inter->instructionIndex++;
}

static void add_insn_start_marker(struct inter *inter, uint32_t value)
{
    struct memoryPool *pool = &inter->instructionPoolAllocator;
    struct x86Instruction *insn = (struct x86Instruction *) pool->alloc(pool, sizeof(struct x86Instruction));

    insn->type = X86_INSN_MARKER;
    insn->u.marker.value = value;

    inter->instructionIndex++;
}

static void allocateInstructions(struct inter *inter, struct irInstruction *irArray, int irInsnNb)
{
    int i;
    int goto_p0 = 0;

    for (i = 0; i < irInsnNb; ++i)
    {
        struct irInstruction *insn = &irArray[i];

        switch(insn->type) {
            case IR_MOV_CONST_8:
            case IR_MOV_CONST_16:
            case IR_MOV_CONST_32:
            case IR_MOV_CONST_64:
                add_mov_const(inter, allocateRegister(inter, insn->u.mov.dst), insn->u.mov.value);
                break;
            case IR_LOAD_8:
            case IR_LOAD_16:
            case IR_LOAD_32:
            case IR_LOAD_64:
                add_load(inter, X86_LOAD_8 + insn->type - IR_LOAD_8, allocateRegister(inter, insn->u.load.dst), allocateRegister(inter, insn->u.load.address));
                break;
            case IR_STORE_8:
            case IR_STORE_16:
            case IR_STORE_32:
            case IR_STORE_64:
                add_store(inter, X86_STORE_8 + insn->type - IR_STORE_8, allocateRegister(inter, insn->u.store.src), allocateRegister(inter, insn->u.store.address));
                break;
            case IR_BINOP:
                {
                    switch(insn->u.binop.type) {
                        case IR_BINOP_ADD_8: case IR_BINOP_ADD_16: case IR_BINOP_ADD_32: case IR_BINOP_ADD_64:
                            add_binop(inter, X86_BINOP_8 + insn->u.binop.type - IR_BINOP_ADD_8, X86_BINOP_ADD, allocateRegister(inter, insn->u.binop.dst), allocateRegister(inter, insn->u.binop.op1), allocateRegister(inter, insn->u.binop.op2));
                            break;
                        case IR_BINOP_SUB_8: case IR_BINOP_SUB_16: case IR_BINOP_SUB_32: case IR_BINOP_SUB_64:
                            add_binop(inter, X86_BINOP_8 + insn->u.binop.type - IR_BINOP_SUB_8, X86_BINOP_SUB, allocateRegister(inter, insn->u.binop.dst), allocateRegister(inter, insn->u.binop.op1), allocateRegister(inter, insn->u.binop.op2));
                            break;
                        case IR_BINOP_XOR_8: case IR_BINOP_XOR_16: case IR_BINOP_XOR_32: case IR_BINOP_XOR_64:
                            add_binop(inter, X86_BINOP_8 + insn->u.binop.type - IR_BINOP_XOR_8, X86_BINOP_XOR, allocateRegister(inter, insn->u.binop.dst), allocateRegister(inter, insn->u.binop.op1), allocateRegister(inter, insn->u.binop.op2));
                            break;
                        case IR_BINOP_AND_8: case IR_BINOP_AND_16: case IR_BINOP_AND_32: case IR_BINOP_AND_64:
                            add_binop(inter, X86_BINOP_8 + insn->u.binop.type - IR_BINOP_AND_8, X86_BINOP_AND, allocateRegister(inter, insn->u.binop.dst), allocateRegister(inter, insn->u.binop.op1), allocateRegister(inter, insn->u.binop.op2));
                            break;
                        case IR_BINOP_OR_8: case IR_BINOP_OR_16: case IR_BINOP_OR_32: case IR_BINOP_OR_64:
                            add_binop(inter, X86_BINOP_8 + insn->u.binop.type - IR_BINOP_OR_8, X86_BINOP_OR, allocateRegister(inter, insn->u.binop.dst), allocateRegister(inter, insn->u.binop.op1), allocateRegister(inter, insn->u.binop.op2));
                            break;
                        case IR_BINOP_SHL_8: case IR_BINOP_SHL_16: case IR_BINOP_SHL_32: case IR_BINOP_SHL_64:
                            add_binop(inter, X86_BINOP_8 + insn->u.binop.type - IR_BINOP_SHL_8, X86_BINOP_SHL, allocateRegister(inter, insn->u.binop.dst), allocateRegister(inter, insn->u.binop.op1), allocateRegister(inter, insn->u.binop.op2));
                            break;
                        case IR_BINOP_SHR_8: case IR_BINOP_SHR_16: case IR_BINOP_SHR_32: case IR_BINOP_SHR_64:
                            add_binop(inter, X86_BINOP_8 + insn->u.binop.type - IR_BINOP_SHR_8, X86_BINOP_SHR, allocateRegister(inter, insn->u.binop.dst), allocateRegister(inter, insn->u.binop.op1), allocateRegister(inter, insn->u.binop.op2));
                            break;
                        case IR_BINOP_ASR_8: goto_p0 = 56; goto binop_asr;
                        case IR_BINOP_ASR_16: goto_p0 = 48; goto binop_asr;
                        case IR_BINOP_ASR_32: goto_p0 = 32; goto binop_asr;
                        case IR_BINOP_ASR_64: goto_p0 = 0; goto binop_asr;
                            binop_asr: {
                                struct x86Register *dst = allocateRegister(inter, insn->u.binop.dst);
                                struct x86Register *op1 = allocateRegister(inter, insn->u.binop.op1);
                                struct x86Register *op2 = allocateRegister(inter, insn->u.binop.op2);
                                struct x86Register *shiftValue;
                                struct x86Register *shiftLeftResult;
                                struct x86Register *shiftRightResult;

                                if (goto_p0) {
                                    shiftValue = allocateRegister(inter, NULL);
                                    add_mov_const(inter, shiftValue, goto_p0);
                                    shiftLeftResult = allocateRegister(inter, NULL);
                                    add_binop(inter, X86_BINOP_64, X86_BINOP_SHL, shiftLeftResult, op1, shiftValue);
                                    shiftRightResult = allocateRegister(inter, NULL);
                                    add_binop(inter, X86_BINOP_64, X86_BINOP_ASR, shiftRightResult, shiftLeftResult, shiftValue);
                                } else {
                                    shiftRightResult = allocateRegister(inter, insn->u.binop.op1);
                                }
                                add_binop(inter, X86_BINOP_8 + insn->u.binop.type - IR_BINOP_ASR_8, X86_BINOP_ASR, dst, shiftRightResult, op2);
                            }
                            break;
                        case IR_BINOP_ROR_8: case IR_BINOP_ROR_16: case IR_BINOP_ROR_32: case IR_BINOP_ROR_64:
                            add_binop(inter, X86_BINOP_8 + insn->u.binop.type - IR_BINOP_ROR_8, X86_BINOP_ROR, allocateRegister(inter, insn->u.binop.dst), allocateRegister(inter, insn->u.binop.op1), allocateRegister(inter, insn->u.binop.op2));
                            break;
                        case IR_BINOP_CMPEQ_8: case IR_BINOP_CMPEQ_16: case IR_BINOP_CMPEQ_32: case IR_BINOP_CMPEQ_64:
                            add_binop(inter, X86_BINOP_8 + insn->u.binop.type - IR_BINOP_CMPEQ_8, X86_BINOP_CMPEQ, allocateRegister(inter, insn->u.binop.dst), allocateRegister(inter, insn->u.binop.op1), allocateRegister(inter, insn->u.binop.op2));
                            break;
                        case IR_BINOP_CMPNE_8: case IR_BINOP_CMPNE_16: case IR_BINOP_CMPNE_32: case IR_BINOP_CMPNE_64:
                            add_binop(inter, X86_BINOP_8 + insn->u.binop.type - IR_BINOP_CMPNE_8, X86_BINOP_CMPNE, allocateRegister(inter, insn->u.binop.dst), allocateRegister(inter, insn->u.binop.op1), allocateRegister(inter, insn->u.binop.op2));
                            break;
                        default:
                            assert(0);
                    }
                }
                break;
            case IR_ITE_8: case IR_ITE_16: case IR_ITE_32: case IR_ITE_64:
                add_ite(inter, allocateRegister(inter, insn->u.ite.dst), allocateRegister(inter, insn->u.ite.pred), allocateRegister(inter, insn->u.ite.trueOp), allocateRegister(inter, insn->u.ite.falseOp));
                break;
            case IR_CAST:
                add_cast(inter, insn->u.cast.type, allocateRegister(inter, insn->u.cast.dst), allocateRegister(inter, insn->u.cast.op));
                break;
            case IR_EXIT:
                if (insn->u.exit.pred)
                    add_exit(inter, allocateRegister(inter, insn->u.exit.value), allocateRegister(inter, insn->u.exit.pred));
                else
                    add_exit(inter, allocateRegister(inter, insn->u.exit.value), NULL);
                break;
            case IR_CALL_VOID: case IR_CALL_8: case IR_CALL_16: case IR_CALL_32: case IR_CALL_64:
                {
                    struct x86Register *params[4];
                    struct x86Register *result;
                    int i;

                    for(i = 0; i < 4; i++) {
                        if (insn->u.call.param[i])
                            params[i] = allocateRegister(inter, insn->u.call.param[i]);
                        else
                            params[i] = NULL;
                    }
                    if (insn->u.call.result)
                        result = allocateRegister(inter, insn->u.call.result);
                    else
                        result = NULL;

                    add_call(inter, allocateRegister(inter, insn->u.call.address), params, result);
                }
                break;
            case IR_READ_8: case IR_READ_16: case IR_READ_32: case IR_READ_64:
                /* if register is never use then drop the read */
                if (insn->u.read_context.dst->lastReadIndex != -1)
                    add_read(inter, X86_READ_8 + insn->type - IR_READ_8, allocateRegister(inter, insn->u.read_context.dst), insn->u.read_context.offset);
                break;
            case IR_WRITE_8: case IR_WRITE_16: case IR_WRITE_32: case IR_WRITE_64:
                add_write(inter, X86_WRITE_8 + insn->type - IR_WRITE_8, allocateRegister(inter, insn->u.write_context.src), insn->u.write_context.offset);
                break;
            case IR_INSN_MARKER:
                add_insn_start_marker(inter, insn->u.marker.value);
                break;
            default:
                assert(0);
        }
    }
}

/* register allocation */
static int getFreeRegRaw(int *freeRegList)
{
    int i;
    for(i = 0; i < REG_NUMBER; i++) {
        if (freeRegList[i]) {
            freeRegList[i] = 0;
            return i;
        }
    }

    assert(0);
}

static void getFreeReg(int *freeRegList, struct x86Register *reg)
{
    reg->index = getFreeRegRaw(freeRegList);
    if (reg->lastReadIndex == -1)
        freeRegList[reg->index] = 1;
}

#ifdef DEBUG_REG_ALLOC
static void displayReg(struct x86Register *reg)
{
    if (reg)
        printf("R%d[%d->%d[", reg->index + 8, reg->firstWriteIndex, reg->lastReadIndex);
    else
        printf("NULL");
}
#endif

static void allocateRegisters(struct inter *inter)
{
    int i;
    struct x86Instruction *insn = (struct x86Instruction *) inter->instructionPoolAllocator.buffer;
    int freeRegList[REG_NUMBER] = {1, 1, 1, 1, 1, 1, 1, 1};

    for (i = 0; i < inter->instructionIndex; ++i, insn++)
    {
#ifdef DEBUG_REG_ALLOC
        printf("%2d (%2d): ", i, insn->type);
#endif
        switch(insn->type) {
            case X86_MOV_CONST:
                getFreeReg(freeRegList, insn->u.mov.dst);
#ifdef DEBUG_REG_ALLOC
                printf("mov_const ");
                displayReg(insn->u.mov.dst);
                printf(", 0x%08lx", insn->u.mov.value);
#endif
                break;
            case X86_LOAD_8:
            case X86_LOAD_16:
            case X86_LOAD_32:
            case X86_LOAD_64:
                getFreeReg(freeRegList, insn->u.load.dst);
                if (insn->u.load.address->lastReadIndex == i)
                    freeRegList[insn->u.load.address->index] = 1;
#ifdef DEBUG_REG_ALLOC
                printf("load ");
                displayReg(insn->u.load.dst);
                printf(", [");
                displayReg(insn->u.load.address);
                printf("]");
#endif
                break;
            case X86_STORE_8:
            case X86_STORE_16:
            case X86_STORE_32:
            case X86_STORE_64:
                if (insn->u.store.address->lastReadIndex == i)
                    freeRegList[insn->u.store.address->index] = 1;
                if (insn->u.store.src->lastReadIndex == i)
                    freeRegList[insn->u.store.src->index] = 1;
#ifdef DEBUG_REG_ALLOC
                printf("store_%d ", 1 << (insn->type - IR_STORE_8 + 3));
                printf("[");
                displayReg(insn->u.store.address);
                printf("], ");
                displayReg(insn->u.store.src);
#endif
                break;
            case X86_BINOP_8:
            case X86_BINOP_16:
            case X86_BINOP_32:
            case X86_BINOP_64:
                getFreeReg(freeRegList, insn->u.binop.dst);
                if (insn->u.binop.op1->lastReadIndex == i)
                    freeRegList[insn->u.binop.op1->index] = 1;
                if (insn->u.binop.op2->lastReadIndex == i)
                    freeRegList[insn->u.binop.op2->index] = 1;
#ifdef DEBUG_REG_ALLOC
                printf("binop ");
                displayReg(insn->u.binop.dst);
                printf(", ");
                displayReg(insn->u.binop.op1);
                printf(", ");
                displayReg(insn->u.binop.op2);
#endif
                break;
            case X86_EXIT:
                if (insn->u.exit.value->lastReadIndex == i)
                    freeRegList[insn->u.exit.value->index] = 1;
                if (insn->u.exit.pred && insn->u.exit.pred->lastReadIndex == i)
                    freeRegList[insn->u.exit.pred->index] = 1;
#ifdef DEBUG_REG_ALLOC
                printf("exit ");
                displayReg(insn->u.exit.value);
                printf(" if ");
                displayReg(insn->u.exit.pred);
#endif
                break;
            case X86_ITE:
                getFreeReg(freeRegList, insn->u.ite.dst);
                if (insn->u.ite.pred->lastReadIndex == i)
                    freeRegList[insn->u.ite.pred->index] = 1;
                if (insn->u.ite.trueOp->lastReadIndex == i)
                    freeRegList[insn->u.ite.trueOp->index] = 1;
                if (insn->u.ite.falseOp->lastReadIndex == i)
                    freeRegList[insn->u.ite.falseOp->index] = 1;
#ifdef DEBUG_REG_ALLOC
                displayReg(insn->u.ite.dst);
                printf(" = ");
                displayReg(insn->u.ite.pred);
                printf("?");
                displayReg(insn->u.ite.trueOp);
                printf(":");
                displayReg(insn->u.ite.falseOp);
#endif
                break;
            case X86_CAST:
                getFreeReg(freeRegList, insn->u.cast.dst);
                if (insn->u.cast.op->lastReadIndex == i)
                    freeRegList[insn->u.cast.op->index] = 1;
#ifdef DEBUG_REG_ALLOC
                {
                const char *typeToString[] = {"(8u_to_16)", "(8u_to_32)", "(8u_to_64)",
                                              "(8s_to_16)", "(8s_to_32)", "(8s_to_64)",
                                              "(16u_to_32)", "(16u_to_64)",
                                              "(16s_to_32)", "(16s_to_64)",
                                              "(32u_to_64)", "(32s_to_64)",
                                              "(64_to_8)", "(32_to_8)", "(16_to_8)",
                                              "(64_to_16)", "(32_to_16)", "(64_to_32)"};
                displayReg(insn->u.cast.dst);
                printf(" = %s ", typeToString[insn->u.cast.type]);
                displayReg(insn->u.cast.op);
                }
#endif
                break;
            case X86_CALL:
                {
                    int j;

                    if (insn->u.call.result)
                        getFreeReg(freeRegList, insn->u.call.result);
                    if (insn->u.call.address->lastReadIndex == i)
                        freeRegList[insn->u.call.address->index] = 1;
                    for(j = 0; j < 4; j++)
                        if (insn->u.call.param[j] && insn->u.call.param[j]->lastReadIndex == i)
                            freeRegList[insn->u.call.param[j]->index] = 1;
#ifdef DEBUG_REG_ALLOC
                    printf("call (");
                    displayReg(insn->u.call.address);
                    printf("), [");
                    displayReg(insn->u.call.param[0]);
                    printf(",");
                    displayReg(insn->u.call.param[1]);
                    printf(",");
                    displayReg(insn->u.call.param[2]);
                    printf(",");
                    displayReg(insn->u.call.param[3]);
                    printf("] => ");
                    displayReg(insn->u.call.result);
#endif
                }
                break;
            case X86_READ_8: case X86_READ_16: case X86_READ_32: case X86_READ_64:
                {
                    getFreeReg(freeRegList, insn->u.read_context.dst);
#ifdef DEBUG_REG_ALLOC
                    printf("read_context ");
                    displayReg(insn->u.read_context.dst);
                    printf(", context[%d]",insn->u.read_context.offset);
#endif
                }
                break;
            case X86_WRITE_8: case X86_WRITE_16: case X86_WRITE_32: case X86_WRITE_64:
                {
                    if (insn->u.write_context.src->lastReadIndex == i)
                        freeRegList[insn->u.write_context.src->index] = 1;
#ifdef DEBUG_REG_ALLOC
                    printf("write_context ");
                    printf("context[%d], ",insn->u.write_context.offset);
                    displayReg(insn->u.write_context.src);
#endif
                }
                break;
            case X86_INSN_MARKER:
#ifdef DEBUG_REG_ALLOC
                printf("start_of_new_instruction\n");
#endif
                break;
            default:
                assert(0);
        }
#ifdef DEBUG_REG_ALLOC
    printf("\n");
    {
        int j;
        printf("    ");
        for(j=0;j<8;j++)
            printf("%d", freeRegList[j]);
        printf("\n");
    }
#endif
    }
}

/* code generation */
#define REX_OPCODE  0x40
//64 bit operand size
#define REX_W       0x08
// modrm reg field extension
#define REX_R       0x04
// sib index extension
#define REX_X       0x02
// modrm r/m field extension
#define REX_B       0x01

#define MODRM_MODE_0        0x00
#define MODRM_MODE_1        0x40
#define MODRM_MODE_2        0x80
#define MODRM_MODE_3        0xC0
#define MODRM_REG_SHIFT     3
#define MODRM_RM_SHIFT      0

#define SIB_SCALE_SHIFT     6
#define SIB_INDEX_SHIFT     3
#define SIB_BASE_SHIFT      0

static char *gen_mov_const_hlp(char *pos, struct x86Register *dst, uint64_t value)
{
    if (value) {
        *pos++ = REX_OPCODE | REX_W | REX_B;
        *pos++ = 0xb8 + dst->index;
        *pos++ = (value >> 0) & 0xff;
        *pos++ = (value >> 8) & 0xff;
        *pos++ = (value >> 16) & 0xff;
        *pos++ = (value >> 24) & 0xff;
        *pos++ = (value >> 32) & 0xff;
        *pos++ = (value >> 40) & 0xff;
        *pos++ = (value >> 48) & 0xff;
        *pos++ = (value >> 56) & 0xff;
    } else {
        *pos++ = REX_OPCODE | REX_R | REX_B;
        *pos++ = 0x31;
        *pos++ = 0xc0 | (dst->index << 3) | dst->index;
    }

    return pos;
}

static char *gen_mov_const(char *pos, struct x86Instruction *insn)
{
    return gen_mov_const_hlp(pos, insn->u.mov.dst, insn->u.mov.value);
}

static char *gen_load_8(char *pos, struct x86Instruction *insn)
{
    *pos++ = REX_OPCODE | REX_R | REX_B;
    *pos++ = 0x8a;
    *pos++ = MODRM_MODE_2 | (insn->u.load.dst->index << MODRM_REG_SHIFT) | 4; //address is sib+disp32
    *pos++ = (0 << SIB_SCALE_SHIFT) | (4 <<  SIB_INDEX_SHIFT) | (insn->u.load.address->index);
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;

    return pos;
}

static char *gen_load_16(char *pos, struct x86Instruction *insn)
{
    *pos++ = 0x66;
    *pos++ = REX_OPCODE | REX_R | REX_B;
    *pos++ = 0x8b;
    *pos++ = MODRM_MODE_2 | (insn->u.load.dst->index << MODRM_REG_SHIFT) | 4; //address is sib+disp32
    *pos++ = (0 << SIB_SCALE_SHIFT) | (4 <<  SIB_INDEX_SHIFT) | (insn->u.load.address->index);
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;

    return pos;
}

static char *gen_load_32(char *pos, struct x86Instruction *insn)
{
    *pos++ = REX_OPCODE | REX_R | REX_B;
    *pos++ = 0x8b;
    *pos++ = MODRM_MODE_2 | (insn->u.load.dst->index << MODRM_REG_SHIFT) | 4; //address is sib+disp32
    *pos++ = (0 << SIB_SCALE_SHIFT) | (4 <<  SIB_INDEX_SHIFT) | (insn->u.load.address->index);
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;

    return pos;
}

static char *gen_load_64(char *pos, struct x86Instruction *insn)
{
    *pos++ = REX_OPCODE | REX_R | REX_B | REX_W;
    *pos++ = 0x8b;
    *pos++ = MODRM_MODE_2 | (insn->u.load.dst->index << MODRM_REG_SHIFT) | 4; //address is sib+disp32
    *pos++ = (0 << SIB_SCALE_SHIFT) | (4 <<  SIB_INDEX_SHIFT) | (insn->u.load.address->index);
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;

    return pos;
}

static char *gen_store_8(char *pos, struct x86Instruction *insn)
{
    *pos++ = REX_OPCODE | REX_R | REX_B;
    *pos++ = 0x88;
    *pos++ = MODRM_MODE_2 | (insn->u.load.dst->index << MODRM_REG_SHIFT) | 4; //address is sib+disp32
    *pos++ = (0 << SIB_SCALE_SHIFT) | (4 <<  SIB_INDEX_SHIFT) | (insn->u.load.address->index);
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;

    return pos;
}

static char *gen_store_16(char *pos, struct x86Instruction *insn)
{
    *pos++ = 0x66;
    *pos++ = REX_OPCODE | REX_R | REX_B;
    *pos++ = 0x89;
    *pos++ = MODRM_MODE_2 | (insn->u.load.dst->index << MODRM_REG_SHIFT) | 4; //address is sib+disp32
    *pos++ = (0 << SIB_SCALE_SHIFT) | (4 <<  SIB_INDEX_SHIFT) | (insn->u.load.address->index);
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;

    return pos;
}

static char *gen_store_32(char *pos, struct x86Instruction *insn)
{
    *pos++ = REX_OPCODE | REX_R | REX_B;
    *pos++ = 0x89;
    *pos++ = MODRM_MODE_2 | (insn->u.load.dst->index << MODRM_REG_SHIFT) | 4; //address is sib+disp32
    *pos++ = (0 << SIB_SCALE_SHIFT) | (4 <<  SIB_INDEX_SHIFT) | (insn->u.load.address->index);
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;

    return pos;
}

static char *gen_store_64(char *pos, struct x86Instruction *insn)
{
    *pos++ = REX_OPCODE | REX_R | REX_B | REX_W;
    *pos++ = 0x89;
    *pos++ = MODRM_MODE_2 | (insn->u.load.dst->index << MODRM_REG_SHIFT) | 4; //address is sib+disp32
    *pos++ = (0 << SIB_SCALE_SHIFT) | (4 <<  SIB_INDEX_SHIFT) | (insn->u.load.address->index);
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;

    return pos;
}

static char *gen_move_reg_low(char *pos, int low_index, struct x86Register *src)
{
    /* mov low_index, src */
    *pos++ = REX_OPCODE | REX_B | REX_W;
    *pos++ = 0x8b;
    *pos++ = MODRM_MODE_3 | (low_index << MODRM_REG_SHIFT) | src->index;

    return pos;
}

static char *gen_move_reg_from_low(char *pos, int low_index, struct x86Register *dst)
{
    /* mov dst, low_index */
    *pos++ = REX_OPCODE | REX_R | REX_W;
    *pos++ = 0x8b;
    *pos++ = MODRM_MODE_3 | (dst->index << MODRM_REG_SHIFT) | low_index;

    return pos;
}

static char *gen_exit(char *pos, struct x86Instruction *insn)
{
    char *pos_start_offset = 0;
    char *pos_patch = NULL;

    if (insn->u.exit.pred) {
        //cmp pred with zero
        *pos++ = REX_OPCODE | REX_B | REX_W;
        *pos++ = 0x81;
        *pos++ = MODRM_MODE_3 | (7/*subcode*/ << MODRM_REG_SHIFT) | insn->u.exit.pred->index;
        *pos++ = 0;
        *pos++ = 0;
        *pos++ = 0;
        *pos++ = 0;
        //je after exit sequence
        *pos++ = 0x74;
        pos_patch = pos;
        *pos++ = 0;
        pos_start_offset = pos;
    }
    /* mov rax, value */
    pos = gen_move_reg_low(pos, 0/*rax*/, insn->u.exit.value);
    /* generate rdx */
    if (insn->u.exit.is_patchable) {
        /* return rip, lea rdx, [rip] */
        *pos++ = 0x48;
        *pos++ = 0x8d;
        *pos++ = 0x15;
        *pos++ = 0;
        *pos++ = 0;
        *pos++ = 0;
        *pos++ = 0;
    } else {
        *pos++ = REX_OPCODE;
        *pos++ = 0x31;
        *pos++ = 0xc0 | (2 << 3) | 2;
    }
    /*retq */
    *pos++ = 0xc3;
    /* setup patch area */
    if (insn->u.exit.is_patchable) {
        /* mov rax, imm64 */
        *pos++ = 0x48;
        *pos++ = 0xb8;
        pos+=8;
        /* jmp rax */
        *pos++ = 0xff;
        *pos++ = 0xe0;
        /* return rip, lea rdx, [rip] */
        *pos++ = 0x48;
        *pos++ = 0x8d;
        *pos++ = 0x15;
        *pos++ = 0;
        *pos++ = 0;
        *pos++ = 0;
        *pos++ = 0;
    }
    /* fixup jump target to go after retq */
    if (insn->u.exit.pred) {
        *pos_patch = (pos - pos_start_offset);
    }

    return pos;
}

static char *gen_move_reg(char *pos, struct x86Register *dst, struct x86Register *src)
{
    *pos++ = REX_OPCODE | REX_R | REX_B | REX_W;
    *pos++ = 0x8b;
    *pos++ = MODRM_MODE_3 | (dst->index << MODRM_REG_SHIFT) | src->index;

    return pos;
}

static char *gen_cmp(char *pos, int isEq, struct x86Register *dst, struct x86Register *op1, struct x86Register *op2)
{
    char *pos_patch;
    char *pos_ori;

    pos = gen_mov_const_hlp(pos, dst, ~0);
    //cmp op1 op2
    *pos++ = REX_OPCODE | REX_R | REX_B | REX_W;
    *pos++ = 0x3b;
    *pos++ = MODRM_MODE_3 | (op2->index << MODRM_REG_SHIFT) | op1->index;
    //je after next move
    *pos++ = (isEq)?0x74:0x75;
    pos_patch = pos;
    *pos++ = 0;
    // set false value
    pos_ori = pos;
    pos = gen_mov_const_hlp(pos, dst, 0);
    *pos_patch = pos - pos_ori;

    return pos;
}

static char *gen_binop(char *pos, struct x86Instruction *insn, uint64_t mask)
{
    static const char binopToOpcode[] = {0x01/*add*/, 0x29/*sub*/, 0x31/*xor*/, 0x21/*and*/,
                                         0x09/*or*/, 0xd3/*shl*/, 0xd3/*shr*/, 0xd3/*sar*/, 0xd3/*ror*/,
                                         0xff/*cmpeq*/, 0xff/*cmpne*/};
    char subtype = 0;

    switch(insn->u.binop.type) {
        case X86_BINOP_CMPEQ:
            pos = gen_cmp(pos, 1, insn->u.binop.dst, insn->u.binop.op1, insn->u.binop.op2);
            break;
        case X86_BINOP_CMPNE:
            pos = gen_cmp(pos, 0, insn->u.binop.dst, insn->u.binop.op1, insn->u.binop.op2);
            break;
        case X86_BINOP_SHL: subtype = 4; goto unop;
        case X86_BINOP_SHR: subtype = 5; goto unop;
        case X86_BINOP_ASR: subtype = 7; goto unop;
            unop: {
                pos = gen_move_reg_low(pos, 1/*cl*/, insn->u.binop.op2);
                pos = gen_move_reg(pos, insn->u.binop.dst, insn->u.binop.op1);
                *pos++ = REX_OPCODE | REX_B | REX_W;
                *pos++ = 0xd3;
                *pos++ = MODRM_MODE_3 | (subtype/*subcode*/ << MODRM_REG_SHIFT) | insn->u.binop.dst->index;
            }
            break;
        case X86_BINOP_ROR:
            pos = gen_move_reg_low(pos, 1/*cl*/, insn->u.binop.op2);
            pos = gen_move_reg(pos, insn->u.binop.dst, insn->u.binop.op1);
            if (insn->type == X86_BINOP_16)
                *pos++ = 0x66;
            *pos++ = REX_OPCODE | REX_B | (insn->type == X86_BINOP_64?REX_W:0);
            *pos++ = insn->type == X86_BINOP_8?0xd2:0xd3;
            *pos++ = MODRM_MODE_3 | (1/*subcode*/ << MODRM_REG_SHIFT) | insn->u.binop.dst->index;
            break;
        default:
            pos = gen_move_reg(pos, insn->u.binop.dst, insn->u.binop.op1);
            *pos++ = REX_OPCODE | REX_R | REX_B | REX_W;
            *pos++ = binopToOpcode[insn->u.binop.type];
            *pos++ = MODRM_MODE_3 | (insn->u.binop.op2->index << MODRM_REG_SHIFT) | insn->u.binop.dst->index;
    }
    //mask result
    if (mask) {
        /* moc cl, mask */
        *pos++ = REX_OPCODE | REX_W;
        *pos++ = 0xb8 + 1/*cl*/;
        *pos++ = (mask >> 0) & 0xff;
        *pos++ = (mask >> 8) & 0xff;
        *pos++ = (mask >> 16) & 0xff;
        *pos++ = (mask >> 24) & 0xff;
        *pos++ = (mask >> 32) & 0xff;
        *pos++ = (mask >> 40) & 0xff;
        *pos++ = (mask >> 48) & 0xff;
        *pos++ = (mask >> 56) & 0xff;
        /* and dst, dst, cl */
        *pos++ = REX_OPCODE | REX_B | REX_W;
        *pos++ = 0x21;
        *pos++ = MODRM_MODE_3 | (1/*cl*/ << MODRM_REG_SHIFT) | insn->u.binop.dst->index;
    }

    return pos;
}

static char *gen_ite(char *pos, struct x86Instruction *insn)
{
    pos = gen_move_reg(pos, insn->u.ite.dst, insn->u.ite.falseOp);
    //cmp pred with zero
    *pos++ = REX_OPCODE | REX_B | REX_W;
    *pos++ = 0x81;
    *pos++ = MODRM_MODE_3 | (7/*subcode*/ << MODRM_REG_SHIFT) | insn->u.ite.pred->index;
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;
    //cmovnz dst <= trueOp
    *pos++ = REX_OPCODE | REX_R | REX_B | REX_W;
    *pos++ = 0x0f;
    *pos++ = 0x45;
    *pos++ = MODRM_MODE_3 | (insn->u.ite.dst->index << MODRM_REG_SHIFT) | insn->u.ite.trueOp->index;

    return pos;
}

static char *gen_upper_unsigned_cast_hlp(char *pos, struct x86Register *dst, struct x86Register *op, uint64_t mask)
{
    pos = gen_mov_const_hlp(pos, dst, mask);
    *pos++ = REX_OPCODE | REX_R | REX_B | REX_W;
    *pos++ = 0x21; /* and */
    *pos++ = MODRM_MODE_3 | (op->index << MODRM_REG_SHIFT) | dst->index;

    return pos;
}

static char *gen_upper_signed_cast_hlp(char *pos, struct x86Register *dst, struct x86Register *op, char shift_value)
{
    pos = gen_move_reg(pos, dst, op);
    *pos++ = REX_OPCODE | REX_B | REX_W;
    *pos++ = 0xc1;
    *pos++ = MODRM_MODE_3 | (4/*subcode*/ << MODRM_REG_SHIFT) | dst->index; //shl dst, imm8
    *pos++ = shift_value;
    *pos++ = REX_OPCODE | REX_B | REX_W;
    *pos++ = 0xc1;
    *pos++ = MODRM_MODE_3 | (7/*subcode*/ << MODRM_REG_SHIFT) | dst->index; //sar dst, imm8
    *pos++ = shift_value;

    return pos;
}

static char *gen_cast(char *pos, struct x86Instruction *insn)
{
    switch(insn->u.cast.type) {
        case IR_CAST_8U_TO_16: case IR_CAST_8U_TO_32: case IR_CAST_8U_TO_64:
            pos = gen_upper_unsigned_cast_hlp(pos, insn->u.cast.dst, insn->u.cast.op, 0xff);
            break;
        case IR_CAST_16U_TO_32: case IR_CAST_16U_TO_64:
            pos = gen_upper_unsigned_cast_hlp(pos, insn->u.cast.dst, insn->u.cast.op, 0xffff);
            break;
        case IR_CAST_32U_TO_64:
            pos = gen_upper_unsigned_cast_hlp(pos, insn->u.cast.dst, insn->u.cast.op, 0xffffffff);
            break;
        case IR_CAST_8S_TO_16: case IR_CAST_8S_TO_32: case IR_CAST_8S_TO_64:
            pos = gen_upper_signed_cast_hlp(pos, insn->u.cast.dst, insn->u.cast.op, 56);
            break;
        case IR_CAST_16S_TO_32: case IR_CAST_16S_TO_64:
            pos = gen_upper_signed_cast_hlp(pos, insn->u.cast.dst, insn->u.cast.op, 48);
            break;
        case IR_CAST_32S_TO_64:
            pos = gen_upper_signed_cast_hlp(pos, insn->u.cast.dst, insn->u.cast.op, 32);
            break;
        case IR_CAST_64_TO_8: case IR_CAST_32_TO_8: case IR_CAST_16_TO_8:
            pos = gen_upper_unsigned_cast_hlp(pos, insn->u.cast.dst, insn->u.cast.op, 0xff);
            break;
        case IR_CAST_64_TO_16: case IR_CAST_32_TO_16:
            pos = gen_upper_unsigned_cast_hlp(pos, insn->u.cast.dst, insn->u.cast.op, 0xffff);
            break;
        case IR_CAST_64_TO_32:
            pos = gen_upper_unsigned_cast_hlp(pos, insn->u.cast.dst, insn->u.cast.op, 0xffffffff);
            break;
        default:
            assert(0);
    }

    return pos;
}

static char *gen_push_hlp(char *pos, int index)
{
    if (index >= 8)
        *pos++ = REX_OPCODE | REX_B | REX_W;
    else
        *pos++ = REX_OPCODE | REX_W;
    *pos++ = 0x50 + (index & 7);

    return pos;
}

static char *gen_pop_hlp(char *pos, int index)
{
    if (index >= 8)
        *pos++ = REX_OPCODE | REX_B | REX_W;
    else
        *pos++ = REX_OPCODE | REX_W;
    *pos++ = 0x58 + (index & 7);

    return pos;
}

static char *gen_call(char *pos, struct x86Instruction *insn)
{
    /* save caller regs we use */
    pos = gen_push_hlp(pos, 7/*rdi*/);
    pos = gen_push_hlp(pos, 8/*r8*/);
    pos = gen_push_hlp(pos, 9/*r9*/);
    pos = gen_push_hlp(pos, 10/*r10*/);
    pos = gen_push_hlp(pos, 11/*r11*/);
    pos = gen_push_hlp(pos, 12/*r12*/);/* only use to keep sp align on 16 bytes */

    /* mov address into rax */
    pos = gen_move_reg_low(pos, 0/*rax*/, insn->u.call.address);

    /* rdi already has context value */
    if (insn->u.call.param[2]) {
        pos = gen_move_reg_low(pos, 1/*rcx*/, insn->u.call.param[2]);
    }
    if (insn->u.call.param[1]) {
        pos = gen_move_reg_low(pos, 2/*rdx*/, insn->u.call.param[1]);
    }
    if (insn->u.call.param[0]) {
        pos = gen_move_reg_low(pos, 6/*rsi*/, insn->u.call.param[0]);
    }
    /* do r8 last in case it's use into others param */
    if (insn->u.call.param[3]) {
        struct x86Register dst;
        dst.index = 0;
        pos = gen_move_reg(pos, &dst/*r8*/, insn->u.call.param[3]);
    }

    /* call function by address */
    *pos++ = REX_OPCODE | REX_W;
    *pos++ = 0xff;
    *pos++ = MODRM_MODE_3 | (2/*subcode*/ << MODRM_REG_SHIFT) | 0/*rax*/;
    /* restore caller save regs */
    pos = gen_pop_hlp(pos, 12/*r12*/);/* only use to keep sp align on 16 bytes */
    pos = gen_pop_hlp(pos, 11/*r11*/);
    pos = gen_pop_hlp(pos, 10/*r10*/);
    pos = gen_pop_hlp(pos, 9/*r9*/);
    pos = gen_pop_hlp(pos, 8/*r8*/);
    pos = gen_pop_hlp(pos, 7/*rdi*/);

    /* move result if need */
    if (insn->u.call.result) {
        pos = gen_move_reg_from_low(pos, 0/*rax*/, insn->u.call.result);
    }

    return pos;
}

static char *gen_read_8(char *pos, struct x86Instruction *insn)
{
    *pos++ = REX_OPCODE | REX_R;
    *pos++ = 0x8a;
    *pos++ = MODRM_MODE_2 | (insn->u.read_context.dst->index << MODRM_REG_SHIFT) | 7; //address is rdi+disp32
    *pos++ = (insn->u.read_context.offset >> 0) & 0xff;
    *pos++ = (insn->u.read_context.offset >> 8) & 0xff;
    *pos++ = (insn->u.read_context.offset >> 16) & 0xff;
    *pos++ = (insn->u.read_context.offset >> 24) & 0xff;

    return pos;
}

static char *gen_read_16(char *pos, struct x86Instruction *insn)
{
    *pos++ = 0x66;
    *pos++ = REX_OPCODE | REX_R;
    *pos++ = 0x8b;
    *pos++ = MODRM_MODE_2 | (insn->u.read_context.dst->index << MODRM_REG_SHIFT) | 7; //address is rdi+disp32
    *pos++ = (insn->u.read_context.offset >> 0) & 0xff;
    *pos++ = (insn->u.read_context.offset >> 8) & 0xff;
    *pos++ = (insn->u.read_context.offset >> 16) & 0xff;
    *pos++ = (insn->u.read_context.offset >> 24) & 0xff;

    return pos;
}

static char *gen_read_32(char *pos, struct x86Instruction *insn)
{
    *pos++ = REX_OPCODE | REX_R;
    *pos++ = 0x8b;
    *pos++ = MODRM_MODE_2 | (insn->u.read_context.dst->index << MODRM_REG_SHIFT) | 7; //address is rdi+disp32
    *pos++ = (insn->u.read_context.offset >> 0) & 0xff;
    *pos++ = (insn->u.read_context.offset >> 8) & 0xff;
    *pos++ = (insn->u.read_context.offset >> 16) & 0xff;
    *pos++ = (insn->u.read_context.offset >> 24) & 0xff;

    return pos;
}

static char *gen_read_64(char *pos, struct x86Instruction *insn)
{
    *pos++ = REX_OPCODE | REX_R | REX_W;
    *pos++ = 0x8b;
    *pos++ = MODRM_MODE_2 | (insn->u.read_context.dst->index << MODRM_REG_SHIFT) | 7; //address is rdi+disp32
    *pos++ = (insn->u.read_context.offset >> 0) & 0xff;
    *pos++ = (insn->u.read_context.offset >> 8) & 0xff;
    *pos++ = (insn->u.read_context.offset >> 16) & 0xff;
    *pos++ = (insn->u.read_context.offset >> 24) & 0xff;

    return pos;
}

static char *gen_write_8(char *pos, struct x86Instruction *insn)
{
    *pos++ = REX_OPCODE | REX_R;
    *pos++ = 0x88;
    *pos++ = MODRM_MODE_2 | (insn->u.write_context.src->index << MODRM_REG_SHIFT) | 7; //address is rdi+disp32
    *pos++ = (insn->u.write_context.offset >> 0) & 0xff;
    *pos++ = (insn->u.write_context.offset >> 8) & 0xff;
    *pos++ = (insn->u.write_context.offset >> 16) & 0xff;
    *pos++ = (insn->u.write_context.offset >> 24) & 0xff;

    return pos;
}

static char *gen_write_16(char *pos, struct x86Instruction *insn)
{
    *pos++ = 0x66;
    *pos++ = REX_OPCODE | REX_R;
    *pos++ = 0x89;
    *pos++ = MODRM_MODE_2 | (insn->u.write_context.src->index << MODRM_REG_SHIFT) | 7; //address is rdi+disp32
    *pos++ = (insn->u.write_context.offset >> 0) & 0xff;
    *pos++ = (insn->u.write_context.offset >> 8) & 0xff;
    *pos++ = (insn->u.write_context.offset >> 16) & 0xff;
    *pos++ = (insn->u.write_context.offset >> 24) & 0xff;

    return pos;
}

static char *gen_write_32(char *pos, struct x86Instruction *insn)
{
    *pos++ = REX_OPCODE | REX_R;
    *pos++ = 0x89;
    *pos++ = MODRM_MODE_2 | (insn->u.write_context.src->index << MODRM_REG_SHIFT) | 7; //address is rdi+disp32
    *pos++ = (insn->u.write_context.offset >> 0) & 0xff;
    *pos++ = (insn->u.write_context.offset >> 8) & 0xff;
    *pos++ = (insn->u.write_context.offset >> 16) & 0xff;
    *pos++ = (insn->u.write_context.offset >> 24) & 0xff;

    return pos;
}

static char *gen_write_64(char *pos, struct x86Instruction *insn)
{
    *pos++ = REX_OPCODE | REX_R | REX_W;;
    *pos++ = 0x89;
    *pos++ = MODRM_MODE_2 | (insn->u.write_context.src->index << MODRM_REG_SHIFT) | 7; //address is rdi+disp32
    *pos++ = (insn->u.write_context.offset >> 0) & 0xff;
    *pos++ = (insn->u.write_context.offset >> 8) & 0xff;
    *pos++ = (insn->u.write_context.offset >> 16) & 0xff;
    *pos++ = (insn->u.write_context.offset >> 24) & 0xff;

    return pos;
}

static int generateCode(struct inter *inter, char *buffer)
{
    int i;
    struct x86Instruction *insn = (struct x86Instruction *) inter->instructionPoolAllocator.buffer;
    char *pos = buffer;
    uint64_t mask;

    for (i = 0; i < inter->instructionIndex; ++i, insn++)
    {
        switch(insn->type) {
            case X86_MOV_CONST:
                pos = gen_mov_const(pos, insn);
                break;
            case X86_LOAD_8:
                pos = gen_load_8(pos, insn);
                break;
            case X86_LOAD_16:
                pos = gen_load_16(pos, insn);
                break;
            case X86_LOAD_32:
                pos = gen_load_32(pos, insn);
                break;
            case X86_LOAD_64:
                pos = gen_load_64(pos, insn);
                break;
            case X86_STORE_8:
                pos = gen_store_8(pos, insn);
                break;
            case X86_STORE_16:
                pos = gen_store_16(pos, insn);
                break;
            case X86_STORE_32:
                pos = gen_store_32(pos, insn);
                break;
            case X86_STORE_64:
                pos = gen_store_64(pos, insn);
                break;
            case X86_BINOP_8: mask = 0xff; goto binop;
            case X86_BINOP_16: mask = 0xffff; goto binop;
            case X86_BINOP_32: mask = 0xffffffff; goto binop;
            case X86_BINOP_64: mask = 0; goto binop;
                binop:
                    pos = gen_binop(pos, insn, mask);
                break;
            case X86_EXIT:
                pos = gen_exit(pos, insn);
                break;
            case X86_ITE:
                pos = gen_ite(pos, insn);
                break;
            case X86_CAST:
                pos = gen_cast(pos, insn);
                break;
            case X86_CALL:
                pos = gen_call(pos, insn);
                break;
            case X86_READ_8:
                pos = gen_read_8(pos, insn);
                break;
            case X86_READ_16:
                pos = gen_read_16(pos, insn);
                break;
            case X86_READ_32:
                pos = gen_read_32(pos, insn);
                break;
            case X86_READ_64:
                pos = gen_read_64(pos, insn);
                break;
            case X86_WRITE_8:
                pos = gen_write_8(pos, insn);
                break;
            case X86_WRITE_16:
                pos = gen_write_16(pos, insn);
                break;
            case X86_WRITE_32:
                pos = gen_write_32(pos, insn);
                break;
            case X86_WRITE_64:
                pos = gen_write_64(pos, insn);
                break;
            default:
                assert(0);
        }
    }

    return pos - buffer;
}

static uint32_t findInsnInCode(struct inter *inter, char *buffer, int offset)
{
    int i;
    struct x86Instruction *insn = (struct x86Instruction *) inter->instructionPoolAllocator.buffer;
    char *pos = buffer;
    uint64_t mask;
    uint32_t res = ~0;

    for (i = 0; i < inter->instructionIndex; ++i, insn++)
    {
        switch(insn->type) {
            case X86_INSN_MARKER:
                res = insn->u.marker.value;
                break;
            case X86_MOV_CONST:
                pos = gen_mov_const(pos, insn);
                break;
            case X86_LOAD_8:
                pos = gen_load_8(pos, insn);
                break;
            case X86_LOAD_16:
                pos = gen_load_16(pos, insn);
                break;
            case X86_LOAD_32:
                pos = gen_load_32(pos, insn);
                break;
            case X86_LOAD_64:
                pos = gen_load_64(pos, insn);
                break;
            case X86_STORE_8:
                pos = gen_store_8(pos, insn);
                break;
            case X86_STORE_16:
                pos = gen_store_16(pos, insn);
                break;
            case X86_STORE_32:
                pos = gen_store_32(pos, insn);
                break;
            case X86_STORE_64:
                pos = gen_store_64(pos, insn);
                break;
            case X86_BINOP_8: mask = 0xff; goto binop;
            case X86_BINOP_16: mask = 0xffff; goto binop;
            case X86_BINOP_32: mask = 0xffffffff; goto binop;
            case X86_BINOP_64: mask = 0; goto binop;
                binop:
                    pos = gen_binop(pos, insn, mask);
                break;
            case X86_EXIT:
                pos = gen_exit(pos, insn);
                break;
            case X86_ITE:
                pos = gen_ite(pos, insn);
                break;
            case X86_CAST:
                pos = gen_cast(pos, insn);
                break;
            case X86_CALL:
                pos = gen_call(pos, insn);
                break;
            case X86_READ_8:
                pos = gen_read_8(pos, insn);
                break;
            case X86_READ_16:
                pos = gen_read_16(pos, insn);
                break;
            case X86_READ_32:
                pos = gen_read_32(pos, insn);
                break;
            case X86_READ_64:
                pos = gen_read_64(pos, insn);
                break;
            case X86_WRITE_8:
                pos = gen_write_8(pos, insn);
                break;
            case X86_WRITE_16:
                pos = gen_write_16(pos, insn);
                break;
            case X86_WRITE_32:
                pos = gen_write_32(pos, insn);
                break;
            case X86_WRITE_64:
                pos = gen_write_64(pos, insn);
                break;
            default:
                assert(0);
        }
        if (pos >= buffer + offset)
            break;
    }

    return res;
}

static void request_signal_alternate_exit(struct backend *backend, void *_ucp, uint64_t result)
{
    ucontext_t *ucp = (ucontext_t *) _ucp;

    /* let the kernel call restore_be_x86_64 for us when we will exit signal handler */
    ucp->uc_mcontext.gregs[REG_RIP] = (uint64_t) restore_be_x86_64;
    ucp->uc_mcontext.gregs[REG_RDI] = (uint64_t) backend;
    ucp->uc_mcontext.gregs[REG_RSI] = result;
}

static void patch(struct backend *backend, void *link_patch_area, void *cache_area)
{
    unsigned char *pos = link_patch_area;
    uint64_t target = (uint64_t) cache_area;

    assert(*pos == 0xc3);
    *pos++=0x90;
    pos+=2;
    *pos++ = (target >> 0) & 0xff;
    *pos++ = (target >> 8) & 0xff;
    *pos++ = (target >> 16) & 0xff;
    *pos++ = (target >> 24) & 0xff;
    *pos++ = (target >> 32) & 0xff;
    *pos++ = (target >> 40) & 0xff;
    *pos++ = (target >> 48) & 0xff;
    *pos++ = (target >> 56) & 0xff;
}

/* backend api */
static int jit(struct backend *backend, struct irInstruction *irArray, int irInsnNb, char *buffer, int bufferSize)
{
    struct inter *inter = container_of(backend, struct inter, backend);
    int res;

    // allocate x86 instructions
    allocateInstructions(inter, irArray, irInsnNb);

    // allocate registers
    allocateRegisters(inter);

    // generate code
    res = generateCode(inter, buffer);
    assert(res <= bufferSize);

    return res;
}

static uint32_t get_marker(struct backend *backend, struct irInstruction *irArray, int irInsnNb, char *buffer, int bufferSize, int offset)
{
    struct inter *inter = container_of(backend, struct inter, backend);
    int res;

    // allocate x86 instructions
    allocateInstructions(inter, irArray, irInsnNb);

    // allocate registers
    allocateRegisters(inter);

    // find insn
    res = findInsnInCode(inter, buffer, offset);

    return res;
}

static void reset(struct backend *backend)
{
    struct inter *inter = container_of(backend, struct inter, backend);

    inter->registerPoolAllocator.reset(&inter->registerPoolAllocator);
    inter->instructionPoolAllocator.reset(&inter->instructionPoolAllocator);
    inter->regIndex = 0;
    inter->instructionIndex = 0;
}

/* api */
struct backend *createX86_64Backend(void *memory, int size)
{
    struct inter *inter;

    assert(BE_X86_64_MIN_CONTEXT_SIZE >= sizeof(*inter));
    inter = (struct inter *) memory;
    if (inter) {
        int pool_mem_size;
        int struct_inter_size_aligned_16 = ((sizeof(*inter) + 15) & ~0xf);

        inter->restore_sp = 0;
        inter->backend.jit = jit;
        inter->backend.execute = execute_be_x86_64;
        inter->backend.request_signal_alternate_exit = request_signal_alternate_exit;
        inter->backend.get_marker = get_marker;
        inter->backend.patch = patch;
        inter->backend.reset = reset;
        inter->registerPoolAllocator.alloc = memoryPoolAlloc;
        inter->instructionPoolAllocator.alloc = memoryPoolAlloc;
        inter->registerPoolAllocator.reset = memoryPoolReset;
        inter->instructionPoolAllocator.reset = memoryPoolReset;

        /* setup pool memory */
        pool_mem_size = (size - struct_inter_size_aligned_16) / 2;
        memoryPoolInit(&inter->registerPoolAllocator, memory + struct_inter_size_aligned_16, pool_mem_size);
        memoryPoolInit(&inter->instructionPoolAllocator, memory + struct_inter_size_aligned_16 + pool_mem_size, pool_mem_size);

        reset(&inter->backend);
    }

    return &inter->backend;
}

void deleteX86_64Backend(struct backend *backend)
{
    ;
}

