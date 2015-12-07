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
#include "be_i386.h"
#include "be_i386_private.h"

//#define DEBUG_REG_ALLOC     1

#define container_of(ptr, type, member) ({          \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type,member) );})

#define REG_NUMBER  16

/* can be a 32 or 64 bits register. If it's a 64 bits register
  then it will use two i386 registers to hold this 64 bit value.
  In that case index2 will hold register index else it will hold
  -1 if not use */
struct x86Register {
    int isConstant;
    int index;
    int index2;
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
        res->index2 = (irReg && irReg->type == IR_REG_64)?inter->regIndex++:-1;
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
            case IR_STORE_8:
            case IR_STORE_16:
            case IR_STORE_32:
            case IR_STORE_64:
                {
                    struct x86Register *address32 = allocateRegister(inter, (insn->u.store.address->type == IR_REG_64)?NULL:insn->u.store.address);

                    if (insn->u.store.address->type == IR_REG_64)
                        add_cast(inter, IR_CAST_64_TO_32, address32, allocateRegister(inter, insn->u.store.address));
                    add_store(inter, X86_STORE_8 + insn->type - IR_STORE_8, allocateRegister(inter, insn->u.store.src), address32);
                }
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
                        case IR_BINOP_ASR_8: goto_p0 = 24; goto binop_asr;
                        case IR_BINOP_ASR_16: goto_p0 = 16; goto binop_asr;
                        case IR_BINOP_ASR_32: goto_p0 = 0; goto binop_asr;
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
                                    add_binop(inter, X86_BINOP_32, X86_BINOP_SHL, shiftLeftResult, op1, shiftValue);
                                    shiftRightResult = allocateRegister(inter, NULL);
                                    add_binop(inter, X86_BINOP_32, X86_BINOP_ASR, shiftRightResult, shiftLeftResult, shiftValue);
                                } else {
                                    shiftRightResult = allocateRegister(inter, insn->u.binop.op1);
                                }
                                add_binop(inter, X86_BINOP_8 + insn->u.binop.type - IR_BINOP_ASR_8, X86_BINOP_ASR, dst, shiftRightResult, op2);
                            }
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
            default:
                fprintf(stderr, "Unknown ir type %d\n", insn->type);
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
    if (reg->index2 != -1) {
        reg->index2 = getFreeRegRaw(freeRegList);
        if (reg->lastReadIndex == -1)
            freeRegList[reg->index2] = 1;
    }
}

static void setRegFree(int *freeRegList, struct x86Register *reg)
{
    freeRegList[reg->index] = 1;
    if (reg->index2 != -1)
        freeRegList[reg->index2] = 1;
}

static void setRegFreeIfNoMoreUse(int *freeRegList, struct x86Register *reg, int index)
{
    if (reg && reg->lastReadIndex == index)
        setRegFree(freeRegList, reg);
}

#ifdef DEBUG_REG_ALLOC
static void displayReg(struct x86Register *reg)
{
    if (reg) {
        if (reg->index2 != -1)
            printf("R%d:%d[%d->%d[", reg->index, reg->index2, reg->firstWriteIndex, reg->lastReadIndex);
        else
            printf("R%d[%d->%d[", reg->index, reg->firstWriteIndex, reg->lastReadIndex);
    }
    else
        printf("NULL");
}
#endif

static void allocateRegisters(struct inter *inter)
{
    int i;
    struct x86Instruction *insn = (struct x86Instruction *) inter->instructionPoolAllocator.buffer;
    int freeRegList[REG_NUMBER] = {1, 1, 1, 1, 1, 1, 1, 1,
                                   1, 1, 1, 1, 1, 1, 1, 1};

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
                printf(", 0x%016lx", insn->u.mov.value);
#endif
                break;
            case X86_STORE_8:
            case X86_STORE_16:
            case X86_STORE_32:
            case X86_STORE_64:
                setRegFreeIfNoMoreUse(freeRegList, insn->u.store.address, i);
                setRegFreeIfNoMoreUse(freeRegList, insn->u.store.src, i);
#ifdef DEBUG_REG_ALLOC
                printf("store_%d ", 1 << (insn->type - IR_STORE_8 + 3));
                printf("[");
                displayReg(insn->u.store.address);
                printf("], ");
                displayReg(insn->u.store.src);
#endif
                break;
            case X86_CAST:
                getFreeReg(freeRegList, insn->u.cast.dst);
                setRegFreeIfNoMoreUse(freeRegList, insn->u.cast.op, i);
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
            case X86_BINOP_8:
            case X86_BINOP_16:
            case X86_BINOP_32:
            case X86_BINOP_64:
                getFreeReg(freeRegList, insn->u.binop.dst);
                setRegFreeIfNoMoreUse(freeRegList, insn->u.binop.op1, i);
                setRegFreeIfNoMoreUse(freeRegList, insn->u.binop.op2, i);
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
                setRegFreeIfNoMoreUse(freeRegList, insn->u.exit.value, i);
                setRegFreeIfNoMoreUse(freeRegList, insn->u.exit.pred, i);
#ifdef DEBUG_REG_ALLOC
                printf("exit ");
                displayReg(insn->u.exit.value);
                printf(" if ");
                displayReg(insn->u.exit.pred);
#endif
                break;
            case X86_ITE:
                getFreeReg(freeRegList, insn->u.ite.dst);
                setRegFreeIfNoMoreUse(freeRegList, insn->u.ite.pred, i);
                setRegFreeIfNoMoreUse(freeRegList, insn->u.ite.trueOp, i);
                setRegFreeIfNoMoreUse(freeRegList, insn->u.ite.falseOp, i);
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
            default:
                fprintf(stderr, "Unknown insn type %d\n", insn->type);
                assert(0);
        }
#ifdef DEBUG_REG_ALLOC
    printf("\n");
    {
        int j;
        printf("    ");
        for(j=0;j<REG_NUMBER;j++)
            printf("%d", freeRegList[j]);
        printf("\n");
    }
#endif
    }
}

/* code generation */
#define VREG_OFFSET(idx)    (-(idx) * 4 - 4)

#define MODRM_MODE_0        0x00
#define MODRM_MODE_1        0x40
#define MODRM_MODE_2        0x80
#define MODRM_MODE_3        0xC0
#define MODRM_REG_SHIFT     3
#define MODRM_RM_SHIFT      0

#define EAX                 0
#define ECX                 1
#define EDX                 2
#define EBX                 3
#define ESP                 4
#define EBP                 5
#define ESI                 6
#define EDI                 7

static char *gen_and_between_physicals(char *pos, int dst, int op)
{
    *pos++ = 0x21;
    *pos++ = MODRM_MODE_3 | (dst << MODRM_RM_SHIFT) | (op << MODRM_REG_SHIFT);

    return pos;
}

static char *gen_xor_between_physicals(char *pos, int dst, int op)
{
    *pos++ = 0x31;
    *pos++ = MODRM_MODE_3 | (dst << MODRM_RM_SHIFT) | (op << MODRM_REG_SHIFT);

    return pos;
}

static char *gen_or_between_physicals(char *pos, int dst, int op)
{
    *pos++ = 0x09;
    *pos++ = MODRM_MODE_3 | (dst << MODRM_RM_SHIFT) | (op << MODRM_REG_SHIFT);

    return pos;
}

static char *gen_add_between_physicals(char *pos, int dst, int op)
{
    *pos++ = 0x01;
    *pos++ = MODRM_MODE_3 | (dst << MODRM_RM_SHIFT) | (op << MODRM_REG_SHIFT);

    return pos;
}

static char *gen_sub_between_physicals(char *pos, int dst, int op)
{
    *pos++ = 0x29;
    *pos++ = MODRM_MODE_3 | (dst << MODRM_RM_SHIFT) | (op << MODRM_REG_SHIFT);

    return pos;
}

static char *gen_mov_const_in_physical_reg(char *pos, int index, uint32_t value)
{
    *pos++ = 0xb8 + index;
    *pos++ = (value >> 0) & 0xff;
    *pos++ = (value >> 8) & 0xff;
    *pos++ = (value >> 16) & 0xff;
    *pos++ = (value >> 24) & 0xff;

    return pos;
}

static char *gen_mov_const_in_virtual_reg(char *pos, int index, uint32_t value)
{
    *pos++ = 0xc7;
    *pos++ = MODRM_MODE_1 | (EBP << MODRM_RM_SHIFT);
    *pos++ = VREG_OFFSET(index);
    *pos++ = (value >> 0) & 0xff;
    *pos++ = (value >> 8) & 0xff;
    *pos++ = (value >> 16) & 0xff;
    *pos++ = (value >> 24) & 0xff;

    return pos;
}

static char *gen_mov_from_virtual_to_physical(char *pos, int from, int to)
{
    *pos++ = 0x8b;
    *pos++ = MODRM_MODE_1 | (EBP << MODRM_RM_SHIFT) | (to << MODRM_REG_SHIFT);
    *pos++ = VREG_OFFSET(from);

    return pos;
}

static char *gen_mov_from_physical_to_physical(char *pos, int from, int to)
{
    *pos++ = 0x89;
    *pos++ = MODRM_MODE_3 | (to << MODRM_RM_SHIFT) | (from << MODRM_REG_SHIFT);

    return pos;
}

static char *gen_mov_from_physical_to_virtual(char *pos, int from, int to)
{
    *pos++ = 0x89;
    *pos++ = MODRM_MODE_1 | (EBP << MODRM_RM_SHIFT) | (from << MODRM_REG_SHIFT);
    *pos++ = VREG_OFFSET(to);

    return pos;
}

static char *gen_mov_from_virtual_to_virtual_hlp(char *pos, int from, int to)
{
    pos = gen_mov_from_virtual_to_physical(pos, from, EAX);
    pos = gen_mov_from_physical_to_virtual(pos, EAX, to);

    return pos;
}

static char *gen_mov_const(char *pos, struct x86Instruction *insn)
{
    pos = gen_mov_const_in_virtual_reg(pos, insn->u.mov.dst->index, insn->u.mov.value);
    if (insn->u.mov.dst->index2 != -1)
        pos = gen_mov_const_in_virtual_reg(pos, insn->u.mov.dst->index2, insn->u.mov.value >> 32);

    return pos;
}

static char *gen_store_8(char *pos, struct x86Instruction *insn)
{
    pos = gen_mov_from_virtual_to_physical(pos, insn->u.store.address->index, EAX);
    pos = gen_mov_from_virtual_to_physical(pos, insn->u.store.src->index, ECX);
    *pos++ = 0x88;
    *pos++ = MODRM_MODE_0 | (EAX << MODRM_RM_SHIFT) | (ECX << MODRM_REG_SHIFT);

    return pos;
}

static char *gen_store_16(char *pos, struct x86Instruction *insn)
{
    pos = gen_mov_from_virtual_to_physical(pos, insn->u.store.address->index, EAX);
    pos = gen_mov_from_virtual_to_physical(pos, insn->u.store.src->index, ECX);
    *pos++ = 0x66;
    *pos++ = 0x89;
    *pos++ = MODRM_MODE_0 | (EAX << MODRM_RM_SHIFT) | (ECX << MODRM_REG_SHIFT);

    return pos;
}

static char *gen_store_32(char *pos, struct x86Instruction *insn)
{
    pos = gen_mov_from_virtual_to_physical(pos, insn->u.store.address->index, EAX);
    pos = gen_mov_from_virtual_to_physical(pos, insn->u.store.src->index, ECX);
    *pos++ = 0x89;
    *pos++ = MODRM_MODE_0 | (EAX << MODRM_RM_SHIFT) | (ECX << MODRM_REG_SHIFT);

    return pos;
}

static char *gen_store_64(char *pos, struct x86Instruction *insn)
{
    pos = gen_mov_from_virtual_to_physical(pos, insn->u.store.address->index, EAX);
    pos = gen_mov_from_virtual_to_physical(pos, insn->u.store.src->index, ECX);
    *pos++ = 0x89;
    *pos++ = MODRM_MODE_0 | (EAX << MODRM_RM_SHIFT) | (ECX << MODRM_REG_SHIFT);
    pos = gen_mov_from_virtual_to_physical(pos, insn->u.store.address->index, EAX);
    /* add four to eax */
    *pos++ = 0x05;
    *pos++ = 4;
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;
    pos = gen_mov_from_virtual_to_physical(pos, insn->u.store.src->index2, ECX);
    *pos++ = 0x89;
    *pos++ = MODRM_MODE_0 | (EAX << MODRM_RM_SHIFT) | (ECX << MODRM_REG_SHIFT);

    return pos;
}

static char *gen_upper_unsigned_cast_hlp(char *pos, struct x86Register *dst, struct x86Register *op, uint32_t mask)
{
    pos = gen_mov_from_virtual_to_physical(pos, op->index, EAX);
    pos = gen_mov_const_in_physical_reg(pos, ECX, mask);
    pos = gen_and_between_physicals(pos, EAX, ECX);
    pos = gen_mov_from_physical_to_virtual(pos, EAX, dst->index);
    if (dst->index2 != -1)
        pos = gen_mov_const_in_virtual_reg(pos, dst->index2, 0);

    return pos;
}

static char *gen_upper_signed_cast_hlp(char *pos, struct x86Register *dst, struct x86Register *op, int shift_value)
{
    pos = gen_mov_from_virtual_to_physical(pos, op->index, EAX);
    if (shift_value) {
        pos = gen_mov_const_in_physical_reg(pos, ECX, shift_value);
        *pos++ = 0xd3;
        *pos++ = MODRM_MODE_3 | (4/*shl*/ << MODRM_REG_SHIFT) | EAX;
        *pos++ = 0xd3;
        *pos++ = MODRM_MODE_3 | (7/*asr*/ << MODRM_REG_SHIFT) | EAX;
    }
    pos = gen_mov_from_physical_to_virtual(pos, EAX, dst->index);
    if (dst->index2 != -1) {
        pos = gen_mov_const_in_physical_reg(pos, ECX, 31 - shift_value);
        *pos++ = 0xd3;
        *pos++ = MODRM_MODE_3 | (7/*asr*/ << MODRM_REG_SHIFT) | EAX;
        pos = gen_mov_from_physical_to_virtual(pos, EAX, dst->index2);
    }

    return pos;
}

static char *gen_cast(char *pos, struct x86Instruction *insn)
{
    switch(insn->u.cast.type) {
        case IR_CAST_8U_TO_16: case IR_CAST_8U_TO_32: case IR_CAST_8U_TO_64:
            pos = gen_upper_unsigned_cast_hlp(pos, insn->u.cast.dst, insn->u.cast.op, 0xff);
            break;
        case IR_CAST_8S_TO_16: case IR_CAST_8S_TO_32: case IR_CAST_8S_TO_64:
            pos = gen_upper_signed_cast_hlp(pos, insn->u.cast.dst, insn->u.cast.op, 24);
            break;
        case IR_CAST_16S_TO_32: case IR_CAST_16S_TO_64:
            pos = gen_upper_signed_cast_hlp(pos, insn->u.cast.dst, insn->u.cast.op, 16);
            break;
        case IR_CAST_32S_TO_64:
            pos = gen_upper_signed_cast_hlp(pos, insn->u.cast.dst, insn->u.cast.op, 0);
            break;
        case IR_CAST_64_TO_8: case IR_CAST_32_TO_8: case IR_CAST_16_TO_8:
            pos = gen_upper_unsigned_cast_hlp(pos, insn->u.cast.dst, insn->u.cast.op, 0xff);
            break;
        case IR_CAST_64_TO_16: case IR_CAST_32_TO_16:
            pos = gen_upper_unsigned_cast_hlp(pos, insn->u.cast.dst, insn->u.cast.op, 0xffff);
            break;
        case IR_CAST_64_TO_32:
            pos = gen_mov_from_virtual_to_virtual_hlp(pos, insn->u.cast.op->index, insn->u.cast.dst->index);
            break;
        default:
            fprintf(stderr, "Implement cast type %d\n", insn->u.cast.type);
            assert(0);
    }

    return pos;
}

static char *gen_exit(char *pos, struct x86Instruction *insn)
{
    /* FIXME : to be rework */
    assert(insn->u.exit.pred == NULL);
    /* For the moment we return exit value into eax+edx */
    pos = gen_mov_from_virtual_to_physical(pos, insn->u.exit.value->index, EAX);
    pos = gen_mov_from_virtual_to_physical(pos, insn->u.exit.value->index2, EDX);

    /* ret */
    *pos++ = 0xc3;

    return pos;
}

static char *gen_ite(char *pos, struct x86Instruction *insn)
{
    char *patch;

    /* mov dst with false predicate */
    pos = gen_mov_from_virtual_to_virtual_hlp(pos, insn->u.ite.falseOp->index, insn->u.ite.dst->index);
    if (insn->u.mov.dst->index2 != -1)
        pos = gen_mov_from_virtual_to_virtual_hlp(pos, insn->u.ite.falseOp->index2, insn->u.ite.dst->index2);

    /* move predicate in eax */
    pos = gen_mov_from_virtual_to_physical(pos, insn->u.ite.pred->index, EAX);
    if (insn->u.mov.dst->index2 != -1) {
        pos = gen_mov_from_virtual_to_physical(pos, insn->u.ite.pred->index, ECX);
        pos = gen_or_between_physicals(pos, EAX, ECX);
    }

    /* compare eax with zero */
    *pos++ = 0x3d;
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;

    /* branch to end if eax is zero */
    *pos++ = 0x74;
    patch = pos++;

    /* mov dst with true predicate */
    pos = gen_mov_from_virtual_to_virtual_hlp(pos, insn->u.ite.trueOp->index, insn->u.ite.dst->index);
    if (insn->u.mov.dst->index2 != -1)
        pos = gen_mov_from_virtual_to_virtual_hlp(pos, insn->u.ite.trueOp->index2, insn->u.ite.dst->index2);

    /* patch jump distance */
    *patch = pos - patch - 1;

    return pos;
}

static char *gen_cmp32(char *pos, int isEq, struct x86Register *dst, struct x86Register *op1, struct x86Register *op2)
{
    char *patch;

    /* set true value */
    pos = gen_mov_const_in_virtual_reg(pos, dst->index, ~0);

    /* compute op1 ^ op2 and put result in eax */
    pos = gen_mov_from_virtual_to_physical(pos, op1->index, EAX);
    pos = gen_mov_from_virtual_to_physical(pos, op2->index, ECX);
    pos = gen_xor_between_physicals(pos, EAX, ECX);

    /* compare eax with zero */
    *pos++ = 0x3d;
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;

    /* branch to end if eax is zero */
    *pos++ = (isEq)?0x74:0x75;
    patch = pos++;

    /* set false value */
    pos = gen_mov_const_in_virtual_reg(pos, dst->index, 0);

    /* patch jump distance */
    *patch = pos - patch - 1;

    pos = gen_mov_from_virtual_to_physical(pos, dst->index, EAX);

    return pos;
}

static char *gen_cmp64(char *pos, int isEq, struct x86Register *dst, struct x86Register *op1, struct x86Register *op2)
{
    char *patch;

    /* set true value */
    pos = gen_mov_const_in_virtual_reg(pos, dst->index, ~0);
    pos = gen_mov_const_in_virtual_reg(pos, dst->index2, ~0);

    /* compute op1 ^ op2 and put result in eax */
    pos = gen_mov_from_virtual_to_physical(pos, op1->index, EAX);
    pos = gen_mov_from_virtual_to_physical(pos, op2->index, ECX);
    pos = gen_xor_between_physicals(pos, EAX, ECX);
    pos = gen_mov_from_virtual_to_physical(pos, op1->index2, EDX);
    pos = gen_mov_from_virtual_to_physical(pos, op2->index2, ECX);
    pos = gen_xor_between_physicals(pos, ECX, EDX);
    pos = gen_or_between_physicals(pos, EAX, ECX);

    /* compare eax with zero */
    *pos++ = 0x3d;
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;
    *pos++ = 0;

    /* branch to end if eax is zero */
    *pos++ = (isEq)?0x74:0x75;
    patch = pos++;

    /* set false value */
    pos = gen_mov_const_in_virtual_reg(pos, dst->index, 0);
    pos = gen_mov_const_in_virtual_reg(pos, dst->index2, 0);

    /* patch jump distance */
    *patch = pos - patch - 1;

    return pos;
}

static char *gen_binop(char *pos, struct x86Instruction *insn, uint32_t mask)
{
    static const char binopToOpcode[] = {0x01/*add*/, 0x29/*sub*/, 0x31/*xor*/, 0x21/*and*/,
                                         0x09/*or*/, 0xd3/*shl*/, 0xd3/*shr*/, 0xd3/*sar*/, 0xd3/*ror*/,
                                         0xff/*cmpeq*/, 0xff/*cmpne*/};
    int subtype;

    /* do ops with result in eax */
    switch(insn->u.binop.type) {
        case X86_BINOP_ADD:
        case X86_BINOP_SUB:
        case X86_BINOP_XOR:
        case X86_BINOP_AND:
        case X86_BINOP_OR:
            pos = gen_mov_from_virtual_to_physical(pos, insn->u.binop.op1->index, EAX);
            pos = gen_mov_from_virtual_to_physical(pos, insn->u.binop.op2->index, ECX);
            *pos++ = binopToOpcode[insn->u.binop.type];
            *pos++ = MODRM_MODE_3 | (ECX << MODRM_REG_SHIFT) | EAX;
            break;
        case X86_BINOP_SHL: subtype = 4; goto unop;
        case X86_BINOP_SHR: subtype = 5; goto unop;
        case X86_BINOP_ASR: subtype = 7; goto unop;
            unop: {
                pos = gen_mov_from_virtual_to_physical(pos, insn->u.binop.op1->index, EAX);
                pos = gen_mov_from_virtual_to_physical(pos, insn->u.binop.op2->index, ECX);
                *pos++ = binopToOpcode[insn->u.binop.type];
                *pos++ = MODRM_MODE_3 | (subtype << MODRM_REG_SHIFT) | EAX;
            }
            break;
        case X86_BINOP_CMPEQ:
            pos = gen_cmp32(pos, 1, insn->u.binop.dst, insn->u.binop.op1, insn->u.binop.op2);
            break;
        case X86_BINOP_CMPNE:
            pos = gen_cmp32(pos, 0, insn->u.binop.dst, insn->u.binop.op1, insn->u.binop.op2);
            break;
        default:
            fprintf(stderr, "Implement binop type %d\n", insn->u.binop.type);
            assert(0);
    }
    /* mask if needed */
    if (mask) {
        *pos++ = 0x25;
        *pos++ = (mask >> 0) & 0xff;
        *pos++ = (mask >> 8) & 0xff;
        *pos++ = (mask >> 16) & 0xff;
        *pos++ = (mask >> 24) & 0xff;
    }
    /* store result */
    pos = gen_mov_from_physical_to_virtual(pos, EAX, insn->u.binop.dst->index);

    return pos;
}

static char *gen_binop64(char *pos, struct x86Instruction *insn)
{
    static const char binopToOpcode1[] = {0x01/*add*/, 0x29/*sub*/, 0x31/*xor*/, 0x21/*and*/,
                                         0x09/*or*/, 0xd3/*shl*/, 0xd3/*shr*/, 0xd3/*sar*/, 0xd3/*ror*/,
                                         0xff/*cmpeq*/, 0xff/*cmpne*/};
    static const char binopToOpcode2[] = {0x11/*adc*/, 0x19/*sbb*/, 0x31/*xor*/, 0x21/*and*/,
                                         0x09/*or*/, 0xd3/*shl*/, 0xd3/*shr*/, 0xd3/*sar*/, 0xd3/*ror*/,
                                         0xff/*cmpeq*/, 0xff/*cmpne*/};
    int subtype;

    /* do ops with result in eax */
    switch(insn->u.binop.type) {
        case X86_BINOP_ADD:
        case X86_BINOP_SUB:
        case X86_BINOP_XOR:
        case X86_BINOP_AND:
        case X86_BINOP_OR:
            /* lower part */
            pos = gen_mov_from_virtual_to_physical(pos, insn->u.binop.op1->index, EAX);
            pos = gen_mov_from_virtual_to_physical(pos, insn->u.binop.op2->index, ECX);
            *pos++ = binopToOpcode1[insn->u.binop.type];
            *pos++ = MODRM_MODE_3 | (ECX << MODRM_REG_SHIFT) | EAX;
            pos = gen_mov_from_physical_to_virtual(pos, EAX, insn->u.binop.dst->index);
            /* upper part */
            pos = gen_mov_from_virtual_to_physical(pos, insn->u.binop.op1->index2, EAX);
            pos = gen_mov_from_virtual_to_physical(pos, insn->u.binop.op2->index2, ECX);
            *pos++ = binopToOpcode2[insn->u.binop.type];
            *pos++ = MODRM_MODE_3 | (ECX << MODRM_REG_SHIFT) | EAX;
            pos = gen_mov_from_physical_to_virtual(pos, EAX, insn->u.binop.dst->index2);
            break;
        case X86_BINOP_SHL:
            /* lower part */
            pos = gen_mov_from_virtual_to_physical(pos, insn->u.binop.op1->index, EAX);
            pos = gen_mov_from_virtual_to_physical(pos, insn->u.binop.op2->index, ECX);
            *pos++ = binopToOpcode1[insn->u.binop.type];
            *pos++ = MODRM_MODE_3 | (4/*shl*/ << MODRM_REG_SHIFT) | EAX;
            pos = gen_mov_from_physical_to_virtual(pos, EAX, insn->u.binop.dst->index);
            /* upper part */
            pos = gen_mov_from_virtual_to_physical(pos, insn->u.binop.op1->index2, EAX);
            *pos++ = binopToOpcode1[insn->u.binop.type];
            *pos++ = MODRM_MODE_3 | (4/*shl*/ << MODRM_REG_SHIFT) | EAX;
             /* store upper shift left into EDX */
            pos = gen_mov_from_physical_to_physical(pos, EAX, EDX);
             /* now shift righ by (32 - shift) of lower 32 bits */
            pos = gen_mov_const_in_physical_reg(pos, EAX, 32);
            pos = gen_sub_between_physicals(pos, EAX, ECX);
            pos = gen_mov_from_physical_to_physical(pos, EAX, ECX);
            pos = gen_mov_from_virtual_to_physical(pos, insn->u.binop.op1->index, EAX);
            *pos++ = binopToOpcode1[insn->u.binop.type];
            *pos++ = MODRM_MODE_3 | (5/*shr*/ << MODRM_REG_SHIFT) | EAX;
             /* now just add both part and write in virtual */
            pos = gen_add_between_physicals(pos, EAX, EDX);
            pos = gen_mov_from_physical_to_virtual(pos, EAX, insn->u.binop.dst->index2);
            break;
        case X86_BINOP_SHR: subtype = 5; goto unop;
        case X86_BINOP_ASR: subtype = 7; goto unop;
            unop: {
                /* upper part */
                pos = gen_mov_from_virtual_to_physical(pos, insn->u.binop.op1->index2, EAX);
                pos = gen_mov_from_virtual_to_physical(pos, insn->u.binop.op2->index, ECX);
                *pos++ = binopToOpcode1[insn->u.binop.type];
                *pos++ = MODRM_MODE_3 | (subtype << MODRM_REG_SHIFT) | EAX;
                pos = gen_mov_from_physical_to_virtual(pos, EAX, insn->u.binop.dst->index2);
                /* lower part */
                pos = gen_mov_from_virtual_to_physical(pos, insn->u.binop.op1->index, EAX);
                *pos++ = binopToOpcode1[insn->u.binop.type];
                *pos++ = MODRM_MODE_3 | (5/*shr*/ << MODRM_REG_SHIFT) | EAX;
                 /* store lower shift left into EDX */
                pos = gen_mov_from_physical_to_physical(pos, EAX, EDX);
                 /* now shift left by (32 - shift) of upper 32 bits */
                pos = gen_mov_const_in_physical_reg(pos, EAX, 32);
                pos = gen_sub_between_physicals(pos, EAX, ECX);
                pos = gen_mov_from_physical_to_physical(pos, EAX, ECX);
                pos = gen_mov_from_virtual_to_physical(pos, insn->u.binop.op1->index2, EAX);
                *pos++ = binopToOpcode1[insn->u.binop.type];
                *pos++ = MODRM_MODE_3 | (4/*shl*/ << MODRM_REG_SHIFT) | EAX;
                 /* now just add both part and write in virtual */
                pos = gen_add_between_physicals(pos, EAX, EDX);
                pos = gen_mov_from_physical_to_virtual(pos, EAX, insn->u.binop.dst->index);
            }
            break;
        case X86_BINOP_CMPEQ:
            pos = gen_cmp64(pos, 1, insn->u.binop.dst, insn->u.binop.op1, insn->u.binop.op2);
            break;
        case X86_BINOP_CMPNE:
            pos = gen_cmp64(pos, 0, insn->u.binop.dst, insn->u.binop.op1, insn->u.binop.op2);
            break;
        default:
            fprintf(stderr, "Implement binop type %d\n", insn->u.binop.type);
            assert(0);
    }

    return pos;
}

static int generateCode(struct inter *inter, char *buffer)
{
    int i;
    struct x86Instruction *insn = (struct x86Instruction *) inter->instructionPoolAllocator.buffer;
    char *pos = buffer;
    uint32_t mask;

    for (i = 0; i < inter->instructionIndex; ++i, insn++)
    {
        switch(insn->type) {
            case X86_MOV_CONST:
                pos = gen_mov_const(pos, insn);
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
            case X86_BINOP_32: mask = 0; goto binop;
                binop:
                    pos = gen_binop(pos, insn, mask);
                break;
            case X86_BINOP_64:
                pos = gen_binop64(pos, insn);
                break;
            case X86_CAST:
                pos = gen_cast(pos, insn);
                break;
            case X86_EXIT:
                pos = gen_exit(pos, insn);
                break;
            case X86_ITE:
                pos = gen_ite(pos, insn);
                break;
            default:
                fprintf(stderr, "unknown insn type for generatecode %d\n", insn->type);
                assert(0);
        }
    }

    return pos - buffer;
}

/* backend api */
static void request_signal_alternate_exit(struct backend *backend, void *_ucp, uint64_t result)
{
    assert(0 && "Implement me\n");
}

static void patch(struct backend *backend, void *link_patch_area, void *cache_area)
{
    assert(0 && "Implement me\n");
}

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

static int find_insn(struct backend *backend, struct irInstruction *irArray, int irInsnNb, char *buffer, int bufferSize, int offset)
{
    struct inter *inter = container_of(backend, struct inter, backend);
    int res;

    assert(0 && "Implement me\n");

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
struct backend *createI386Backend(void *memory, int size)
{
    struct inter *inter;

    assert(BE_I386_MIN_CONTEXT_SIZE >= sizeof(*inter));
    inter = (struct inter *) memory;
    if (inter) {
        int pool_mem_size;
        int struct_inter_size_aligned_16 = ((sizeof(*inter) + 15) & ~0xf);

        inter->backend.jit = jit;
        inter->backend.execute = execute_be_i386;
        inter->backend.request_signal_alternate_exit = request_signal_alternate_exit;
        inter->backend.find_insn = find_insn;
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

void deleteI386Backend(struct backend *backend)
{
    ;
}
