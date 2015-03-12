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

#include <stdio.h>
#include <assert.h>
#include <stddef.h>

#include "arm_private.h"
#include "arm_helpers.h"
#include "runtime.h"
#include "cache.h"

//#define DUMP_STATE  1
#define INSN(msb, lsb) ((insn >> (lsb)) & ((1 << ((msb) - (lsb) + 1))-1))

/* sequence shortcut */
static void dump_state(struct arm_target *context, struct irInstructionAllocator *ir)
{
#if DUMP_STATE
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};

    ir->add_call_void(ir, "arm_hlp_dump",
                      ir->add_mov_const_64(ir, (uint64_t) arm_hlp_dump),
                      param);
#endif
}
/*
static void dump_state_and_assert(struct arm_target *context, struct irInstructionAllocator *ir)
{
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};

    ir->add_call_void(ir, "arm_hlp_dump_and_assert",
                      ir->add_mov_const_64(ir, (uint64_t) arm_hlp_dump_and_assert),
                      param);
}
*/

static uint32_t armExpandImm(int imm12)
{
    int shift_value = ((imm12 >> 8) & 0xf) * 2;
    uint32_t unrotated = imm12 & 0xff;
    uint32_t res;

    res = (unrotated >> shift_value) | ( unrotated << (32 - shift_value));

    return res;
}

static struct irRegister *mk_8(struct irInstructionAllocator *ir, uint8_t value)
{
    return ir->add_mov_const_8(ir, value);
}
static struct irRegister *mk_16(struct irInstructionAllocator *ir, uint16_t value)
{
    return ir->add_mov_const_16(ir, value);
}
static struct irRegister *mk_32(struct irInstructionAllocator *ir, uint32_t value)
{
    return ir->add_mov_const_32(ir, value);
}
static struct irRegister *mk_64(struct irInstructionAllocator *ir, uint64_t value)
{
    return ir->add_mov_const_64(ir, value);
}

static struct irRegister *read_reg(struct arm_target *context, struct irInstructionAllocator *ir, int index)
{
    if (index == 15)
        return mk_32(ir, context->pc + 8);
    else
        return ir->add_read_context_32(ir, offsetof(struct arm_registers, r[index]));
}

static void write_reg(struct arm_target *context, struct irInstructionAllocator *ir, int index, struct irRegister *value)
{
    ir->add_write_context_32(ir, value, offsetof(struct arm_registers, r[index]));
}

static struct irRegister *read_reg_s(struct arm_target *context, struct irInstructionAllocator *ir, int index)
{
    return ir->add_read_context_32(ir, offsetof(struct arm_registers, e.s[index]));
}

static void write_reg_s(struct arm_target *context, struct irInstructionAllocator *ir, int index, struct irRegister *value)
{
    ir->add_write_context_32(ir, value, offsetof(struct arm_registers, e.s[index]));
}

static struct irRegister *read_reg_d(struct arm_target *context, struct irInstructionAllocator *ir, int index)
{
    return ir->add_read_context_64(ir, offsetof(struct arm_registers, e.d[index]));
}

static void write_reg_d(struct arm_target *context, struct irInstructionAllocator *ir, int index, struct irRegister *value)
{
    ir->add_write_context_64(ir, value, offsetof(struct arm_registers, e.d[index]));
}

static struct irRegister *read_cpsr(struct arm_target *context, struct irInstructionAllocator *ir)
{
    return ir->add_read_context_32(ir, offsetof(struct arm_registers, cpsr));
}

static void write_cpsr(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *value)
{
    ir->add_write_context_32(ir, value, offsetof(struct arm_registers, cpsr));
}

static struct irRegister *read_sco(struct arm_target *context, struct irInstructionAllocator *ir)
{
    return ir->add_read_context_32(ir, offsetof(struct arm_registers, shifter_carry_out));
}

static void write_sco(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *value)
{
    ir->add_write_context_32(ir, value, offsetof(struct arm_registers, shifter_carry_out));
}

static struct irRegister *mk_address(struct irInstructionAllocator *ir, struct irRegister *address)
{
    if (mmap_offset)
        return ir->add_add_64(ir, ir->add_32U_to_64(ir, address), ir->add_mov_const_64(ir, mmap_offset));
    else
        return address;
}

static struct irRegister *address_register_offset(struct arm_target *context, struct irInstructionAllocator *ir, int index, int offset)
{
    struct irRegister *res;

    if (index == 15) {
        res = ir->add_mov_const_32(ir, context->pc + 8 + offset);
    } else {
        if (offset > 0) {
            res = ir->add_add_32(ir,
                                 read_reg(context, ir, index),
                                 ir->add_mov_const_32(ir, offset));
        } else if (offset < 0) {
            res = ir->add_sub_32(ir,
                                 read_reg(context, ir, index),
                                 ir->add_mov_const_32(ir, -offset));
        } else {
            res = read_reg(context, ir, index);
        }
    }

    return res;
}

static int mk_data_processing(struct arm_target *context, struct irInstructionAllocator *ir, int insn, struct irRegister *op2)
{
    int opcode = INSN(24, 21);
    int s = INSN(20, 20);
    int rn = INSN(19, 16);
    int rd = INSN(15, 12);
    struct irRegister *nextCpsr = NULL;
    struct irRegister *op1 = read_reg(context, ir, rn);
    int isExit = (rd == 15)?1:0;
    struct irRegister *result = NULL;

    if (s) {
        struct irRegister *params[4];

        params[0] = ir->add_or_32(ir,
                                  ir->add_mov_const_32(ir, opcode),
                                  read_sco(context, ir));
        params[1] = op1;
        params[2] = op2;
        params[3] = read_cpsr(context, ir);

        nextCpsr = ir->add_call_32(ir, "arm_hlp_compute_next_flags",
                                   ir->add_mov_const_64(ir, (uint64_t) arm_hlp_compute_next_flags),
                                   params);
    }
    switch(opcode) {
        case 0://and
            result = ir->add_and_32(ir, op1, op2);
            break;
        case 1://eor
            result = ir->add_xor_32(ir, op1, op2);
            break;
        case 2://sub
            result = ir->add_sub_32(ir, op1, op2);
            break;
        case 3://rsb
            result = ir->add_sub_32(ir, op2, op1);
            break;
        case 4://add
            result = ir->add_add_32(ir, op1, op2);
            break;
        case 5://adc
            {
                struct irRegister *c = ir->add_and_32(ir,
                                                      ir->add_shr_32(ir, read_cpsr(context, ir), mk_8(ir, 29)),
                                                      mk_32(ir, 1));
                result = ir->add_add_32(ir, op1, ir->add_add_32(ir, op2, c));
            }
            break;
        case 6://sbc
            {
                struct irRegister *c = ir->add_and_32(ir,
                                                      ir->add_shr_32(ir, read_cpsr(context, ir), mk_8(ir, 29)),
                                                      mk_32(ir, 1));
                result = ir->add_add_32(ir, op1, ir->add_add_32(ir, ir->add_xor_32(ir, op2, mk_32(ir, 0xffffffff)), c));
            }
            break;
        case 7://rsc
            {
                struct irRegister *c = ir->add_and_32(ir,
                                                      ir->add_shr_32(ir, read_cpsr(context, ir), mk_8(ir, 29)),
                                                      mk_32(ir, 1));
                result = ir->add_add_32(ir, op2, ir->add_add_32(ir, ir->add_xor_32(ir, op1, mk_32(ir, 0xffffffff)), c));
            }
            break;
        case 8://tst
        case 9://teq
        case 10://cmp
        case 11://cmn
            break;
        case 12://orr
            result = ir->add_or_32(ir, op1, op2);
            break;
        case 13://mov
            result = op2;
            break;
        case 14://bic
            result = ir->add_and_32(ir,
                                    op1,
                                    ir->add_xor_32(ir, op2, mk_32(ir, 0xffffffff)));
            break;
        case 15://mvn
            result = ir->add_xor_32(ir, op2, mk_32(ir, 0xffffffff));
            break;
        default:
            assert(0);
    }
    if (result)
        write_reg(context, ir, rd, result);
    if (s) {
        write_cpsr(context, ir, nextCpsr);
    }

    if (isExit && result) {
        ir->add_exit(ir, ir->add_32U_to_64(ir, result));
    }

    return isExit;
}

static struct irRegister *mk_ror_imm_32(struct irInstructionAllocator *ir, struct irRegister *op, int rotation)
{
    return ir->add_or_32(ir,
                         ir->add_shr_32(ir, op, mk_8(ir, rotation)),
                         ir->add_shl_32(ir, op, mk_8(ir, 32-rotation)));
}

static struct irRegister *mk_sco(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *insn,
                                 struct irRegister *rm, struct irRegister * shift_op)
{
    struct irRegister *params[4];

    params[0] = insn;
    params[1] = rm;
    params[2] = shift_op;
    params[3] = read_cpsr(context, ir);

    return ir->add_call_32(ir, "arm_hlp_compute_sco",
                               ir->add_mov_const_64(ir, (uint64_t) arm_hlp_compute_sco),
                               params);
}

static struct irRegister *mk_media_rotate(struct irInstructionAllocator *ir, struct irRegister *op, int rotation)
{
    static struct irRegister *res;

    if (rotation) {
        res = mk_ror_imm_32(ir, op, rotation);
    } else {
        res = op;
    }

    return res;
}

static struct irRegister *mk_ror_reg_32(struct irInstructionAllocator *ir, struct irRegister *op, struct irRegister *rs)
{
    struct irRegister *rs_mask = ir->add_and_32(ir, rs, mk_32(ir, 0x1f));
    struct irRegister *rotate_r = ir->add_32_to_8(ir, rs_mask);
    struct irRegister *rotate_l = ir->add_32_to_8(ir, ir->add_sub_32(ir, mk_32(ir, 32), rs_mask));

    return ir->add_or_32(ir,
                         ir->add_shr_32(ir, op, rotate_r),
                         ir->add_shl_32(ir, op, rotate_l));
}

static struct irRegister *mk_mul_u_lsb(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *op1, struct irRegister *op2)
{
    struct irRegister *param[4] = {op1, op2, NULL, NULL};

    return ir->add_call_32(ir, "arm_hlp_multiply_unsigned_lsb",
                           ir->add_mov_const_64(ir, (uint64_t) arm_hlp_multiply_unsigned_lsb),
                           param);
}

static struct irRegister *mk_mul_flag_update(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *res, struct irRegister *old_cpsr)
{
    struct irRegister *param[4] = {res, old_cpsr, NULL, NULL};

    return ir->add_call_32(ir, "arm_hlp_multiply_flag_update",
                           ir->add_mov_const_64(ir, (uint64_t) arm_hlp_multiply_flag_update),
                           param);
}

static struct irRegister *mk_mul_u_msb(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *op1, struct irRegister *op2)
{
    struct irRegister *param[4] = {op1, op2, NULL, NULL};

    return ir->add_call_32(ir, "arm_hlp_multiply_unsigned_msb",
                           ir->add_mov_const_64(ir, (uint64_t) arm_hlp_multiply_unsigned_msb),
                           param);
}

static struct irRegister *mk_mul_s_lsb(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *op1, struct irRegister *op2)
{
    struct irRegister *param[4] = {op1, op2, NULL, NULL};

    return ir->add_call_32(ir, "arm_hlp_multiply_signed_lsb",
                           ir->add_mov_const_64(ir, (uint64_t) arm_hlp_multiply_signed_lsb),
                           param);
}

static struct irRegister *mk_mul_s_msb(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *op1, struct irRegister *op2)
{
    struct irRegister *param[4] = {op1, op2, NULL, NULL};

    return ir->add_call_32(ir, "arm_hlp_multiply_signed_msb",
                           ir->add_mov_const_64(ir, (uint64_t) arm_hlp_multiply_signed_msb),
                           param);
}

static struct irRegister *mk_mula_u_lsb(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *op1, struct irRegister *op2,
                                        struct irRegister *rdhi, struct irRegister *rdlow)
{
    struct irRegister *param[4] = {op1, op2, rdhi, rdlow};

    return ir->add_call_32(ir, "arm_hlp_multiply_accumulate_unsigned_lsb",
                           ir->add_mov_const_64(ir, (uint64_t) arm_hlp_multiply_accumulate_unsigned_lsb),
                           param);
}

static struct irRegister *mk_mula_u_msb(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *op1, struct irRegister *op2,
                                        struct irRegister *rdhi, struct irRegister *rdlow)
{
    struct irRegister *param[4] = {op1, op2, rdhi, rdlow};

    return ir->add_call_32(ir, "arm_hlp_multiply_accumulate_unsigned_msb",
                           ir->add_mov_const_64(ir, (uint64_t) arm_hlp_multiply_accumulate_unsigned_msb),
                           param);
}

static struct irRegister *mk_mula_s_lsb(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *op1, struct irRegister *op2,
                                        struct irRegister *rdhi, struct irRegister *rdlow)
{
    struct irRegister *param[4] = {op1, op2, rdhi, rdlow};

    return ir->add_call_32(ir, "arm_hlp_multiply_accumulate_signed_lsb",
                           ir->add_mov_const_64(ir, (uint64_t) arm_hlp_multiply_accumulate_signed_lsb),
                           param);
}

static struct irRegister *mk_mula_s_msb(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *op1, struct irRegister *op2,
                                        struct irRegister *rdhi, struct irRegister *rdlow)
{
    struct irRegister *param[4] = {op1, op2, rdhi, rdlow};

    return ir->add_call_32(ir, "arm_hlp_multiply_accumulate_signed_msb",
                           ir->add_mov_const_64(ir, (uint64_t) arm_hlp_multiply_accumulate_signed_msb),
                           param);
}

static void mk_gdb_breakpoint_instruction(struct irInstructionAllocator *ir)
{
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};

    ir->add_call_32(ir, "arm_gdb_breakpoint_instruction",
                        ir->add_mov_const_64(ir, (uint64_t) arm_gdb_breakpoint_instruction),
                        param);
}

/* disassembler to ir */
#define COMMON_ARM          1
#include "arm_common_disassembler.c"

 /* code generators */
static int dis_bl(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int l = INSN(24, 24);
    int32_t offset = INSN(23, 0);
    uint32_t branch_target;

    offset = (offset << 8) >> 6;
    branch_target = context->pc + 8 + offset;
    if (l) {
        write_reg(context, ir, 14, ir->add_mov_const_32(ir, context->pc + 4));
    }
    write_reg(context, ir, 15, ir->add_mov_const_32(ir, branch_target));
    ir->add_exit(ir, ir->add_mov_const_64(ir, branch_target));

    return 1;
}

static int dis_ldm_stm(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int p = INSN(24, 24);
    int u = INSN(23, 23);
    int s = INSN(22, 22);
    int w = INSN(21, 21);
    int l = INSN(20, 20);
    int rn = INSN(19, 16);
    int reglist = INSN(15, 0);
    int bitNb = 0;
    int i;
    struct irRegister *start_address;
    struct irRegister *rn_initial_value = NULL;
    int offset = 0;
    int isExit = 0;
    struct irRegister *newPc = NULL;

    assert(s == 0);

    if (w) {
        rn_initial_value = read_reg(context, ir, rn);
    }
    for(i=0;i<16;i++) {
        if ((reglist >> i) & 1)
            bitNb++;
    }
    if (p) {
        if (u) {
            start_address = ir->add_add_32(ir, read_reg(context, ir, rn), mk_32(ir, 4));
        } else {
            start_address = ir->add_sub_32(ir, read_reg(context, ir, rn), mk_32(ir, 4 * bitNb));
        }
    } else {
        if (u) {
            start_address =read_reg(context, ir, rn);
        } else {
            start_address = ir->add_sub_32(ir, read_reg(context, ir, rn), mk_32(ir, 4 * (bitNb - 1)));
        }
    }
    //load/store registers
    if (l) {
        for(i=0;i<15;i++) {
            if ((reglist >> i) & 1) {
                write_reg(context, ir, i,
                          ir->add_load_32(ir,
                                          mk_address(ir, ir->add_add_32(ir, start_address, mk_32(ir, offset)))));
                offset += 4;
            }
        }
        if ((reglist >> 15) & 1) {
            //pc will be update => block exit
            newPc = ir->add_load_32(ir, mk_address(ir, ir->add_add_32(ir, start_address, mk_32(ir, offset))));

            write_reg(context, ir, 15, newPc);
            isExit = 1;
        }
    } else {
        for(i=0;i<16;i++) {
            if ((reglist >> i) & 1) {
                ir->add_store_32(ir, read_reg(context, ir, i),
                                 mk_address(ir, ir->add_add_32(ir, start_address, mk_32(ir, offset))));
                offset += 4;
            }
        }
    }
    //update rn if needed
    if (w) {
        if (u == 1) {
            write_reg(context, ir, rn,
                      ir->add_add_32(ir, rn_initial_value, mk_32(ir, 4 * bitNb)));
        } else {
            write_reg(context, ir, rn,
                      ir->add_sub_32(ir, rn_initial_value, mk_32(ir, 4 * bitNb)));
        }
    }

    if (isExit)
        ir->add_exit(ir, ir->add_32U_to_64(ir, newPc));

    return isExit;
}

static int dis_data_processing_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int s = INSN(20, 20);
    int rotate = INSN(11, 8);
    uint32_t imm32 = INSN(7, 0);
    struct irRegister *op;

    if (rotate) {
        imm32 = (imm32 >> (rotate * 2)) | (imm32 << (32 - rotate * 2));
    }
    op = mk_32(ir, imm32);

    if (s) {
        if (rotate) {
            write_sco(context, ir, mk_32(ir, (imm32 >> 2) & 0x20000000));
        } else {
            write_sco(context, ir, ir->add_and_32(ir,
                                                  read_cpsr(context, ir),
                                                  mk_32(ir, 0x20000000)));
        }
    }

    return mk_data_processing(context, ir, insn, op);
}

static int dis_movw(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    uint32_t imm32 = (INSN(19, 16) << 12) + INSN(11, 0);

    assert(rd != 15);

    write_reg(context, ir, rd, mk_32(ir, imm32));

    return 0;
}

static int dis_movt(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    uint16_t imm16 = (INSN(19, 16) << 12) + INSN(11, 0);

    assert(rd != 15);

    /* only write msb part of rd */
    ir->add_write_context_16(ir, mk_16(ir, imm16), offsetof(struct arm_registers, r[rd]) + 2);

    return 0;
}

static int dis_load_store_word_and_unsigned_byte_imm_offset(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int p = INSN(24, 24);
    int u = INSN(23, 23);
    int b = INSN(22, 22);
    int w = INSN(21, 21);
    int l = INSN(20, 20);
    int rn = INSN(19, 16);
    int rd = INSN(15, 12);
    uint32_t imm12 = INSN(11, 0);
    struct irRegister *address;
    int isExit = 0;

    assert(p != 0 || w != 1);

    /* compute address of access */
    if (p) {
        //either offset or pre-indexedregs
        address = address_register_offset(context, ir, rn, (u==1?imm12:-imm12));
    } else {
        //post indexed
        address = address_register_offset(context, ir, rn, 0);
    }
    /* make load/store */
    if (l == 1) {
        //load
        if (b)
            write_reg(context, ir, rd, ir->add_8U_to_32(ir, ir->add_load_8(ir, mk_address(ir, address))));
        else
            write_reg(context, ir, rd, ir->add_load_32(ir, mk_address(ir, address)));
    } else {
        //store
        if (b)
            ir->add_store_8(ir, ir->add_32_to_8(ir, read_reg(context, ir, rd)), mk_address(ir, address));
        else
            ir->add_store_32(ir, read_reg(context, ir, rd), mk_address(ir, address));
    }
    /* rn write back if needed */
    if (p && w) {
        //pre-indexed
        write_reg(context, ir, rn, address);
    } else if (!p && !w) {
        //post-indexed
        struct irRegister *newVal;

        newVal = address_register_offset(context, ir, rn, (u==1?imm12:-imm12));
        write_reg(context, ir, rn, newVal);
    }

    /* hande case we load pc */
    if (l && rd == 15) {
        ir->add_exit(ir, ir->add_32U_to_64(ir, ir->add_read_context_32(ir, offsetof(struct arm_registers, r[15]))));
        isExit = 1;
    }

    return isExit;
}

static int dis_load_store_word_and_unsigned_byte_register_offset(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int p = INSN(24, 24);
    int u = INSN(23, 23);
    int b = INSN(22, 22);
    int w = INSN(21, 21);
    int l = INSN(20, 20);
    int rn = INSN(19, 16);
    int rd = INSN(15, 12);
    int shift_imm = INSN(11, 7);
    int shift_mode = INSN(6, 5);
    int rm = INSN(3, 0);
    struct irRegister *address;
    struct irRegister *index;
    int isExit = 0;

    assert(p != 0 || w != 1);

    /* compute index */
    switch(shift_mode) {
        case 0:
            if (shift_imm)
                index = ir->add_shl_32(ir, read_reg(context, ir, rm), mk_8(ir, shift_imm));
            else
                index = read_reg(context, ir, rm);
            break;
        case 1:
            if (shift_imm)
                index = ir->add_shr_32(ir, read_reg(context, ir, rm), mk_8(ir, shift_imm));
            else
                index = mk_32(ir, 0);
            break;
        case 2:
            if (shift_imm)
                index = ir->add_asr_32(ir, read_reg(context, ir, rm), mk_8(ir, shift_imm));
            else
                index = ir->add_asr_32(ir, read_reg(context, ir, rm), mk_8(ir, 32));
            break;
        case 3:
            if (shift_imm)
                index = mk_ror_imm_32(ir, read_reg(context, ir, rm), shift_imm);
            else
                assert(0 && "RRX");
            break;
        default:
            assert(0);
    }

    /* compute address of access */
    if (p) {
        //either offset or pre-indexedregs
        if (u)
            address = ir->add_add_32(ir, read_reg(context, ir, rn), index);
        else
            address = ir->add_sub_32(ir, read_reg(context, ir, rn), index);
    } else {
        //post indexed
        address = read_reg(context, ir, rn);
    }
    /* make load/store */
    if (l == 1) {
        //load
        if (b)
            write_reg(context, ir, rd, ir->add_8U_to_32(ir, ir->add_load_8(ir, mk_address(ir, address))));
        else
            write_reg(context, ir, rd, ir->add_load_32(ir, mk_address(ir, address)));
    } else {
        //store
        if (b)
            ir->add_store_8(ir, ir->add_32_to_8(ir, read_reg(context, ir, rd)), mk_address(ir, address));
        else
            ir->add_store_32(ir, read_reg(context, ir, rd), mk_address(ir, address));
    }
    /* rn write back if needed */
    if (p && w) {
        //pre-indexed
        write_reg(context, ir, rn, address);
    } else if (!p && !w) {
        //post-indexed
        struct irRegister *newVal;

        if (u)
            newVal = ir->add_add_32(ir, read_reg(context, ir, rn), index);
        else
            newVal = ir->add_sub_32(ir, read_reg(context, ir, rn), index);
        write_reg(context, ir, rn, newVal);
    }

    /* hande case we load pc */
    if (l && rd == 15) {
        ir->add_exit(ir, ir->add_32U_to_64(ir, ir->add_read_context_32(ir, offsetof(struct arm_registers, r[15]))));
        isExit = 1;
    }

    return isExit;
}

static int dis_data_processing_register_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int s = INSN(20, 20);
    int shift_imm = INSN(11, 7);
    int shift_mode = INSN(6, 5);
    int rm = INSN(3, 0);
    struct irRegister *op;
    struct irRegister *rm_reg;

    rm_reg = read_reg(context, ir, rm);

    if (s) {
        write_sco(context, ir, mk_sco(context, ir,
                                      mk_32(ir, insn),
                                      rm_reg,
                                      mk_32(ir, shift_imm)));
    }
    switch(shift_mode) {
        case 0://shift left
            if (shift_imm == 0) {
                op = rm_reg;
            } else {
                op = ir->add_shl_32(ir,
                                    rm_reg,
                                    mk_8(ir, shift_imm));
            }
            break;
        case 1://shift right
            op = ir->add_shr_32(ir,
                                rm_reg,
                                mk_8(ir, (shift_imm == 0)?32:shift_imm));
            break;
        case 2://arithmetic shift right
            op = ir->add_asr_32(ir,
                                rm_reg,
                                mk_8(ir, (shift_imm == 0)?32:shift_imm));
            break;
        case 3://rotate right
            if (shift_imm == 0) {
                //extented rotate right
                struct irRegister *c = ir->add_and_32(ir,
                                                      ir->add_shl_32(ir, read_cpsr(context, ir), mk_8(ir, 2)),
                                                      mk_32(ir, 0x80000000));
                op = ir->add_or_32(ir,
                                   ir->add_shr_32(ir, rm_reg, mk_8(ir, 1)),
                                   c);
            } else {
                op = mk_ror_imm_32(ir, rm_reg, shift_imm);
            }
            break;
        default:
            assert(0);
    }

    return mk_data_processing(context, ir, insn, op);
}

static int dis_swi(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};

    /* be sure pc as the correct value so clone syscall can use pc value */
    write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc + 4));
    ir->add_call_void(ir, "arm_hlp_syscall",
                      ir->add_mov_const_64(ir, (uint64_t) arm_hlp_syscall),
                      param);

    write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc + 4));
    ir->add_exit(ir, ir->add_mov_const_64(ir, context->pc + 4));

    return 1;
}

static int dis_bx(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN(3, 0);
    struct irRegister *dst = read_reg(context, ir, rm);

    write_reg(context, ir, 15, dst);
    ir->add_exit(ir, ir->add_32U_to_64(ir, dst));

    return 1;
}

static int dis_clz(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN(3, 0);
    int rd = INSN(15, 12);
    struct irRegister *params[4];
    struct irRegister *result;

    assert(rm != 15);
    assert(rd != 15);

    params[0] = read_reg(context, ir, rm);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    result = ir->add_call_32(ir, "arm_hlp_clz",
                             mk_64(ir, (uint64_t) arm_hlp_clz),
                             params);

    write_reg(context, ir, rd, result);

    return 0;
}

static int dis_uxtb16(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    int rm = INSN(3, 0);
    int rotation = INSN(11, 10) << 3;
    struct irRegister *rm_reg = read_reg(context, ir, rm);
    struct irRegister *rm_rotated;
    struct irRegister *lsb, *msb;
    struct irRegister *result;

    rm_rotated = mk_media_rotate(ir, rm_reg, rotation);
    lsb = ir->add_8U_to_16(ir, ir->add_32_to_8(ir, rm_rotated));
    msb = ir->add_8U_to_16(ir, ir->add_32_to_8(ir, ir->add_shr_32(ir, rm_rotated, mk_8(ir, 16))));
    result = ir->add_or_32(ir,
                           ir->add_16U_to_32(ir, lsb),
                           ir->add_shl_32(ir, ir->add_16U_to_32(ir, msb), mk_8(ir, 16)));
    write_reg(context, ir, rd, result);

    return 0;
}

static int dis_uxtab16(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    int rn = INSN(19, 16);
    int rm = INSN(3, 0);
    int rotation = INSN(11, 10) << 3;
    struct irRegister *rm_reg = read_reg(context, ir, rm);
    struct irRegister *rn_reg = read_reg(context, ir, rn);
    struct irRegister *rm_rotated;
    struct irRegister *lsb, *msb;
    struct irRegister *result;

    rm_rotated = mk_media_rotate(ir, rm_reg, rotation);
    lsb = ir->add_add_16(ir, ir->add_32_to_16(ir, rn_reg),
                             ir->add_8U_to_16(ir, ir->add_32_to_8(ir, rm_rotated)));
    msb = ir->add_add_16(ir, ir->add_32_to_16(ir, ir->add_shr_32(ir, rn_reg, mk_8(ir, 16))),
                             ir->add_8U_to_16(ir, ir->add_32_to_8(ir,
                                                                  ir->add_shr_32(ir, rm_rotated, mk_8(ir, 16)))));
    result = ir->add_or_32(ir,
                           ir->add_16U_to_32(ir, lsb),
                           ir->add_shl_32(ir, ir->add_16U_to_32(ir, msb), mk_8(ir, 16)));
    write_reg(context, ir, rd, result);

    return 0;
}

static int dis_uxtb(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    int rm = INSN(3, 0);
    int rotation = INSN(11, 10) << 3;
    struct irRegister *rm_reg = read_reg(context, ir, rm);
    struct irRegister *rm_rotated;

    /* rotate */
    rm_rotated = mk_media_rotate(ir, rm_reg, rotation);
    /* mask and assign */
    write_reg(context, ir, rd, ir->add_and_32(ir, rm_rotated, mk_32(ir, 0xff)));

    return 0;
}

static int dis_uxtab(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    int rn = INSN(19, 16);
    int rm = INSN(3, 0);
    int rotation = INSN(11, 10) << 3;
    struct irRegister *rm_reg = read_reg(context, ir, rm);
    struct irRegister *rm_rotated;
    struct irRegister *result;

    rm_rotated = mk_media_rotate(ir, rm_reg, rotation);
    result = ir->add_add_32(ir, read_reg(context, ir, rn),
                                ir->add_8U_to_32(ir, ir->add_32_to_8(ir, rm_rotated)));
    write_reg(context, ir, rd, result);

    return 0;
}

static int dis_rbit(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15,12);
    int rm = INSN(3,0);
    struct irRegister *w[6];
    uint32_t c[] = {0x55555555, 0x33333333,
                    0x0F0F0F0F, 0x00FF00FF,
                    0x0000FFFF};
    int i;

    w[0] = read_reg(context, ir, rm);
    for(i = 0; i < 5; i++) {
        w[i+1] = ir->add_or_32(ir,
                               ir->add_shl_32(ir,
                                              ir->add_and_32(ir,w[i], mk_32(ir, c[i])),
                                              mk_8(ir, 1 << i)),
                               ir->add_shr_32(ir,
                                              ir->add_and_32(ir,w[i], mk_32(ir, ~c[i])),
                                              mk_8(ir, 1 << i)));
    }
    write_reg(context, ir, rd, w[5]);

    return 0;
}

static int dis_uxth(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    int rm = INSN(3, 0);
    int rotation = INSN(11, 10) << 3;
    struct irRegister *rm_reg = read_reg(context, ir, rm);
    struct irRegister *rm_rotated;

    /* rotate */
    rm_rotated = mk_media_rotate(ir, rm_reg, rotation);
    /* mask and assign */
    write_reg(context, ir, rd, ir->add_and_32(ir, rm_rotated, mk_32(ir, 0xffff)));

    return 0;
}

static int dis_uxtah(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    int rn = INSN(19, 16);
    int rm = INSN(3, 0);
    int rotation = INSN(11, 10) << 3;
    struct irRegister *rm_reg = read_reg(context, ir, rm);
    struct irRegister *rm_rotated;
    struct irRegister *result;

    rm_rotated = mk_media_rotate(ir, rm_reg, rotation);
    result = ir->add_add_32(ir, read_reg(context, ir, rn),
                                ir->add_16U_to_32(ir, ir->add_32_to_16(ir, rm_rotated)));
    write_reg(context, ir, rd, result);

    return 0;
}

static int dis_blx(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN(3, 0);
    struct irRegister *dst = read_reg(context, ir, rm);

    write_reg(context, ir, 14, mk_32(ir, context->pc + 4));
    write_reg(context, ir, 15, dst);
    ir->add_exit(ir, ir->add_32U_to_64(ir, dst));

    return 1;
}

static int dis_blx_imm(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int32_t imm32 = INSN(23, 0) << 2 | INSN(24, 24) << 1;
    uint32_t dst = context->pc + 8;

    /* sign extend */
    imm32 = (imm32 << 6) >> 6;
    dst += imm32;
    dst |= 1;

    write_reg(context, ir, 14, mk_32(ir, context->pc + 4));
    write_reg(context, ir, 15, mk_32(ir, dst));

    ir->add_exit(ir, ir->add_32U_to_64(ir, mk_32(ir, dst)));

    return 1;
}

static int dis_mla(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int a = INSN(21, 21);
    int s = INSN(20, 20);
    int rd = INSN(19, 16);
    int rn = INSN(15, 12);
    int rs = INSN(11, 8);
    int rm = INSN(3, 0);
    struct irRegister *mul = mk_mul_u_lsb(context, ir, read_reg(context, ir, rm), read_reg(context, ir, rs));
    struct irRegister *res;

    if (a)
        res = ir->add_add_32(ir, mul, read_reg(context, ir, rn));
    else
        res = mul;
    if (s) {
        write_cpsr(context, ir, mk_mul_flag_update(context, ir, res, read_cpsr(context, ir)));
    }

    write_reg(context, ir, rd, res);

    return 0;
}

static int dis_umaal(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rdhi = INSN(19, 16);
    int rdlo = INSN(15,12);
    int rn = INSN(3, 0);
    int rm = INSN(11, 8);
    struct irRegister *params[4];

    params[0] = mk_32(ir, rdhi);
    params[1] = mk_32(ir, rdlo);
    params[2] = mk_32(ir, rn);
    params[3] = mk_32(ir, rm);

    ir->add_call_void(ir, "arm_hlp_umaal",
                           mk_64(ir, (uint64_t) arm_hlp_umaal),
                           params);

    return 0;
}

static int dis_mls(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(19, 16);
    int ra = INSN(15, 12);
    int rm = INSN(11, 8);
    int rn = INSN(3, 0);
    struct irRegister *mul = mk_mul_u_lsb(context, ir, read_reg(context, ir, rn), read_reg(context, ir, rm));

    write_reg(context, ir, rd, ir->add_sub_32(ir, read_reg(context, ir, ra), mul));

    return 0;
}

static int dis_mlla(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int sign = INSN(22, 22);
    int a = INSN(21, 21);
    int s = INSN(20, 20);
    int rdhi = INSN(19, 16);
    int rdlo = INSN(15, 12);
    int rs = INSN(11, 8);
    int rm = INSN(3, 0);
    struct irRegister *rm_reg = read_reg(context, ir, rm);
    struct irRegister *rs_reg = read_reg(context, ir, rs);
    struct irRegister *mul_lsb;
    struct irRegister *mul_msb;

    //FIXME
    assert(s == 0);

    if (a) {
        struct irRegister *rdhi_reg = read_reg(context, ir, rdhi);
        struct irRegister *rdlo_reg = read_reg(context, ir, rdlo);

        if (sign) {
            mul_lsb = mk_mula_s_lsb(context, ir, rm_reg, rs_reg, rdhi_reg, rdlo_reg);
            mul_msb = mk_mula_s_msb(context, ir, rm_reg, rs_reg, rdhi_reg, rdlo_reg);
        } else {
            mul_lsb = mk_mula_u_lsb(context, ir, rm_reg, rs_reg, rdhi_reg, rdlo_reg);
            mul_msb = mk_mula_u_msb(context, ir, rm_reg, rs_reg, rdhi_reg, rdlo_reg);
        }
    } else {
        if (sign) {
            mul_lsb = mk_mul_s_lsb(context, ir, rm_reg, rs_reg);
            mul_msb = mk_mul_s_msb(context, ir, rm_reg, rs_reg);
        } else {
            mul_lsb = mk_mul_u_lsb(context, ir, rm_reg, rs_reg);
            mul_msb = mk_mul_u_msb(context, ir, rm_reg, rs_reg);
        }
    }

    write_reg(context, ir, rdhi, mul_msb);
    write_reg(context, ir, rdlo, mul_lsb);

    return 0;
}

static int dis_mrc(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int opcode_1 = INSN(23, 21);
    int crn = INSN(19, 16);
    int rd = INSN(15, 12);
    int cp_num = INSN(11, 8);
    int opcode_2 = INSN(7, 5);
    int crm = INSN(3, 0);

    /* only read of TPIDRPRW is possible */
    assert(opcode_1 == 0 && crn == 13 && cp_num == 15 && opcode_2 == 3 && crm == 0);

    write_reg(context, ir, rd,
                           ir->add_read_context_32(ir, offsetof(struct arm_registers, c13_tls2)));

    return 0;
}

static int dis_mcr(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);

    return 0;
}

static int dis_load_store_halfword_register_offset(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int p = INSN(24, 24);
    int u = INSN(23, 23);
    int w = INSN(21, 21);
    int l = INSN(20, 20);
    int rn = INSN(19, 16);
    int rd = INSN(15, 12);
    int rm = INSN(3, 0);
    struct irRegister *address;
    struct irRegister *rm_reg;
    struct irRegister *rn_reg;

    assert(rd != 15);

    rm_reg = read_reg(context, ir, rm);
    rn_reg = read_reg(context, ir, rn);
    /* compute address of access */
    if (p) {
        //either offset or pre-indexedregs
        if (u)
            address = ir->add_add_32(ir, rn_reg, rm_reg);
        else
            address = ir->add_sub_32(ir, rn_reg, rm_reg);
    } else {
        //post indexed
        address = rn_reg;
    }
    /* make load/store */
    if (l == 1) {
        write_reg(context, ir, rd, ir->add_16U_to_32(ir, ir->add_load_16(ir, mk_address(ir, address))));
    } else {
        ir->add_store_16(ir, ir->add_32_to_16(ir, read_reg(context, ir, rd)), mk_address(ir, address));
    }
    /* write-back if needed */
    if (p && w) {
        //pre-indexed
        write_reg(context, ir, rn, address);
    } else if (!p && !w) {
        //post-indexed
        struct irRegister *newVal;

        if (u)
            newVal = ir->add_add_32(ir, rn_reg, rm_reg);
        else
            newVal = ir->add_sub_32(ir, rn_reg, rm_reg);
        write_reg(context, ir, rn, newVal);
    }

    return 0;
}

static int dis_load_store_halfword_immediate_offset(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int p = INSN(24, 24);
    int u = INSN(23, 23);
    int w = INSN(21, 21);
    int l = INSN(20, 20);
    int rn = INSN(19, 16);
    int rd = INSN(15, 12);
    int offset = (INSN(11, 8) << 4) + INSN(3, 0);
    struct irRegister *address;

    assert(rd != 15);

    /* compute address of access */
    if (p) {
        //either offset or pre-indexedregs
        address = address_register_offset(context, ir, rn, (u==1?offset:-offset));
    } else {
        //post indexed
        address = address_register_offset(context, ir, rn, 0);
    }
    /* make load/store */
    if (l == 1) {
        write_reg(context, ir, rd, ir->add_16U_to_32(ir, ir->add_load_16(ir, mk_address(ir, address))));
    } else {
        ir->add_store_16(ir, ir->add_32_to_16(ir, read_reg(context, ir, rd)), mk_address(ir, address));
    }
    /* write-back if needed */
    if (p && w) {
        //pre-indexed
        write_reg(context, ir, rn, address);
    } else if (!p && !w) {
        //post-indexed
        struct irRegister *newVal;

        newVal = address_register_offset(context, ir, rn, (u==1?offset:-offset));
        write_reg(context, ir, rn, newVal);
    }

    return 0;
}

static int dis_ldrh_literal(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);

    return 0;
}

static int dis_ldrex(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rn = INSN(19, 16);
    int rt = INSN(15, 12);
    struct irRegister *params[4];
    struct irRegister *result;

    assert(rn != 15);
    assert(rt != 15);

    params[0] = read_reg(context, ir, rn);
    params[1] = mk_32(ir, 4);
    params[2] = NULL;
    params[3] = NULL;

    result = ir->add_call_32(ir, "arm_hlp_ldrex",
                             mk_64(ir, (uint64_t) arm_hlp_ldrex),
                             params);

    write_reg(context, ir, rt, result);

    return 0;
}

static int dis_strex(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rn = INSN(19, 16);
    int rd = INSN(15, 12);
    int rt = INSN(3, 0);
    struct irRegister *params[4];
    struct irRegister *result;

    assert(rn != 15);
    assert(rd != 15);
    assert(rt != 15);

    params[0] = read_reg(context, ir, rn);
    params[1] = mk_32(ir, 4);
    params[2] = read_reg(context, ir, rt);
    params[3] = NULL;

    result = ir->add_call_32(ir, "arm_hlp_strex",
                             mk_64(ir, (uint64_t) arm_hlp_strex),
                             params);

    write_reg(context, ir, rd, result);

    return 0;
}

static int dis_isb(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = NULL;
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "arm_hlp_memory_barrier",
                           mk_64(ir, (uint64_t) arm_hlp_memory_barrier),
                           params);

    return 0;
}

static int dis_dsb(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = NULL;
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "arm_hlp_memory_barrier",
                           mk_64(ir, (uint64_t) arm_hlp_memory_barrier),
                           params);

    return 0;
}

static int dis_dmb(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = NULL;
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "arm_hlp_memory_barrier",
                           mk_64(ir, (uint64_t) arm_hlp_memory_barrier),
                           params);

    return 0;
}

static int dis_sxtb16(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    int rm = INSN(3, 0);
    int rotation = INSN(11, 10) << 3;
    struct irRegister *rm_reg = read_reg(context, ir, rm);
    struct irRegister *rm_rotated;
    struct irRegister *lsb, *msb;
    struct irRegister *result;

    rm_rotated = mk_media_rotate(ir, rm_reg, rotation);
    lsb = ir->add_8S_to_16(ir, ir->add_32_to_8(ir, rm_rotated));
    msb = ir->add_8S_to_16(ir, ir->add_32_to_8(ir, ir->add_shr_32(ir, rm_rotated, mk_8(ir, 16))));
    result = ir->add_or_32(ir,
                           ir->add_16U_to_32(ir, lsb),
                           ir->add_shl_32(ir, ir->add_16U_to_32(ir, msb), mk_8(ir, 16)));
    write_reg(context, ir, rd, result);

    return 0;
}

static int dis_sxtab16(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    int rn = INSN(19, 16);
    int rm = INSN(3, 0);
    int rotation = INSN(11, 10) << 3;
    struct irRegister *rm_reg = read_reg(context, ir, rm);
    struct irRegister *rn_reg = read_reg(context, ir, rn);
    struct irRegister *rm_rotated;
    struct irRegister *lsb, *msb;
    struct irRegister *result;

    rm_rotated = mk_media_rotate(ir, rm_reg, rotation);
    lsb = ir->add_add_16(ir, ir->add_32_to_16(ir, rn_reg),
                             ir->add_8S_to_16(ir, ir->add_32_to_8(ir, rm_rotated)));
    msb = ir->add_add_16(ir, ir->add_32_to_16(ir, ir->add_shr_32(ir, rn_reg, mk_8(ir, 16))),
                             ir->add_8S_to_16(ir, ir->add_32_to_8(ir,
                                                                  ir->add_shr_32(ir, rm_rotated, mk_8(ir, 16)))));
    result = ir->add_or_32(ir,
                           ir->add_16U_to_32(ir, lsb),
                           ir->add_shl_32(ir, ir->add_16U_to_32(ir, msb), mk_8(ir, 16)));
    write_reg(context, ir, rd, result);

    return 0;
}

static int dis_sxtb(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    int rm = INSN(3, 0);
    int rotation = INSN(11, 10) << 3;
    struct irRegister *rm_reg = read_reg(context, ir, rm);
    struct irRegister *rm_rotated;
    struct irRegister *result;

    rm_rotated = mk_media_rotate(ir, rm_reg, rotation);
    result = ir->add_8S_to_32(ir, ir->add_32_to_8(ir, rm_rotated));
    write_reg(context, ir, rd, result);

    return 0;
}

static int dis_sxtab(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    int rn = INSN(19, 16);
    int rm = INSN(3, 0);
    int rotation = INSN(11, 10) << 3;
    struct irRegister *rm_reg = read_reg(context, ir, rm);
    struct irRegister *rm_rotated;
    struct irRegister *result;

    rm_rotated = mk_media_rotate(ir, rm_reg, rotation);
    result = ir->add_add_32(ir, read_reg(context, ir, rn),
                                ir->add_8S_to_32(ir, ir->add_32_to_8(ir, rm_rotated)));
    write_reg(context, ir, rd, result);

    return 0;
}

static int dis_sxth(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    int rm = INSN(3, 0);
    int rotation = INSN(11, 10) << 3;
    struct irRegister *rm_reg = read_reg(context, ir, rm);
    struct irRegister *rm_rotated;
    /* rotate */
    rm_rotated = mk_media_rotate(ir, rm_reg, rotation);
    /* sign extend and assign */
    write_reg(context, ir, rd, ir->add_16S_to_32(ir, ir->add_32_to_16(ir, rm_rotated)));

    return 0;
}

static int dis_sxtah(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    int rn = INSN(19, 16);
    int rm = INSN(3, 0);
    int rotation = INSN(11, 10) << 3;
    struct irRegister *rm_reg = read_reg(context, ir, rm);
    struct irRegister *rm_rotated;
    struct irRegister *result;

    rm_rotated = mk_media_rotate(ir, rm_reg, rotation);
    result = ir->add_add_32(ir, read_reg(context, ir, rn),
                                ir->add_16S_to_32(ir, ir->add_32_to_16(ir, rm_rotated)));
    write_reg(context, ir, rd, result);

    return 0;
}

static int dis_load_store_double_immediate_offset(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int p = INSN(24, 24);
    int u = INSN(23, 23);
    int w = INSN(21, 21);
    int l = 1 - INSN(5, 5);
    int rn = INSN(19, 16);
    int rd = INSN(15, 12);
    int offset = (INSN(11, 8) << 4) + INSN(3, 0);
    struct irRegister *address;

    assert(rd + 1 != 15);
    assert(rd % 2 == 0);

    /* compute address of access */
    if (p) {
        //either offset or pre-indexedregs
        address = address_register_offset(context, ir, rn, (u==1?offset:-offset));
    } else {
        //post indexed
        address = address_register_offset(context, ir, rn, 0);
    }
    /* make load/store */
    if (l == 1) {
        write_reg(context, ir, rd, ir->add_load_32(ir, mk_address(ir, address)));
        write_reg(context, ir, rd + 1, ir->add_load_32(ir, mk_address(ir, ir->add_add_32(ir, address, mk_32(ir, 4)))));
    } else {
        ir->add_store_32(ir, read_reg(context, ir, rd), mk_address(ir, address));
        ir->add_store_32(ir, read_reg(context, ir, rd + 1), mk_address(ir, ir->add_add_32(ir, address, mk_32(ir, 4))));
    }
    /* write-back if needed */
    if (p && w) {
        //pre-indexed
        write_reg(context, ir, rn, address);
    } else if (!p && !w) {
        //post-indexed
        struct irRegister *newVal;

        newVal = address_register_offset(context, ir, rn, (u==1?offset:-offset));
        write_reg(context, ir, rn, newVal);
    }

    return 0;
}

static int dis_load_store_double_register_offset(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int p = INSN(24, 24);
    int u = INSN(23, 23);
    int w = INSN(21, 21);
    int l = 1 - INSN(5, 5);
    int rn = INSN(19, 16);
    int rd = INSN(15, 12);
    int rm = INSN(3, 0);
    struct irRegister *address;
    struct irRegister *rm_reg;
    struct irRegister *rn_reg;

    assert(rd + 1 != 15);
    assert(rd % 2 == 0);

    rm_reg = read_reg(context, ir, rm);
    rn_reg = read_reg(context, ir, rn);
    /* compute address of access */
    if (p) {
        //either offset or pre-indexedregs
        if (u)
            address = ir->add_add_32(ir, rn_reg, rm_reg);
        else
            address = ir->add_sub_32(ir, rn_reg, rm_reg);
    } else {
        //post indexed
        address = rn_reg;
    }
    /* make load/store */
    if (l == 1) {
        write_reg(context, ir, rd, ir->add_load_32(ir, mk_address(ir, address)));
        write_reg(context, ir, rd + 1, ir->add_load_32(ir, mk_address(ir, ir->add_add_32(ir, address, mk_32(ir, 4)))));
    } else {
        ir->add_store_32(ir, read_reg(context, ir, rd), mk_address(ir, address));
        ir->add_store_32(ir, read_reg(context, ir, rd + 1), mk_address(ir, ir->add_add_32(ir, address, mk_32(ir, 4))));
    }
    /* write-back if needed */
    if (p && w) {
        //pre-indexed
        write_reg(context, ir, rn, address);
    } else if (!p && !w) {
        //post-indexed
        struct irRegister *newVal;

        if (u)
            newVal = ir->add_add_32(ir, rn_reg, rm_reg);
        else
            newVal = ir->add_sub_32(ir, rn_reg, rm_reg);
        write_reg(context, ir, rn, newVal);
    }

    return 0;
}

static int dis_load_signed_halfword_byte_immediate_offset(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int p = INSN(24, 24);
    int u = INSN(23, 23);
    int w = INSN(21, 21);
    int rn = INSN(19, 16);
    int rd = INSN(15, 12);
    int offset = (INSN(11, 8) << 4) + INSN(3, 0);
    int isHalfWord = INSN(5, 5);
    struct irRegister *address;

    assert(rd != 15);

    /* compute address of access */
    if (p) {
        //either offset or pre-indexedregs
        address = address_register_offset(context, ir, rn, (u==1?offset:-offset));
    } else {
        //post indexed
        address = address_register_offset(context, ir, rn, 0);
    }
    /* make load */
    if (isHalfWord)
        write_reg(context, ir, rd, ir->add_16S_to_32(ir, ir->add_load_16(ir, mk_address(ir, address))));
    else
        write_reg(context, ir, rd, ir->add_8S_to_32(ir, ir->add_load_8(ir, mk_address(ir, address))));
    /* write-back if needed */
    if (p && w) {
        //pre-indexed
        write_reg(context, ir, rn, address);
    } else if (!p && !w) {
        //post-indexed
        struct irRegister *newVal;

        newVal = address_register_offset(context, ir, rn, (u==1?offset:-offset));
        write_reg(context, ir, rn, newVal);
    }

    return 0;
}

static int dis_load_signed_halfword_byte_register_offset(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int p = INSN(24, 24);
    int u = INSN(23, 23);
    int w = INSN(21, 21);
    int rn = INSN(19, 16);
    int rd = INSN(15, 12);
    int rm = INSN(3, 0);
    int isHalfWord = INSN(5, 5);
    struct irRegister *address;
    struct irRegister *rm_reg;
    struct irRegister *rn_reg;

    assert(rd != 15);

    rm_reg = read_reg(context, ir, rm);
    rn_reg = read_reg(context, ir, rn);
    /* compute address of access */
    if (p) {
        //either offset or pre-indexedregs
        if (u)
            address = ir->add_add_32(ir, rn_reg, rm_reg);
        else
            address = ir->add_sub_32(ir, rn_reg, rm_reg);
    } else {
        //post indexed
        address = rn_reg;
    }
    /* make load */
    if (isHalfWord)
        write_reg(context, ir, rd, ir->add_16S_to_32(ir, ir->add_load_16(ir, mk_address(ir, address))));
    else
        write_reg(context, ir, rd, ir->add_8S_to_32(ir, ir->add_load_8(ir, mk_address(ir, address))));
    /* write-back if needed */
    if (p && w) {
        //pre-indexed
        write_reg(context, ir, rn, address);
    } else if (!p && !w) {
        //post-indexed
        struct irRegister *newVal;

        if (u)
            newVal = ir->add_add_32(ir, rn_reg, rm_reg);
        else
            newVal = ir->add_sub_32(ir, rn_reg, rm_reg);
        write_reg(context, ir, rn, newVal);
    }

    return 0;
}

static int dis_ldrd_literal(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rt = INSN(15, 12);
    int is_add = INSN(23, 23);
    uint32_t imm32 = (INSN(11, 8) << 4) + INSN(3, 0);
    int rt2 = rt + 1;
    struct irRegister *address;
    struct irRegister *address2;

    assert(rt % 2 == 0);
    assert(rt2 != 15);

    address = mk_address(ir, mk_32(ir, context->pc + 8 + (is_add?imm32:-imm32)));
    address2 = mk_address(ir, mk_32(ir, context->pc + 8 + 4 + (is_add?imm32:-imm32)));
    write_reg(context, ir, rt, ir->add_load_32(ir, address));
    write_reg(context, ir, rt2, ir->add_load_32(ir, address2));

    return 0;
}

static int dis_ubfx(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int widthm1 = INSN(20, 16);
    int rd = INSN(15, 12);
    int lsb = INSN(11, 7);
    int rn = INSN(3, 0);
    uint32_t mask = (1 << (widthm1 + 1)) - 1;

    write_reg(context, ir, rd, ir->add_and_32(ir,
                                              ir->add_shr_32(ir, read_reg(context, ir, rn), mk_8(ir, lsb)),
                                              mk_32(ir, mask)));

    return 0;
}

static int dis_sbfx(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int widthm1 = INSN(20, 16);
    int rd = INSN(15, 12);
    int lsb = INSN(11, 7);
    int rn = INSN(3, 0);

    write_reg(context, ir, rd, ir->add_asr_32(ir,
                                              ir->add_shl_32(ir, read_reg(context, ir, rn), mk_8(ir, 31-lsb-widthm1)),
                                              mk_8(ir, 31 - widthm1)));

    return 0;
}

static int dis_bfc(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    int msb = INSN(20, 16);
    int lsb = INSN(11, 7);
    int width = msb - lsb + 1;
    int mask = ~(((1 << width) - 1) << lsb);

    assert(rd != 15);

    write_reg(context, ir, rd, ir->add_and_32(ir,
                                              read_reg(context, ir, rd),
                                              mk_32(ir, mask)));

    return 0;
}

static int dis_bfi(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    int rn = INSN(3, 0);
    int msb = INSN(20, 16);
    int lsb = INSN(11, 7);
    int width = msb - lsb + 1;
    uint32_t mask_rn = (((1 << width) - 1) << lsb);
    uint32_t mask_rd = ~mask_rn;
    struct irRegister *op1;
    struct irRegister *op2;

    op1 = ir->add_and_32(ir,
                         ir->add_shl_32(ir, read_reg(context, ir, rn), mk_8(ir, lsb)),
                         mk_32(ir, mask_rn));
    op2 = ir->add_and_32(ir,
                         read_reg(context, ir, rd),
                         mk_32(ir, mask_rd));
    write_reg(context, ir, rd, ir->add_or_32(ir, op1, op2));

    return 0;
}

static int dis_rev(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    int rn = INSN(3, 0);
    struct irRegister *rn_reg = read_reg(context, ir, rn);
    struct irRegister *tmp;
    struct irRegister *result;

    tmp = ir->add_or_32(ir,
                        ir->add_shl_32(ir,
                                       ir->add_and_32(ir, rn_reg, mk_32(ir, 0x0000ffff)),
                                       mk_8(ir, 16)),
                        ir->add_shr_32(ir,
                                       ir->add_and_32(ir, rn_reg, mk_32(ir, 0xffff0000)),
                                       mk_8(ir, 16)));
    result = ir->add_or_32(ir,
                        ir->add_shl_32(ir,
                                       ir->add_and_32(ir, tmp, mk_32(ir, 0x00ff00ff)),
                                       mk_8(ir, 8)),
                        ir->add_shr_32(ir,
                                       ir->add_and_32(ir, tmp, mk_32(ir, 0xff00ff00)),
                                       mk_8(ir, 8)));
    write_reg(context, ir, rd, result);

    return 0;
}

static int dis_rev16(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    int rn = INSN(3, 0);
    struct irRegister *rn_reg = read_reg(context, ir, rn);
    struct irRegister *result;

    result = ir->add_or_32(ir,
                           ir->add_shl_32(ir,
                                          ir->add_and_32(ir, rn_reg, mk_32(ir, 0x00ff00ff)),
                                          mk_8(ir, 8)),
                           ir->add_shr_32(ir,
                                          ir->add_and_32(ir, rn_reg, mk_32(ir, 0xff00ff00)),
                                          mk_8(ir, 8)));
    write_reg(context, ir, rd, result);

    return 0;
}

static int dis_revsh(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);
    int rn = INSN(3, 0);
    struct irRegister *rn_reg = read_reg(context, ir, rn);
    struct irRegister *tmp;
    struct irRegister *result;

    tmp = ir->add_or_32(ir,
                        ir->add_shl_32(ir,
                                       ir->add_and_32(ir, rn_reg, mk_32(ir, 0x000000ff)),
                                       mk_8(ir, 8)),
                        ir->add_shr_32(ir,
                                       ir->add_and_32(ir, rn_reg, mk_32(ir, 0x0000ff00)),
                                       mk_8(ir, 8)));
    result = ir->add_16S_to_32(ir, ir->add_32_to_16(ir, tmp));

    write_reg(context, ir, rd, result);

    return 0;
}

static int dis_mrs(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(15, 12);

    write_reg(context, ir, rd, read_cpsr(context, ir));

    return 0;
}

static int dis_msr_register(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rn = INSN(3, 0);
    int write_nzcvq = INSN(19, 19);
    int write_g = INSN(18, 18);
    uint32_t mask = 0;
    struct irRegister *cpsr_mask;
    struct irRegister *rn_mask;

    assert(rn != 15);
    mask += write_nzcvq?0xf8000000:0;
    mask += write_g?0x000f0000:0;

    cpsr_mask = ir->add_and_32(ir,
                               read_cpsr(context, ir),
                               mk_32(ir, ~mask));
    rn_mask = ir->add_and_32(ir,
                             read_reg(context, ir, rn),
                             mk_32(ir, mask));

    write_cpsr(context, ir, ir->add_or_32(ir, cpsr_mask, rn_mask));

    return 0;
}

static int dis_gdb_breakpoint(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    //set pc to correct location before sending sigill signal
    write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc));
    //will send a SIGILL signal
    mk_gdb_breakpoint_instruction(ir);

    //clean caches since code has been modify to remove/insert breakpoints
    cleanCaches(0,~0);
    //reexecute same instructions since opcode has been updated by gdb
    ir->add_exit(ir, ir->add_mov_const_64(ir, context->pc));

    return 1;
}

static int dis_pkh(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rn = INSN(19, 16);
    int rd = INSN(15, 12);
    int rm = INSN(3, 0);
    int imm5 = INSN(11, 7);
    int tb = INSN(6, 6);
    struct irRegister *operand2;
    struct irRegister *res;

    if (tb)
        operand2 = ir->add_asr_32(ir, read_reg(context, ir, rm), mk_8(ir, imm5?imm5:32));
    else
        operand2 = ir->add_shl_32(ir, read_reg(context, ir, rm), mk_8(ir, imm5));
    if (tb)
        res = ir->add_or_32(ir,
                            ir->add_and_32(ir, read_reg(context, ir, rn), mk_32(ir, 0xffff0000)),
                            ir->add_and_32(ir, operand2, mk_32(ir, 0x0000ffff)));
    else
        res = ir->add_or_32(ir,
                            ir->add_and_32(ir, read_reg(context, ir, rn), mk_32(ir, 0x0000ffff)),
                            ir->add_and_32(ir, operand2, mk_32(ir, 0xffff0000)));

    write_reg(context, ir, rd, res);

    return 0;
}

static int dis_sel(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rn = INSN(19, 16);
    int rd = INSN(15, 12);
    int rm = INSN(3, 0);
    struct irRegister *params[4];
    struct irRegister *result;

    assert(rn != 15);
    assert(rm != 15);
    assert(rd != 15);

    params[0] = read_cpsr(context, ir);
    params[1] = read_reg(context, ir, rn);
    params[2] = read_reg(context, ir, rm);
    params[3] = NULL;

    result = ir->add_call_32(ir, "arm_hlp_sel",
                             mk_64(ir, (uint64_t) arm_hlp_sel),
                             params);

    write_reg(context, ir, rd, result);

    return 0;
}

static int dis_msr_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int mask = INSN(19, 18);
    int imm12 = INSN(11, 0);
    uint32_t imm32 = armExpandImm(imm12);
    uint32_t mask32 = 0;

    if (mask & 1)
        mask32 |= 0x000f0000;
    if (mask & 2)
        mask32 |= 0xf8000000;

    write_cpsr(context, ir, ir->add_or_32(ir,
                                          ir->add_and_32(ir, read_cpsr(context, ir), mk_32(ir, ~mask32)),
                                          mk_32(ir, imm32 & mask32)));

    return 0;
}

 /* pure disassembler */
static int dis_msr_imm_and_hints(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op = INSN(22, 22);
    int op1 = INSN(19, 16);
    //int op2 = INSN(7, 0);


    assert(op == 0);

    switch(op1) {
        case 4: case 8: case 12:
            isExit = dis_msr_immediate(context, insn, ir);
            break;
        default:
            fatal("op1 = %d(0x%x)\n", op1, op1);
    }

    return isExit;
}

static int dis_sync_primitive(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int op = INSN(23, 20);
    int isExit = 0;

    switch(op) {
        case 8:
            isExit = dis_strex(context, insn, ir);
            break;
        case 9:
            isExit = dis_ldrex(context, insn, ir);
            break;
        default:
            fatal("op = %d(0x%x)\n", op, op);
    }

    return isExit;
}

static int dis_mult_and_mult_acc_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int op = INSN(23, 20);
    int isExit = 0;

    switch(op) {
        case 0 ... 3:
            isExit = dis_mla(context, insn, ir);
            break;
        case 4:
            isExit = dis_umaal(context, insn, ir);
            break;
        case 6:
            isExit = dis_mls(context, insn, ir);
            break;
        case 8 ... 15:
            // sign/unsigned multiply accumulate or not long
            isExit = dis_mlla(context, insn, ir);
            break;
        default:
            fatal("op = %d(0x%x)\n", op, op);
    }

    return isExit;
}

static int dis_extra_load_store_unprivilege_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);

    return 0;
}

static int dis_extra_load_store_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int op2 = INSN(6, 5);
    int op1 = INSN(24, 20);
    int rn = INSN(19, 16);
    int isExit = 0;

    switch(op2) {
        case 1:
            switch(op1 & 0x5) {
                case 0:
                case 1:
                    isExit = dis_load_store_halfword_register_offset(context, insn, ir);
                    break;
                case 4:
                    isExit = dis_load_store_halfword_immediate_offset(context, insn, ir);
                    break;
                case 5:
                    if (rn == 15)
                        isExit = dis_ldrh_literal(context, insn, ir);
                    else
                        isExit = dis_load_store_halfword_immediate_offset(context, insn, ir);
                    break;
                default:
                    fatal("op1 = %d(0x%x)\n", op1 & 0x5, op1 & 0x5);
            }
            break;
        case 2:
            switch(op1 & 0x5) {
                case 0:
                    isExit = dis_load_store_double_register_offset(context, insn, ir);
                    break;
                 case 1:
                    isExit = dis_load_signed_halfword_byte_register_offset(context, insn, ir);
                    break;
                case 4:
                    if (rn == 15)
                        isExit = dis_ldrd_literal(context, insn, ir);
                    else
                        isExit = dis_load_store_double_immediate_offset(context, insn, ir);
                    break;
                case 5:
                    isExit = dis_load_signed_halfword_byte_immediate_offset(context, insn, ir);
                    break;
                default:
                    fatal("insn = 0x%08x / op1 = %d(0x%x)\n", insn, op1 & 0x5, op1 & 0x5);
            }
            break;
        case 3:
            switch(op1 & 0x5) {
                case 0:
                    isExit = dis_load_store_double_register_offset(context, insn, ir);
                    break;
                case 1:
                    isExit = dis_load_signed_halfword_byte_register_offset(context, insn, ir);
                    break;
                case 4:
                    isExit = dis_load_store_double_immediate_offset(context, insn, ir);
                    break;
                case 5:
                    isExit = dis_load_signed_halfword_byte_immediate_offset(context, insn, ir);
                    break;
                default:
                    fatal("op1 = %d(0x%x)\n", op1 & 0x5, op1 & 0x5);
            }
            break;
        default:
            fatal("op2 = %d(0x%x)\n", op2, op2);
    }
    return isExit;
}

static int dis_halfword_mult_and_mult_acc_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "arm_hlp_halfword_mult_and_mult_acc",
                           ir->add_mov_const_64(ir, (uint64_t) arm_hlp_halfword_mult_and_mult_acc),
                           params);

    return 0;
}

static int dis_saturating_add_A_sub(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "arm_hlp_dirty_saturating",
                           ir->add_mov_const_64(ir, (uint64_t) arm_hlp_dirty_saturating),
                           params);

    return 0;
}

static int dis_misc_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    uint32_t op = INSN(22,21);
    uint32_t op1 = INSN(19,16);
    uint32_t op2 = INSN(6,4);
    int isExit = 0;

    switch(op2) {
        case 0:
            if ((op & 1) == 0) {
                isExit = dis_mrs(context, insn, ir);
            } else if(op == 1 && (op1 & 3) == 0) {
                //msr
                isExit = dis_msr_register(context, insn, ir);
            } else {
                assert(0 && "msr system in user code !!!!");
            }
            break;
        case 1:
            if (op == 1)
                isExit = dis_bx(context, insn, ir);
            else if (op == 3)
                isExit = dis_clz(context, insn, ir);
            else
                assert(0);
            break;
        case 3:
            isExit = dis_blx(context, insn, ir);
            break;
        case 5:
            isExit = dis_saturating_add_A_sub(context, insn, ir);
            break;
        default:
            assert(0);
    }

    return isExit;
}

static int dis_saturation(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "arm_hlp_saturation",
                           ir->add_mov_const_64(ir, (uint64_t) arm_hlp_saturation),
                           params);

    return 0;
}

static int dis_packing_A_unpacking_A_saturation_A_reversal(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int op1 = INSN(22, 20);
    int op2 = INSN(7, 5);
    int a = INSN(19, 16);
    int isExit = 0;

    switch(op1) {
        case 0:
            if ((op2 & 1) == 0) {
                isExit = dis_pkh(context, insn, ir);
            } else if (op2 == 5) {
                isExit = dis_sel(context, insn, ir);
            } else if (op2 == 3) {
                if (a == 15)
                    isExit = dis_sxtb16(context, insn, ir);
                else
                    isExit = dis_sxtab16(context, insn, ir);
            } else
                fatal("op2 = %d(0x%x)\n", op2, op2);
            break;
        case 2:
            if ((op2 & 1) == 0 || op2 == 1) {
                isExit = dis_saturation(context, insn, ir);
            } else if (op2 == 3) {
                if (a == 15)
                    isExit = dis_sxtb(context, insn, ir);
                else
                    isExit = dis_sxtab(context, insn, ir);
            } else
                fatal("Invalid opcode op2 = %d\n", op2);
            break;
        case 3:
            if ((op2 & 1) == 0) {
                isExit = dis_saturation(context, insn, ir);
            } else if (op2 == 1) {
                isExit = dis_rev(context, insn, ir);
            } else if (op2 == 3) {
                if (a == 15)
                    isExit = dis_sxth(context, insn, ir);
                else
                    isExit = dis_sxtah(context, insn, ir);
            } else if (op2 == 5) {
                isExit = dis_rev16(context, insn, ir);
            }
            break;
        case 4:
            if (a == 15)
                isExit = dis_uxtb16(context, insn, ir);
            else
                isExit = dis_uxtab16(context, insn, ir);
            break;
        case 6:
            if ((op2 & 1) == 0 || op2 == 1) {
                isExit = dis_saturation(context, insn, ir);
            } else if (op2 == 3) {
                if (a == 15)
                    isExit = dis_uxtb(context, insn, ir);
                else
                    isExit = dis_uxtab(context, insn, ir);
            } else
                fatal("op2 = %d(0x%x)\n", op2, op2);
            break;
        case 7:
            if ((op2 & 1) == 0) {
                isExit = dis_saturation(context, insn, ir);
            } else if (op2 == 1) {
                isExit = dis_rbit(context, insn, ir);
            } else if (op2 == 3) {
                if (a == 15)
                    isExit = dis_uxth(context, insn, ir);
                else
                    isExit = dis_uxtah(context, insn, ir);
            } else if (op2 == 5) {
                isExit = dis_revsh(context, insn, ir);
            } else {
                assert(0);
            }
            break;
        default:
            fatal("op1 = %d(0x%x)\n", op1, op1);
    }

    return isExit;
}

static int dis_signed_multiplies(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "arm_hlp_signed_multiplies",
                           ir->add_mov_const_64(ir, (uint64_t) arm_hlp_signed_multiplies),
                           params);

    return 0;
}

static int dis_sum_absolute_difference(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "arm_hlp_sum_absolute_difference",
                           ir->add_mov_const_64(ir, (uint64_t) arm_hlp_sum_absolute_difference),
                           params);

    return 0;
}

static int dis_signed_parallel(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "arm_hlp_signed_parallel",
                           ir->add_mov_const_64(ir, (uint64_t) arm_hlp_signed_parallel),
                           params);

    return 0;
}

static int dis_unsigned_parallel(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "arm_hlp_unsigned_parallel",
                           ir->add_mov_const_64(ir, (uint64_t) arm_hlp_unsigned_parallel),
                           params);

    return 0;
}

static int dis_data_processing_register_shifted_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int s = INSN(20, 20);
    int rs = INSN(11, 8);
    int shift_mode = INSN(6, 5);
    int rm = INSN(3, 0);
    struct irRegister *op;
    struct irRegister *rm_reg;
    struct irRegister *rs_reg;

    //FIXME :
    assert(rm != 15);
    assert(rs != 15);

    rm_reg = read_reg(context, ir, rm);
    rs_reg = read_reg(context, ir, rs);

    if (s) {
        write_sco(context, ir, mk_sco(context, ir,
                                      mk_32(ir, insn),
                                      rm_reg,
                                      ir->add_32_to_8(ir, rs_reg)));
    }
    switch(shift_mode) {
        case 0:
            op = ir->add_shl_32(ir,
                                rm_reg,
                                ir->add_32_to_8(ir, rs_reg));
            break;
        case 1:
            op = ir->add_shr_32(ir,
                                rm_reg,
                                ir->add_32_to_8(ir, rs_reg));
            break;
        case 2:
            op = ir->add_asr_32(ir,
                                rm_reg,
                                ir->add_32_to_8(ir, rs_reg));
            break;
        case 3:
            //ror
            op = mk_ror_reg_32(ir, rm_reg, rs_reg);
            break;
    default:
            assert(0);
    }

    return mk_data_processing(context, ir, insn, op);
}

static int dis_data_processing_and_misc_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    uint32_t op1 = INSN(24, 20);
    uint32_t op2 = INSN(7, 4);

    if (INSN(25,25)) {
        if (op1 == 0x10)
            isExit = dis_movw(context, insn, ir);
        else if (op1 == 0x14)
            isExit = dis_movt(context, insn, ir);
        else if ((op1 & 0x1b) == 0x12)
            isExit = dis_msr_imm_and_hints(context, insn, ir);
        else
            isExit = dis_data_processing_immediate(context, insn, ir);
    } else {
        if (op2 == 0x9) {
            if (op1 & 0x10)
                isExit = dis_sync_primitive(context, insn, ir);
            else
                isExit = dis_mult_and_mult_acc_insn(context, insn, ir);
        } else if (op2 == 0xb || op2 == 0xd || op2 == 0xf) {
            if ((op1 & 0x12) == 0x2)
                isExit = dis_extra_load_store_unprivilege_insn(context, insn, ir);
            else
                isExit = dis_extra_load_store_insn(context, insn, ir);
        } else if ((op1 & 0x19) == 0x10) {
            if (op2 & 0x8)
                isExit = dis_halfword_mult_and_mult_acc_insn(context, insn, ir);
            else
                isExit = dis_misc_insn(context, insn, ir);
        } else {
            if (op2 & 0x1)
                isExit = dis_data_processing_register_shifted_insn(context, insn, ir);
            else
                isExit = dis_data_processing_register_insn(context, insn, ir);
        }
    }

    return isExit;
}

static int dis_load_store_word_and_unsigned_byte(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    if (INSN(25, 25))
        return dis_load_store_word_and_unsigned_byte_register_offset(context, insn, ir);
    else
        return dis_load_store_word_and_unsigned_byte_imm_offset(context, insn, ir);
}

static int dis_media_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int op1 = INSN(24, 20);
    int op2 = INSN(7, 5);
    //int rd = INSN(14, 12);
    int rn = INSN(3, 0);
    int isExit = 0;


    switch(op1) {
        case 0 ... 3:
            isExit = dis_signed_parallel(context, insn, ir);
            break;
        case 4 ... 7:
            isExit = dis_unsigned_parallel(context, insn, ir);
            break;
        case 8 ... 15:
            isExit = dis_packing_A_unpacking_A_saturation_A_reversal(context, insn, ir);
            break;
        case 16 ... 23:
            isExit = dis_signed_multiplies(context, insn, ir);
            break;
        case 24:
            isExit = dis_sum_absolute_difference(context, insn, ir);
            break;
        case 26 ... 27:
            isExit = dis_sbfx(context, insn, ir);
            break;
        case 28 ... 29:
            if (rn == 15)
                isExit = dis_bfc(context, insn, ir);
            else
                isExit = dis_bfi(context, insn, ir);
            break;
        case 30 ... 31:
            if (op1 == 31 && op2 == 7)
                isExit = dis_gdb_breakpoint(context, insn, ir);
            else
                isExit = dis_ubfx(context, insn, ir);
            break;
        default:
            fatal("op1 = %d(0x%x)\n", op1, op1);
    }

    return isExit;
}

static int dis_branch_and_branch_with_link_and_block_data_transfert(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    if (INSN(25,25))
        return dis_bl(context, insn, ir);
    else
        return dis_ldm_stm(context, insn, ir);
}

static int dis_system_call_and_coprocessor_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    uint32_t opt1 = INSN(25, 20);
    uint32_t op = INSN(4,4);
    uint32_t coproc = INSN(11,8);
    int isExit = 0;

    if ((opt1 & 0x30) == 0x30) {
        isExit = dis_swi(context, insn, ir);
    } else if ((opt1 & 0x30) == 0x20) {
        if ((coproc & 0xe) == 0xa) {
            if (op)
                isExit = dis_common_transfert_arm_core_and_extension_registers_insn(context, insn, ir);
            else {
                isExit = dis_common_vfp_data_processing_insn(context, insn, ir);
            }
        } else {
            if (op) {
                if (opt1 & 1)
                    isExit = dis_mrc(context, insn, ir);
                else
                    isExit = dis_mcr(context, insn, ir);
            } else {
                //cdp, cdp2
                assert(0);
            }
        }
    } else {
        if (opt1 == 0 || opt1 == 1) {
            fatal("undefined\n");
        } else if ((opt1 & 0x3a) == 0) {
            if ((coproc & 0xe) == 0xa) {
                if (coproc == 10)
                    isExit = dis_common_vmov_two_arm_core_and_two_s_insn(context, insn, ir);
                else
                    isExit = dis_common_vmov_two_arm_core_and_d_insn(context, insn, ir);
            } else if (opt1 == 4) {
                fatal("mcrr, mcrr2\n");
            } else if (opt1 == 5) {
                fatal("mrrc, mrrc2\n");
            } else
                fatal("bug. Should not be here\n");
        } else {
            if ((coproc & 0xe) == 0xa) {
                isExit = dis_common_extension_register_load_store_insn(context, insn, ir);
            } else if ((opt1 & 0x21) == 0) {
                fatal("stc, stc2\n");
            } else if ((opt1 & 0x21) == 0) {
                fatal("ldc, ldc2\n");
            } else
                fatal("bug. Should not be here\n");
        }
    }

    return isExit;
}

static int dis_misc_A_memory_hints_A_adv_simd_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op1 = INSN(26, 20);
    int op2 = INSN(7, 4);
    //int op = INSN(16, 16);

    if (op1 == 0x57) {
        switch(op2) {
            case 1:
                assert(0 && "clrex");
                break;
            case 4:
                isExit = dis_dsb(context, insn, ir);
                break;
            case 5:
                isExit = dis_dmb(context, insn, ir);
                break;
            case 6:
                isExit = dis_isb(context, insn, ir);
                break;
            default:
                assert(0);
        }
    } else if ((op1 & 0x60) == 0x20) {
        isExit = dis_common_adv_simd_data_preocessing_insn(context, insn, ir);
    } else if ((op1 & 0x77) == 0x55) {
        //pld. nothing to do
    } else
        fatal("op1 = %d(0x%x)\n", op1, op1);

    return isExit;
}

static int dis_unconditional_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op1 = INSN(27, 20);

    if (INSN(27, 27)) {
        if ((op1 & 0xe0) == 0xa0)
            isExit = dis_blx_imm(context, insn, ir);
        else
            assert(0);
    } else {
        isExit = dis_misc_A_memory_hints_A_adv_simd_insn(context, insn, ir);
    }

    return isExit;
}

static int disassemble_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    uint32_t cond;
    int isExit = 0;

    cond = INSN(31, 28);
    /* if instruction is conditionnal then test condition */
    if (cond < 14) {
        struct irRegister *params[4];
        struct irRegister *pred;

        params[0] = mk_32(ir, cond);
        params[1] = read_cpsr(context, ir);
        params[2] = NULL;
        params[3] = NULL;

        pred = ir->add_call_32(ir, "arm_hlp_compute_flags_pred",
                               mk_64(ir, (uint64_t) arm_hlp_compute_flags_pred),
                               params);
        write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc + 4));
        ir->add_exit_cond(ir, ir->add_mov_const_64(ir, context->pc + 4), pred);
        //Following code is useless except when dumping state
#if DUMP_STATE
    write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc));
#endif
    }

    /* now really disassemble instruction */
    if (cond == 15) {
        isExit = dis_unconditional_insn(context, insn, ir);
    } else {
        uint32_t op1 = INSN(27, 25);

        switch(op1) {
            case 0: case 1:
                isExit = dis_data_processing_and_misc_insn(context, insn, ir);
                break;
            case 2:
                isExit = dis_load_store_word_and_unsigned_byte(context, insn, ir);
                break;
            case 3:
                if (INSN(4,4))
                    isExit = dis_media_insn(context, insn, ir);
                else
                    isExit = dis_load_store_word_and_unsigned_byte(context, insn, ir);
                break;
            case 4: case 5:
                isExit = dis_branch_and_branch_with_link_and_block_data_transfert(context, insn, ir);
                break;
            case 6: case 7:
                isExit = dis_system_call_and_coprocessor_insn(context, insn, ir);
                break;
        }
    }

    return isExit;
}

static int vdso_cmpxchg(struct arm_target *context, struct irInstructionAllocator *ir)
{
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};
    struct irRegister *newPc;

    ir->add_call_void(ir, "arm_hlp_vdso_cmpxchg",
                      ir->add_mov_const_64(ir, (uint64_t) arm_hlp_vdso_cmpxchg),
                      param);

    newPc = read_reg(context, ir, 14);
    write_reg(context, ir, 15, newPc);
    ir->add_exit(ir, ir->add_32U_to_64(ir, newPc));

    return 1;
}

static int vdso_memory_barrier(struct arm_target *context, struct irInstructionAllocator *ir)
{
    struct irRegister *newPc;

    newPc = read_reg(context, ir, 14);
    write_reg(context, ir, 15, newPc);
    ir->add_exit(ir, ir->add_32U_to_64(ir, newPc));

    return 1;
}

static int vdso_get_tls(struct arm_target *context, struct irInstructionAllocator *ir)
{
    struct irRegister *newPc;

    write_reg(context, ir, 0, ir->add_read_context_32(ir, offsetof(struct arm_registers, c13_tls2)));

    newPc = read_reg(context, ir, 14);
    write_reg(context, ir, 15, newPc);
    ir->add_exit(ir, ir->add_32U_to_64(ir, newPc));

    return 1;
}
/* api */
void disassemble_arm(struct target *target, struct irInstructionAllocator *ir, uint64_t pc, int maxInsn)
{
    struct arm_target *context = container_of(target, struct arm_target, target);
    int i;
    int isExit = 0; //unconditionnal exit
    uint32_t *pc_ptr = (uint32_t *) g_2_h(pc);

    assert((pc & 3) == 0);
    for(i = 0; i < maxInsn; i++) {
        context->pc = h_2_g(pc_ptr);
        if (context->pc == 0xffff0fc0) {
            isExit = vdso_cmpxchg(context, ir);
        } else if (context->pc == 0xffff0fa0) {
            isExit = vdso_memory_barrier(context, ir);
        } else if (context->pc == 0xffff0fe0) {
            isExit = vdso_get_tls(context, ir);
        } else {
            isExit = disassemble_insn(context, *pc_ptr, ir);
            pc_ptr++;
        }
        dump_state(context, ir);
        if (isExit)
            break;
    }
    if (!isExit) {
        write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc + 4));
        ir->add_exit(ir, ir->add_mov_const_64(ir, context->pc + 4));
    }
}
