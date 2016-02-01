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
#include <stddef.h>
#include <assert.h>
#include "jitter.h"

#define container_of(ptr, type, member) ({			\
    const typeof( ((type *)0)->member ) *__mptr = (ptr);	\
    (type *)( (char *)__mptr - offsetof(type,member) );})

struct memoryPool {
    void *(*alloc)(struct memoryPool *pool, int size);
    void *(*reset)(struct memoryPool *pool);
    char *buffer;
    int index;
    int size;
};

struct jitter {
    struct memoryPool registerPoolAllocator;
    int instructionIndex;
    int regIndex;
    struct memoryPool instructionPoolAllocator;
    struct irInstructionAllocator irInstructionAllocator;
    struct backend *backend;
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

/* ir register allocator */
struct irRegister *allocateRegister(struct irInstructionAllocator *irAlloc, enum irRegisterType type)
{
    struct jitter *jitter = container_of(irAlloc, struct jitter, irInstructionAllocator);
    struct memoryPool *pool = &jitter->registerPoolAllocator;
    struct irRegister *reg = (struct irRegister *) pool->alloc(pool, sizeof(struct irRegister));

    reg->type = type;
    reg->index = jitter->regIndex++;
    reg->firstWriteIndex = jitter->instructionIndex;
    reg->lastReadIndex = -1;
    reg->backend = NULL;

    return reg;
}

/* instruction creation */
static struct irRegister *add_mov_const(struct irInstructionAllocator *irAlloc, uint64_t value, enum irInstructionType insnType, enum irRegisterType regType)
{
    struct jitter *jitter = container_of(irAlloc, struct jitter, irInstructionAllocator);
    struct memoryPool *pool = &jitter->instructionPoolAllocator;
    struct irRegister *dst = allocateRegister(irAlloc, regType);
    struct irInstruction *insn = (struct irInstruction *) pool->alloc(pool, sizeof(struct irInstruction));

    insn->type = insnType;
    insn->u.mov.dst = dst;
    insn->u.mov.value = value;
    
    jitter->instructionIndex++;

    return dst;
}
static struct irRegister *add_mov_const_8(struct irInstructionAllocator *irAlloc, uint64_t value)
{
    return add_mov_const(irAlloc, value, IR_MOV_CONST_8, IR_REG_8);
}
static struct irRegister *add_mov_const_16(struct irInstructionAllocator *irAlloc, uint64_t value)
{
    return add_mov_const(irAlloc, value, IR_MOV_CONST_16, IR_REG_16);
}
static struct irRegister *add_mov_const_32(struct irInstructionAllocator *irAlloc, uint64_t value)
{
    return add_mov_const(irAlloc, value, IR_MOV_CONST_32, IR_REG_32);
}
static struct irRegister *add_mov_const_64(struct irInstructionAllocator *irAlloc, uint64_t value)
{
    return add_mov_const(irAlloc, value, IR_MOV_CONST_64, IR_REG_64);
}

static struct irRegister *add_load(struct irInstructionAllocator *irAlloc, struct irRegister *address, enum irInstructionType insnType, enum irRegisterType regType)
{
    struct jitter *jitter = container_of(irAlloc, struct jitter, irInstructionAllocator);
    struct memoryPool *pool = &jitter->instructionPoolAllocator;
    struct irRegister *dst = allocateRegister(irAlloc, regType);
    struct irInstruction *insn = (struct irInstruction *) pool->alloc(pool, sizeof(struct irInstruction));

    address->lastReadIndex = jitter->instructionIndex;

    insn->type = insnType;
    insn->u.load.dst = dst;
    insn->u.load.address = address;

    jitter->instructionIndex++;

    return dst;
}
static struct irRegister *add_load_8(struct irInstructionAllocator *irAlloc, struct irRegister *address)
{
    return add_load(irAlloc, address, IR_LOAD_8, IR_REG_8);
}
static struct irRegister *add_load_16(struct irInstructionAllocator *irAlloc, struct irRegister *address)
{
    return add_load(irAlloc, address, IR_LOAD_16, IR_REG_16);
}
static struct irRegister *add_load_32(struct irInstructionAllocator *irAlloc, struct irRegister *address)
{
    return add_load(irAlloc, address, IR_LOAD_32, IR_REG_32);
}
static struct irRegister *add_load_64(struct irInstructionAllocator *irAlloc, struct irRegister *address)
{
    return add_load(irAlloc, address, IR_LOAD_64, IR_REG_64);
}

static void add_store(struct irInstructionAllocator *irAlloc, struct irRegister *src, struct irRegister *address, enum irInstructionType insnType)
{
    struct jitter *jitter = container_of(irAlloc, struct jitter, irInstructionAllocator);
    struct memoryPool *pool = &jitter->instructionPoolAllocator;
    struct irInstruction *insn = (struct irInstruction *) pool->alloc(pool, sizeof(struct irInstruction));

    address->lastReadIndex = jitter->instructionIndex;
    src->lastReadIndex = jitter->instructionIndex;

    insn->type = insnType;
    insn->u.store.src = src;
    insn->u.store.address = address;

    jitter->instructionIndex++;
}
void add_store_8(struct irInstructionAllocator * irAlloc, struct irRegister *src, struct irRegister *address)
{
    assert(src->type == IR_REG_8);
    add_store(irAlloc, src, address, IR_STORE_8);
}
void add_store_16(struct irInstructionAllocator * irAlloc, struct irRegister *src, struct irRegister *address)
{
    assert(src->type == IR_REG_16);
    add_store(irAlloc, src, address, IR_STORE_16);
}
void add_store_32(struct irInstructionAllocator * irAlloc, struct irRegister *src, struct irRegister *address)
{
    assert(src->type == IR_REG_32);
    add_store(irAlloc, src, address, IR_STORE_32);
}
void add_store_64(struct irInstructionAllocator * irAlloc, struct irRegister *src, struct irRegister *address)
{
    assert(src->type == IR_REG_64);
    add_store(irAlloc, src, address, IR_STORE_64);
}

static struct irRegister *add_binop(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2, enum irBinopType binopType)
{
    struct jitter *jitter = container_of(irAlloc, struct jitter, irInstructionAllocator);
    struct memoryPool *pool = &jitter->instructionPoolAllocator;
    struct irRegister *dst = allocateRegister(irAlloc, op1->type);
    struct irInstruction *insn = (struct irInstruction *) pool->alloc(pool, sizeof(struct irInstruction));

    op1->lastReadIndex = jitter->instructionIndex;
    op2->lastReadIndex = jitter->instructionIndex;

    insn->type = IR_BINOP;
    insn->u.binop.type = binopType;
    insn->u.binop.dst = dst;
    insn->u.binop.op1 = op1;
    insn->u.binop.op2 = op2;

    jitter->instructionIndex++;

    return dst;
}

static struct irRegister *add_add_8(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_8 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_ADD_8);
}
static struct irRegister *add_add_16(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_16 && op2->type == IR_REG_16);
    return add_binop(irAlloc, op1, op2, IR_BINOP_ADD_16);
}
static struct irRegister *add_add_32(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_32 && op2->type == IR_REG_32);
    return add_binop(irAlloc, op1, op2, IR_BINOP_ADD_32);
}
static struct irRegister *add_add_64(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_64 && op2->type == IR_REG_64);
    return add_binop(irAlloc, op1, op2, IR_BINOP_ADD_64);
}

static struct irRegister *add_sub_8(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_8 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_SUB_8);
}
static struct irRegister *add_sub_16(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_16 && op2->type == IR_REG_16);
    return add_binop(irAlloc, op1, op2, IR_BINOP_SUB_16);
}
static struct irRegister *add_sub_32(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_32 && op2->type == IR_REG_32);
    return add_binop(irAlloc, op1, op2, IR_BINOP_SUB_32);
}
static struct irRegister *add_sub_64(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_64 && op2->type == IR_REG_64);
    return add_binop(irAlloc, op1, op2, IR_BINOP_SUB_64);
}

static struct irRegister *add_xor_8(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_8 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_XOR_8);
}
static struct irRegister *add_xor_16(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_16 && op2->type == IR_REG_16);
    return add_binop(irAlloc, op1, op2, IR_BINOP_XOR_16);
}
static struct irRegister *add_xor_32(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_32 && op2->type == IR_REG_32);
    return add_binop(irAlloc, op1, op2, IR_BINOP_XOR_32);
}
static struct irRegister *add_xor_64(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_64 && op2->type == IR_REG_64);
    return add_binop(irAlloc, op1, op2, IR_BINOP_XOR_64);
}

static struct irRegister *add_and_8(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_8 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_AND_8);
}
static struct irRegister *add_and_16(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_16 && op2->type == IR_REG_16);
    return add_binop(irAlloc, op1, op2, IR_BINOP_AND_16);
}
static struct irRegister *add_and_32(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_32 && op2->type == IR_REG_32);
    return add_binop(irAlloc, op1, op2, IR_BINOP_AND_32);
}
static struct irRegister *add_and_64(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_64 && op2->type == IR_REG_64);
    return add_binop(irAlloc, op1, op2, IR_BINOP_AND_64);
}

static struct irRegister *add_or_8(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_8 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_OR_8);
}
static struct irRegister *add_or_16(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_16 && op2->type == IR_REG_16);
    return add_binop(irAlloc, op1, op2, IR_BINOP_OR_16);
}
static struct irRegister *add_or_32(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_32 && op2->type == IR_REG_32);
    return add_binop(irAlloc, op1, op2, IR_BINOP_OR_32);
}
static struct irRegister *add_or_64(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_64 && op2->type == IR_REG_64);
    return add_binop(irAlloc, op1, op2, IR_BINOP_OR_64);
}

static struct irRegister *add_shl_8(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_8 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_SHL_8);
}
static struct irRegister *add_shl_16(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_16 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_SHL_16);
}
static struct irRegister *add_shl_32(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_32 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_SHL_32);
}
static struct irRegister *add_shl_64(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_64 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_SHL_64);
}

static struct irRegister *add_shr_8(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_8 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_SHR_8);
}
static struct irRegister *add_shr_16(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_16 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_SHR_16);
}
static struct irRegister *add_shr_32(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_32 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_SHR_32);
}
static struct irRegister *add_shr_64(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_64 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_SHR_64);
}

static struct irRegister *add_asr_8(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_8 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_ASR_8);
}
static struct irRegister *add_asr_16(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_16 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_ASR_16);
}
static struct irRegister *add_asr_32(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_32 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_ASR_32);
}
static struct irRegister *add_asr_64(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_64 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_ASR_64);
}

static struct irRegister *add_ror_8(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_8 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_ROR_8);
}
static struct irRegister *add_ror_16(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_16 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_ROR_16);
}
static struct irRegister *add_ror_32(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_32 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_ROR_32);
}
static struct irRegister *add_ror_64(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_64 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_ROR_64);
}

static struct irRegister *add_cmpeq_8(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_8 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_CMPEQ_8);
}
static struct irRegister *add_cmpeq_16(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_16 && op2->type == IR_REG_16);
    return add_binop(irAlloc, op1, op2, IR_BINOP_CMPEQ_16);
}
static struct irRegister *add_cmpeq_32(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_32 && op2->type == IR_REG_32);
    return add_binop(irAlloc, op1, op2, IR_BINOP_CMPEQ_32);
}
static struct irRegister *add_cmpeq_64(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_64 && op2->type == IR_REG_64);
    return add_binop(irAlloc, op1, op2, IR_BINOP_CMPEQ_64);
}

static struct irRegister *add_cmpne_8(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_8 && op2->type == IR_REG_8);
    return add_binop(irAlloc, op1, op2, IR_BINOP_CMPNE_8);
}
static struct irRegister *add_cmpne_16(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_16 && op2->type == IR_REG_16);
    return add_binop(irAlloc, op1, op2, IR_BINOP_CMPNE_16);
}
static struct irRegister *add_cmpne_32(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_32 && op2->type == IR_REG_32);
    return add_binop(irAlloc, op1, op2, IR_BINOP_CMPNE_32);
}
static struct irRegister *add_cmpne_64(struct irInstructionAllocator *irAlloc, struct irRegister *op1, struct irRegister *op2)
{
    assert(op1->type == IR_REG_64 && op2->type == IR_REG_64);
    return add_binop(irAlloc, op1, op2, IR_BINOP_CMPNE_64);
}

struct irRegister *add_call(struct irInstructionAllocator *irAlloc, char *name, struct irRegister *address, struct irRegister *param[4], enum irInstructionType type)
{
    struct jitter *jitter = container_of(irAlloc, struct jitter, irInstructionAllocator);
    struct memoryPool *pool = &jitter->instructionPoolAllocator;
    struct irInstruction *insn = (struct irInstruction *) pool->alloc(pool, sizeof(struct irInstruction));
    int i;

    address->lastReadIndex = jitter->instructionIndex;
    for(i=0;i<4;i++)
        if (param[i])
            param[i]->lastReadIndex = jitter->instructionIndex;

    insn->u.call.address = address;
    insn->type = type;
    insn->u.call.name = "Unknown";
    for(i=0;i<4;i++)
        insn->u.call.param[i] = param[i];
    if (type == IR_CALL_VOID)
        insn->u.call.result = NULL;
    else
        insn->u.call.result = allocateRegister(irAlloc, IR_REG_8 + type - IR_CALL_8);

    jitter->instructionIndex++;

    return insn->u.call.result;
}

static void add_call_void(struct irInstructionAllocator *irAlloc, char *name, struct irRegister *address, struct irRegister *param[4])
{
    add_call(irAlloc, name, address, param, IR_CALL_VOID);
}

struct irRegister *add_call_8(struct irInstructionAllocator *irAlloc, char *name, struct irRegister *address, struct irRegister *param[4])
{
    return add_call(irAlloc, name, address, param, IR_CALL_8);
}

struct irRegister *add_call_16(struct irInstructionAllocator *irAlloc, char *name, struct irRegister *address, struct irRegister *param[4])
{
    return add_call(irAlloc, name, address, param, IR_CALL_16);
}

struct irRegister *add_call_32(struct irInstructionAllocator *irAlloc, char *name, struct irRegister *address, struct irRegister *param[4])
{
    return add_call(irAlloc, name, address, param, IR_CALL_32);
}

struct irRegister *add_call_64(struct irInstructionAllocator *irAlloc, char *name, struct irRegister *address, struct irRegister *param[4])
{
    return add_call(irAlloc, name, address, param, IR_CALL_64);
}

static struct irRegister *add_ite(struct irInstructionAllocator *irAlloc, struct irRegister *pred, struct irRegister *trueOp, struct irRegister *falseOp, enum irInstructionType type)
{
    struct jitter *jitter = container_of(irAlloc, struct jitter, irInstructionAllocator);
    struct memoryPool *pool = &jitter->instructionPoolAllocator;
    struct irRegister *dst = allocateRegister(irAlloc, trueOp->type);
    struct irInstruction *insn = (struct irInstruction *) pool->alloc(pool, sizeof(struct irInstruction));

    assert(pred->type == trueOp->type && pred->type == falseOp->type);

    pred->lastReadIndex = jitter->instructionIndex;
    trueOp->lastReadIndex = jitter->instructionIndex;
    falseOp->lastReadIndex = jitter->instructionIndex;

    insn->type = type;
    insn->u.ite.dst = dst;
    insn->u.ite.pred = pred;
    insn->u.ite.trueOp = trueOp;
    insn->u.ite.falseOp = falseOp;

    jitter->instructionIndex++;

    return dst;
}

static struct irRegister *add_ite_8(struct irInstructionAllocator *irAlloc, struct irRegister *pred, struct irRegister *trueOp, struct irRegister *falseOp)
{
    return add_ite(irAlloc, pred, trueOp, falseOp, IR_ITE_8);
}
static struct irRegister *add_ite_16(struct irInstructionAllocator *irAlloc, struct irRegister *pred, struct irRegister *trueOp, struct irRegister *falseOp)
{
    return add_ite(irAlloc, pred, trueOp, falseOp, IR_ITE_16);
}
static struct irRegister *add_ite_32(struct irInstructionAllocator *irAlloc, struct irRegister *pred, struct irRegister *trueOp, struct irRegister *falseOp)
{
    return add_ite(irAlloc, pred, trueOp, falseOp, IR_ITE_32);
}
static struct irRegister *add_ite_64(struct irInstructionAllocator *irAlloc, struct irRegister *pred, struct irRegister *trueOp, struct irRegister *falseOp)
{
    return add_ite(irAlloc, pred, trueOp, falseOp, IR_ITE_64);
}

static struct irRegister *add_cast(struct irInstructionAllocator *irAlloc, struct irRegister *op, enum irCastType type, enum irRegisterType dstType)
{
    struct jitter *jitter = container_of(irAlloc, struct jitter, irInstructionAllocator);
    struct memoryPool *pool = &jitter->instructionPoolAllocator;
    struct irRegister *dst = allocateRegister(irAlloc, dstType);
    struct irInstruction *insn = (struct irInstruction *) pool->alloc(pool, sizeof(struct irInstruction));

    op->lastReadIndex = jitter->instructionIndex;

    insn->type = IR_CAST;
    insn->u.cast.type = type;
    insn->u.cast.dst = dst;
    insn->u.cast.op = op;

    jitter->instructionIndex++;

    return dst;
}

static struct irRegister *add_8U_to_16(struct irInstructionAllocator *ir, struct irRegister *op)
{
    return add_cast(ir, op, IR_CAST_8U_TO_16, IR_REG_16);
}
static struct irRegister *add_8U_to_32(struct irInstructionAllocator *ir, struct irRegister *op)
{
    return add_cast(ir, op, IR_CAST_8U_TO_32, IR_REG_32);
}
static struct irRegister *add_8U_to_64(struct irInstructionAllocator *ir, struct irRegister *op)
{
    return add_cast(ir, op, IR_CAST_8U_TO_64, IR_REG_64);
}
static struct irRegister *add_8S_to_16(struct irInstructionAllocator *ir, struct irRegister *op)
{
    return add_cast(ir, op, IR_CAST_8S_TO_16, IR_REG_16);
}
static struct irRegister *add_8S_to_32(struct irInstructionAllocator *ir, struct irRegister *op)
{
    return add_cast(ir, op, IR_CAST_8S_TO_32, IR_REG_32);
}
static struct irRegister *add_8S_to_64(struct irInstructionAllocator *ir, struct irRegister *op)
{
    return add_cast(ir, op, IR_CAST_8S_TO_64, IR_REG_64);
}
static struct irRegister *add_16U_to_32(struct irInstructionAllocator *ir, struct irRegister *op)
{
    return add_cast(ir, op, IR_CAST_16U_TO_32, IR_REG_32);
}
static struct irRegister *add_16U_to_64(struct irInstructionAllocator *ir, struct irRegister *op)
{
    return add_cast(ir, op, IR_CAST_16U_TO_64, IR_REG_64);
}
static struct irRegister *add_16S_to_32(struct irInstructionAllocator *ir, struct irRegister *op)
{
    return add_cast(ir, op, IR_CAST_16S_TO_32, IR_REG_32);
}
static struct irRegister *add_16S_to_64(struct irInstructionAllocator *ir, struct irRegister *op)
{
    return add_cast(ir, op, IR_CAST_16S_TO_64, IR_REG_64);
}
static struct irRegister *add_32U_to_64(struct irInstructionAllocator *ir, struct irRegister *op)
{
    return add_cast(ir, op, IR_CAST_32U_TO_64, IR_REG_64);
}
static struct irRegister *add_32S_to_64(struct irInstructionAllocator *ir, struct irRegister *op)
{
    return add_cast(ir, op, IR_CAST_32S_TO_64, IR_REG_64);
}
static struct irRegister *add_64_to_8(struct irInstructionAllocator *ir, struct irRegister *op)
{
    return add_cast(ir, op, IR_CAST_64_TO_8, IR_REG_8);
}
static struct irRegister *add_32_to_8(struct irInstructionAllocator *ir, struct irRegister *op)
{
    return add_cast(ir, op, IR_CAST_32_TO_8, IR_REG_8);
}
static struct irRegister *add_16_to_8(struct irInstructionAllocator *ir, struct irRegister *op)
{
    return add_cast(ir, op, IR_CAST_16_TO_8, IR_REG_8);
}
static struct irRegister *add_64_to_16(struct irInstructionAllocator *ir, struct irRegister *op)
{
    return add_cast(ir, op, IR_CAST_64_TO_16, IR_REG_16);
}
static struct irRegister *add_32_to_16(struct irInstructionAllocator *ir, struct irRegister *op)
{
    return add_cast(ir, op, IR_CAST_32_TO_16, IR_REG_16);
}
static struct irRegister *add_64_to_32(struct irInstructionAllocator *ir, struct irRegister *op)
{
    return add_cast(ir, op, IR_CAST_64_TO_32, IR_REG_32);
}

static void add_exit_cond(struct irInstructionAllocator *irAlloc, struct irRegister *exitValue, struct irRegister *pred)
{
    struct jitter *jitter = container_of(irAlloc, struct jitter, irInstructionAllocator);
    struct memoryPool *pool = &jitter->instructionPoolAllocator;
    struct irInstruction *insn = (struct irInstruction *) pool->alloc(pool, sizeof(struct irInstruction));

    exitValue->lastReadIndex = jitter->instructionIndex;
    if (pred)
        pred->lastReadIndex = jitter->instructionIndex;

    assert(exitValue->type == IR_REG_64);
    insn->type = IR_EXIT;
    insn->u.exit.value = exitValue;
    insn->u.exit.pred = pred;

    jitter->instructionIndex++;
}

static void add_exit(struct irInstructionAllocator *ir, struct irRegister *exitValue)
{
    add_exit_cond(ir, exitValue, NULL);
}

static struct irRegister *add_read_context(struct irInstructionAllocator *irAlloc, int32_t offset, enum irInstructionType insnType, enum irRegisterType regType)
{
    struct jitter *jitter = container_of(irAlloc, struct jitter, irInstructionAllocator);
    struct memoryPool *pool = &jitter->instructionPoolAllocator;
    struct irRegister *dst = allocateRegister(irAlloc, regType);
    struct irInstruction *insn = (struct irInstruction *) pool->alloc(pool, sizeof(struct irInstruction));

    insn->type = insnType;
    insn->u.read_context.dst = dst;
    insn->u.read_context.offset = offset;

    jitter->instructionIndex++;

    return dst;
}

static struct irRegister *add_read_context_8(struct irInstructionAllocator *irAlloc, int32_t offset)
{
    return add_read_context(irAlloc, offset, IR_READ_8, IR_REG_8);
}
static struct irRegister *add_read_context_16(struct irInstructionAllocator *irAlloc, int32_t offset)
{
    return add_read_context(irAlloc, offset, IR_READ_16, IR_REG_16);
}
static struct irRegister *add_read_context_32(struct irInstructionAllocator *irAlloc, int32_t offset)
{
    return add_read_context(irAlloc, offset, IR_READ_32, IR_REG_32);
}
static struct irRegister *add_read_context_64(struct irInstructionAllocator *irAlloc, int32_t offset)
{
    return add_read_context(irAlloc, offset, IR_READ_64, IR_REG_64);
}

static void add_write_context(struct irInstructionAllocator *irAlloc, struct irRegister *src, int32_t offset, enum irInstructionType insnType)
{
    struct jitter *jitter = container_of(irAlloc, struct jitter, irInstructionAllocator);
    struct memoryPool *pool = &jitter->instructionPoolAllocator;
    struct irInstruction *insn = (struct irInstruction *) pool->alloc(pool, sizeof(struct irInstruction));

    src->lastReadIndex = jitter->instructionIndex;

    insn->type = insnType;
    insn->u.write_context.src = src;
    insn->u.write_context.offset = offset;

    jitter->instructionIndex++;
}

static void add_write_context_8(struct irInstructionAllocator *irAlloc, struct irRegister *src, int32_t offset)
{
    add_write_context(irAlloc, src, offset, IR_WRITE_8);
}
static void add_write_context_16(struct irInstructionAllocator *irAlloc, struct irRegister *src, int32_t offset)
{
    add_write_context(irAlloc, src, offset, IR_WRITE_16);
}
static void add_write_context_32(struct irInstructionAllocator *irAlloc, struct irRegister *src, int32_t offset)
{
    add_write_context(irAlloc, src, offset, IR_WRITE_32);
}
static void add_write_context_64(struct irInstructionAllocator *irAlloc, struct irRegister *src, int32_t offset)
{
    add_write_context(irAlloc, src, offset, IR_WRITE_64);
}

static void add_insn_marker(struct irInstructionAllocator *irAlloc, uint32_t value)
{
    struct jitter *jitter = container_of(irAlloc, struct jitter, irInstructionAllocator);
    struct memoryPool *pool = &jitter->instructionPoolAllocator;
    struct irInstruction *insn = (struct irInstruction *) pool->alloc(pool, sizeof(struct irInstruction));

    insn->type = IR_INSN_MARKER;
    insn->u.marker.value = value;

    jitter->instructionIndex++;
}

static void displayReg(struct irRegister *reg)
{
    if (reg)
        printf("R%d_%d[%d->%d[", reg->index, 1 << (reg->type + 3), reg->firstWriteIndex, reg->lastReadIndex);
    else
        printf("NULL");
}

static void displayInsn(struct irInstruction *insn)
{
    switch(insn->type) {
        case IR_MOV_CONST_8:
        case IR_MOV_CONST_16:
        case IR_MOV_CONST_32:
        case IR_MOV_CONST_64:
            {
                printf("mov_const_%d ", 1 << (insn->type - IR_MOV_CONST_8 + 3));
                displayReg(insn->u.mov.dst);
                printf(", 0x%08lx\n", insn->u.mov.value);
            }
            break;
        case IR_LOAD_8:
        case IR_LOAD_16:
        case IR_LOAD_32:
        case IR_LOAD_64:
            {
                printf("load_%d ", 1 << (insn->type - IR_LOAD_8 + 3));
                displayReg(insn->u.load.dst);
                printf(", [");
                displayReg(insn->u.load.address);
                printf("]\n");
            }
            break;
        case IR_STORE_8:
        case IR_STORE_16:
        case IR_STORE_32:
        case IR_STORE_64:
            {
                printf("store_%d ", 1 << (insn->type - IR_STORE_8 + 3));
                printf("[");
                displayReg(insn->u.store.address);
                printf("], ");
                displayReg(insn->u.store.src);
                printf("\n");
            }
            break;
        case IR_BINOP:
            {
                const char *binopTypeToName[] = {"add", "sub", "xor", "and", "or", "shl", "shr", "asr", "ror", "cmpeq", "cmpne"};
                const char *name = binopTypeToName[insn->u.binop.type / 4];
                int bitNb = 1 << ((insn->u.binop.type % 4) + 3);
                
                printf("%s_%d ", name, bitNb);
                displayReg(insn->u.binop.dst);
                printf(", ");
                displayReg(insn->u.binop.op1);
                printf(", ");
                displayReg(insn->u.binop.op2);
                printf("\n");
            }
            break;
        case IR_CALL_VOID:
        case IR_CALL_8:
        case IR_CALL_16:
        case IR_CALL_32:
        case IR_CALL_64:
            {
                printf("call %s(", insn->u.call.name);
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
                printf("\n");
            }
            break;
        case IR_ITE_8:
        case IR_ITE_16:
        case IR_ITE_32:
        case IR_ITE_64:
            {
                displayReg(insn->u.ite.dst);
                printf(" = ");
                displayReg(insn->u.ite.pred);
                printf("?");
                displayReg(insn->u.ite.trueOp);
                printf(":");
                displayReg(insn->u.ite.falseOp);
                printf("\n");
            }
            break;
        case IR_CAST:
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
                printf("\n");
            }
            break;
        case IR_EXIT:
            {
                printf("exit ");
                displayReg(insn->u.exit.value);
                printf(" if ");
                displayReg(insn->u.exit.pred);
                printf("\n");
            }
            break;
        case IR_READ_8:
        case IR_READ_16:
        case IR_READ_32:
        case IR_READ_64:
            {
                printf("read_context_%d ", 1 << (insn->type - IR_READ_8 + 3));
                displayReg(insn->u.read_context.dst);
                printf(", context[%d]\n",insn->u.read_context.offset);
            }
            break;
        case IR_WRITE_8:
        case IR_WRITE_16:
        case IR_WRITE_32:
        case IR_WRITE_64:
            {
                printf("write_context_%d ", 1 << (insn->type - IR_WRITE_8 + 3));
                printf("context[%d], ",insn->u.write_context.offset);
                displayReg(insn->u.write_context.src);
                printf("\n");
            }
            break;
        case IR_INSN_MARKER:
            printf("start_of_new_instruction\n");
            break;
        default:
            printf("Unknown instruction\n");
    }
}

/* api */
jitContext createJitter(void *memory, struct backend *backend, int size)
{
    struct jitter *jitter;

    /* memory must be at least JITTER_MIN_CONTEXT_SIZE */
    assert(size >= JITTER_MIN_CONTEXT_SIZE);
    jitter = (struct jitter *) memory;
    if (jitter) {
        int pool_mem_size;
        int struct_jitter_size_aligned_16 = ((sizeof(*jitter) + 15) & ~0xf);

        jitter->backend = backend;
        jitter->registerPoolAllocator.alloc = memoryPoolAlloc;
        jitter->registerPoolAllocator.reset = memoryPoolReset;
        jitter->instructionPoolAllocator.alloc = memoryPoolAlloc;
        jitter->instructionPoolAllocator.reset = memoryPoolReset;
        jitter->irInstructionAllocator.add_mov_const_8 = add_mov_const_8;
        jitter->irInstructionAllocator.add_mov_const_16 = add_mov_const_16;
        jitter->irInstructionAllocator.add_mov_const_32 = add_mov_const_32;
        jitter->irInstructionAllocator.add_mov_const_64 = add_mov_const_64;
        jitter->irInstructionAllocator.add_load_8 = add_load_8;
        jitter->irInstructionAllocator.add_load_16 = add_load_16;
        jitter->irInstructionAllocator.add_load_32 = add_load_32;
        jitter->irInstructionAllocator.add_load_64 = add_load_64;
        jitter->irInstructionAllocator.add_store_8 = add_store_8;
        jitter->irInstructionAllocator.add_store_16 = add_store_16;
        jitter->irInstructionAllocator.add_store_32 = add_store_32;
        jitter->irInstructionAllocator.add_store_64 = add_store_64;
        jitter->irInstructionAllocator.add_call_void = add_call_void;
        jitter->irInstructionAllocator.add_call_8 = add_call_8;
        jitter->irInstructionAllocator.add_call_16 = add_call_16;
        jitter->irInstructionAllocator.add_call_32 = add_call_32;
        jitter->irInstructionAllocator.add_call_64 = add_call_64;
        jitter->irInstructionAllocator.add_add_8 = add_add_8;
        jitter->irInstructionAllocator.add_add_16 = add_add_16;
        jitter->irInstructionAllocator.add_add_32 = add_add_32;
        jitter->irInstructionAllocator.add_add_64 = add_add_64;
        jitter->irInstructionAllocator.add_sub_8 = add_sub_8;
        jitter->irInstructionAllocator.add_sub_16 = add_sub_16;
        jitter->irInstructionAllocator.add_sub_32 = add_sub_32;
        jitter->irInstructionAllocator.add_sub_64 = add_sub_64;
        jitter->irInstructionAllocator.add_xor_8 = add_xor_8;
        jitter->irInstructionAllocator.add_xor_16 = add_xor_16;
        jitter->irInstructionAllocator.add_xor_32 = add_xor_32;
        jitter->irInstructionAllocator.add_xor_64 = add_xor_64;
        jitter->irInstructionAllocator.add_and_8 = add_and_8;
        jitter->irInstructionAllocator.add_and_16 = add_and_16;
        jitter->irInstructionAllocator.add_and_32 = add_and_32;
        jitter->irInstructionAllocator.add_and_64 = add_and_64;
        jitter->irInstructionAllocator.add_or_8 = add_or_8;
        jitter->irInstructionAllocator.add_or_16 = add_or_16;
        jitter->irInstructionAllocator.add_or_32 = add_or_32;
        jitter->irInstructionAllocator.add_or_64 = add_or_64;
        jitter->irInstructionAllocator.add_shl_8 = add_shl_8;
        jitter->irInstructionAllocator.add_shl_16 = add_shl_16;
        jitter->irInstructionAllocator.add_shl_32 = add_shl_32;
        jitter->irInstructionAllocator.add_shl_64 = add_shl_64;
        jitter->irInstructionAllocator.add_shr_8 = add_shr_8;
        jitter->irInstructionAllocator.add_shr_16 = add_shr_16;
        jitter->irInstructionAllocator.add_shr_32 = add_shr_32;
        jitter->irInstructionAllocator.add_shr_64 = add_shr_64;
        jitter->irInstructionAllocator.add_asr_8 = add_asr_8;
        jitter->irInstructionAllocator.add_asr_16 = add_asr_16;
        jitter->irInstructionAllocator.add_asr_32 = add_asr_32;
        jitter->irInstructionAllocator.add_asr_64 = add_asr_64;
        jitter->irInstructionAllocator.add_ror_8 = add_ror_8;
        jitter->irInstructionAllocator.add_ror_16 = add_ror_16;
        jitter->irInstructionAllocator.add_ror_32 = add_ror_32;
        jitter->irInstructionAllocator.add_ror_64 = add_ror_64;
        jitter->irInstructionAllocator.add_ite_8 = add_ite_8;
        jitter->irInstructionAllocator.add_ite_16 = add_ite_16;
        jitter->irInstructionAllocator.add_ite_32 = add_ite_32;
        jitter->irInstructionAllocator.add_ite_64 = add_ite_64;
        jitter->irInstructionAllocator.add_cmpeq_8 = add_cmpeq_8;
        jitter->irInstructionAllocator.add_cmpeq_16 = add_cmpeq_16;
        jitter->irInstructionAllocator.add_cmpeq_32 = add_cmpeq_32;
        jitter->irInstructionAllocator.add_cmpeq_64 = add_cmpeq_64;
        jitter->irInstructionAllocator.add_cmpne_8 = add_cmpne_8;
        jitter->irInstructionAllocator.add_cmpne_16 = add_cmpne_16;
        jitter->irInstructionAllocator.add_cmpne_32 = add_cmpne_32;
        jitter->irInstructionAllocator.add_cmpne_64 = add_cmpne_64;
        jitter->irInstructionAllocator.add_8U_to_16 = add_8U_to_16;
        jitter->irInstructionAllocator.add_8U_to_32 = add_8U_to_32;
        jitter->irInstructionAllocator.add_8U_to_64 = add_8U_to_64;
        jitter->irInstructionAllocator.add_8S_to_16 = add_8S_to_16;
        jitter->irInstructionAllocator.add_8S_to_32 = add_8S_to_32;
        jitter->irInstructionAllocator.add_8S_to_64 = add_8S_to_64;
        jitter->irInstructionAllocator.add_16U_to_32 = add_16U_to_32;
        jitter->irInstructionAllocator.add_16U_to_64 = add_16U_to_64;
        jitter->irInstructionAllocator.add_16S_to_32 = add_16S_to_32;
        jitter->irInstructionAllocator.add_16S_to_64 = add_16S_to_64;
        jitter->irInstructionAllocator.add_32U_to_64 = add_32U_to_64;
        jitter->irInstructionAllocator.add_32S_to_64 = add_32S_to_64;
        jitter->irInstructionAllocator.add_64_to_8 = add_64_to_8;
        jitter->irInstructionAllocator.add_32_to_8 = add_32_to_8;
        jitter->irInstructionAllocator.add_16_to_8 = add_16_to_8;
        jitter->irInstructionAllocator.add_64_to_16 = add_64_to_16;
        jitter->irInstructionAllocator.add_32_to_16 = add_32_to_16;
        jitter->irInstructionAllocator.add_64_to_32 = add_64_to_32;
        jitter->irInstructionAllocator.add_exit = add_exit;
        jitter->irInstructionAllocator.add_exit_cond = add_exit_cond;
        jitter->irInstructionAllocator.add_read_context_8 = add_read_context_8;
        jitter->irInstructionAllocator.add_read_context_16 = add_read_context_16;
        jitter->irInstructionAllocator.add_read_context_32 = add_read_context_32;
        jitter->irInstructionAllocator.add_read_context_64 = add_read_context_64;
        jitter->irInstructionAllocator.add_write_context_8 = add_write_context_8;
        jitter->irInstructionAllocator.add_write_context_16 = add_write_context_16;
        jitter->irInstructionAllocator.add_write_context_32 = add_write_context_32;
        jitter->irInstructionAllocator.add_write_context_64 = add_write_context_64;
        jitter->irInstructionAllocator.add_insn_marker = add_insn_marker;

        /* setup pool memory */
        pool_mem_size = (size - struct_jitter_size_aligned_16) / 2;
        memoryPoolInit(&jitter->registerPoolAllocator, memory + struct_jitter_size_aligned_16, pool_mem_size);
        memoryPoolInit(&jitter->instructionPoolAllocator, memory + struct_jitter_size_aligned_16 + pool_mem_size, pool_mem_size);

        resetJitter((jitContext) jitter);
    }

    return (jitContext) jitter;
}

void deleteJitter(jitContext handle)
{
    ;
}

void resetJitter(jitContext handle)
{
    struct jitter *jitter = (struct jitter *) handle;

    jitter->backend->reset(jitter->backend);
    jitter->registerPoolAllocator.reset(&jitter->registerPoolAllocator);
    jitter->instructionPoolAllocator.reset(&jitter->instructionPoolAllocator);
    jitter->instructionIndex = 0;
    jitter->regIndex = 0;
}

struct irInstructionAllocator *getIrInstructionAllocator(jitContext handle) {
    struct jitter *jitter = (struct jitter *) handle;

    return &jitter->irInstructionAllocator;
}

void displayIr(jitContext handle)
{
    struct jitter *jitter = (struct jitter *) handle;
    int insnNb = jitter->instructionPoolAllocator.index / sizeof(struct irInstruction);
    struct irInstruction *insn = (struct irInstruction *) jitter->instructionPoolAllocator.buffer;
    int i;

    for(i=0;i<insnNb;i++) {
        printf("%2d : ", i);
        displayInsn(insn++);
    }
}

int jitCode(jitContext handle, char *buffer, int bufferSize)
{
    struct jitter *jitter = (struct jitter *) handle;
    int insnNb = jitter->instructionPoolAllocator.index / sizeof(struct irInstruction);
    struct irInstruction *irArray = (struct irInstruction *) jitter->instructionPoolAllocator.buffer;

    return jitter->backend->jit(jitter->backend, irArray, insnNb, buffer, bufferSize);
}

int findInsn(jitContext handle, char *buffer, int bufferSize, int offset)
{
    struct jitter *jitter = (struct jitter *) handle;
    int insnNb = jitter->instructionPoolAllocator.index / sizeof(struct irInstruction);
    struct irInstruction *irArray = (struct irInstruction *) jitter->instructionPoolAllocator.buffer;

    return jitter->backend->get_marker(jitter->backend, irArray, insnNb, buffer, bufferSize, offset);
}
