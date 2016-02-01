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
#include "jitter_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __JITTER__
#define __JITTER__ 1

#define JITTER_MIN_CONTEXT_SIZE     (32 * 1024)

typedef void *jitContext;

struct irInstructionAllocator {
    struct irRegister *(*add_mov_const_8)(struct irInstructionAllocator *, uint64_t value);
    struct irRegister *(*add_mov_const_16)(struct irInstructionAllocator *, uint64_t value);
    struct irRegister *(*add_mov_const_32)(struct irInstructionAllocator *, uint64_t value);
    struct irRegister *(*add_mov_const_64)(struct irInstructionAllocator *, uint64_t value);
    struct irRegister *(*add_load_8)(struct irInstructionAllocator *, struct irRegister *address);
    struct irRegister *(*add_load_16)(struct irInstructionAllocator *, struct irRegister *address);
    struct irRegister *(*add_load_32)(struct irInstructionAllocator *, struct irRegister *address);
    struct irRegister *(*add_load_64)(struct irInstructionAllocator *, struct irRegister *address);
    void (*add_store_8)(struct irInstructionAllocator *, struct irRegister *src, struct irRegister *address);
    void (*add_store_16)(struct irInstructionAllocator *, struct irRegister *src, struct irRegister *address);
    void (*add_store_32)(struct irInstructionAllocator *, struct irRegister *src, struct irRegister *address);
    void (*add_store_64)(struct irInstructionAllocator *, struct irRegister *src, struct irRegister *address);
    void (*add_call_void)(struct irInstructionAllocator *, char *name, struct irRegister *address, struct irRegister *param[4]);
    struct irRegister *(*add_call_8)(struct irInstructionAllocator *, char *name, struct irRegister *address, struct irRegister *param[4]);
    struct irRegister *(*add_call_16)(struct irInstructionAllocator *, char *name, struct irRegister *address, struct irRegister *param[4]);
    struct irRegister *(*add_call_32)(struct irInstructionAllocator *, char *name, struct irRegister *address, struct irRegister *param[4]);
    struct irRegister *(*add_call_64)(struct irInstructionAllocator *, char *name, struct irRegister *address, struct irRegister *param[4]);
    struct irRegister *(*add_add_8)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_add_16)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_add_32)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_add_64)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_sub_8)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_sub_16)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_sub_32)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_sub_64)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_xor_8)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_xor_16)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_xor_32)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_xor_64)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_and_8)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_and_16)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_and_32)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_and_64)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_or_8)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_or_16)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_or_32)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_or_64)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    /* shift value in op2 is only defined when it's < operator width. Else result is not defined. */
    struct irRegister *(*add_shl_8)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_shl_16)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_shl_32)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_shl_64)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_shr_8)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_shr_16)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_shr_32)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_shr_64)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_asr_8)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_asr_16)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_asr_32)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_asr_64)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_ror_8)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_ror_16)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_ror_32)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_ror_64)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_ite_8)(struct irInstructionAllocator *, struct irRegister *pred, struct irRegister *trueOp, struct irRegister *falseOp);
    struct irRegister *(*add_ite_16)(struct irInstructionAllocator *, struct irRegister *pred, struct irRegister *trueOp, struct irRegister *falseOp);
    struct irRegister *(*add_ite_32)(struct irInstructionAllocator *, struct irRegister *pred, struct irRegister *trueOp, struct irRegister *falseOp);
    struct irRegister *(*add_ite_64)(struct irInstructionAllocator *, struct irRegister *pred, struct irRegister *trueOp, struct irRegister *falseOp);
    struct irRegister *(*add_cmpeq_8)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_cmpeq_16)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_cmpeq_32)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_cmpeq_64)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_cmpne_8)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_cmpne_16)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_cmpne_32)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_cmpne_64)(struct irInstructionAllocator *, struct irRegister *op1, struct irRegister *op2);
    struct irRegister *(*add_8U_to_16)(struct irInstructionAllocator *, struct irRegister *op);
    struct irRegister *(*add_8U_to_32)(struct irInstructionAllocator *, struct irRegister *op);
    struct irRegister *(*add_8U_to_64)(struct irInstructionAllocator *, struct irRegister *op);
    struct irRegister *(*add_8S_to_16)(struct irInstructionAllocator *, struct irRegister *op);
    struct irRegister *(*add_8S_to_32)(struct irInstructionAllocator *, struct irRegister *op);
    struct irRegister *(*add_8S_to_64)(struct irInstructionAllocator *, struct irRegister *op);
    struct irRegister *(*add_16U_to_32)(struct irInstructionAllocator *, struct irRegister *op);
    struct irRegister *(*add_16U_to_64)(struct irInstructionAllocator *, struct irRegister *op);
    struct irRegister *(*add_16S_to_32)(struct irInstructionAllocator *, struct irRegister *op);
    struct irRegister *(*add_16S_to_64)(struct irInstructionAllocator *, struct irRegister *op);
    struct irRegister *(*add_32U_to_64)(struct irInstructionAllocator *, struct irRegister *op);
    struct irRegister *(*add_32S_to_64)(struct irInstructionAllocator *, struct irRegister *op);
    struct irRegister *(*add_64_to_8)(struct irInstructionAllocator *, struct irRegister *op);
    struct irRegister *(*add_32_to_8)(struct irInstructionAllocator *, struct irRegister *op);
    struct irRegister *(*add_16_to_8)(struct irInstructionAllocator *, struct irRegister *op);
    struct irRegister *(*add_64_to_16)(struct irInstructionAllocator *, struct irRegister *op);
    struct irRegister *(*add_32_to_16)(struct irInstructionAllocator *, struct irRegister *op);
    struct irRegister *(*add_64_to_32)(struct irInstructionAllocator *, struct irRegister *op);
    void (*add_exit)(struct irInstructionAllocator *, struct irRegister *exitValue);
    void (*add_exit_cond)(struct irInstructionAllocator *, struct irRegister *exitValue, struct irRegister *pred);
    struct irRegister *(*add_read_context_8)(struct irInstructionAllocator *, int32_t offset);
    struct irRegister *(*add_read_context_16)(struct irInstructionAllocator *, int32_t offset);
    struct irRegister *(*add_read_context_32)(struct irInstructionAllocator *, int32_t offset);
    struct irRegister *(*add_read_context_64)(struct irInstructionAllocator *, int32_t offset);
    void (*add_write_context_8)(struct irInstructionAllocator *, struct irRegister *src, int32_t offset);
    void (*add_write_context_16)(struct irInstructionAllocator *, struct irRegister *src, int32_t offset);
    void (*add_write_context_32)(struct irInstructionAllocator *, struct irRegister *src, int32_t offset);
    void (*add_write_context_64)(struct irInstructionAllocator *, struct irRegister *src, int32_t offset);
    void (*add_insn_marker)(struct irInstructionAllocator *, uint32_t value);
};

struct backend_execute_result {
    uint64_t result;
    void *link_patch_area;
};

struct backend {
    int (*jit)(struct backend *backend, struct irInstruction *irArray, int irInsnNb, char *buffer, int bufferSize);
    void (*reset)(struct backend *backend);
    struct backend_execute_result (*execute)(struct backend *backend, char *buffer, uint64_t context);
    void (*request_signal_alternate_exit)(struct backend *backend, void *ucp, uint64_t result);
    uint32_t (*get_marker)(struct backend *backend, struct irInstruction *irArray, int irInsnNb, char *buffer, int bufferSize, int offset);
    void (*patch)(struct backend *backend, void *link_patch_area, void *cache_area);
};

/* jitter public api */
/* Create a new jitter instance */
jitContext createJitter(void *memory, struct backend *backend, int size);
/* Delete a jitter instance */
void deleteJitter(jitContext);
/* Reset a jitter instance so it can jit a new sequence of code */
void resetJitter(jitContext);
/* Return an struct irInstructionAllocator pointer to describe a sequence of instruction for a jitter instance */
struct irInstructionAllocator *getIrInstructionAllocator(jitContext);
/* Print current instruction sequence */
void displayIr(jitContext handle);
/* Translate ir instruction sequence into buffer area using backend */
int jitCode(jitContext handle, char *buffer, int bufferSize);
/* Find guest insn address offset that is associated with byte located at buffer + offset */
int findInsn(jitContext handle, char *buffer, int bufferSize, int offset);

#endif

#ifdef __cplusplus
}
#endif
