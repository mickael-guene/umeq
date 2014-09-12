#include <stdio.h>
#include <assert.h>
#include <stddef.h>

#include "arm_private.h"
#include "arm_helpers.h"
#include "runtime.h"

//#define DUMP_STATE  1
#define INSN(msb, lsb) ((insn >> (lsb)) & ((1 << ((msb) - (lsb) + 1))-1))
#define INSN1(msb, lsb) INSN(msb+16, lsb+16)
#define INSN2(msb, lsb) INSN(msb, lsb)

/* it block handle */
static int inItBlock(struct arm_target *context)
{
    return (context->disa_itstate & 0xf) != 0;
}

static int lastInItBlock(struct arm_target *context)
{
    return (context->disa_itstate & 0xf) == 0x8;
}

static uint32_t nextItState(struct arm_target *context)
{
    if ((context->disa_itstate & 0x7) == 0)
        return 0;
    else
        return (context->disa_itstate & 0xe0) | ((context->disa_itstate << 1) & 0x1f);
}

static void itAdvance(struct arm_target *context)
{
    context->disa_itstate = nextItState(context);
}

static uint32_t ThumbExpandimm_C_imm32(uint32_t imm12)
{
    uint32_t res;

    if (imm12 & 0xc00) {
        uint32_t unrotated_value = 0x80 + (imm12 & 0x7f);
        uint32_t shift = (imm12 >> 7) & 0x1f;

        res = (unrotated_value >> shift) + (unrotated_value << (32 - shift));
    } else {
        uint32_t imm8 = imm12 & 0xff;
        switch((imm12 >> 8) & 3) {
            case 0:
                res = imm8;
                break;
            case 1:
                res = (imm8 << 16) + imm8;
                break;
            case 2:
                res = (imm8 << 24) + (imm8 << 8);
                break;
            case 3:
                res = (imm8 << 24) + (imm8 << 16) + (imm8 << 8) + imm8;
                break;
        }
    }

    return res;
}

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
static void dump_state_and_assert(struct arm_target *context, struct irInstructionAllocator *ir)
{
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};

    ir->add_call_void(ir, "arm_hlp_dump_and_assert",
                      ir->add_mov_const_64(ir, (uint64_t) arm_hlp_dump_and_assert),
                      param);
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
        return mk_32(ir, (context->pc + 4) & ~1);
    else
        return ir->add_read_context_32(ir, offsetof(struct arm_registers, r[index]));
}

static void write_reg(struct arm_target *context, struct irInstructionAllocator *ir, int index, struct irRegister *value)
{
    ir->add_write_context_32(ir, value, offsetof(struct arm_registers, r[index]));
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

static void write_itstate(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *value)
{
    ir->add_write_context_32(ir, value, offsetof(struct arm_registers, reg_itstate));
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

static struct irRegister *mk_sco_t2(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *insn,
                                 struct irRegister *rm, struct irRegister * shift_op)
{
    struct irRegister *params[4];

    params[0] = insn;
    params[1] = rm;
    params[2] = shift_op;
    params[3] = read_cpsr(context, ir);

    return ir->add_call_32(ir, "thumb_t2_hlp_compute_sco",
                               ir->add_mov_const_64(ir, (uint64_t) thumb_t2_hlp_compute_sco),
                               params);
}

static int mk_data_processing_modified(struct arm_target *context, struct irInstructionAllocator *ir, int insn, struct irRegister *op2)
{
    int opcode = INSN1(8, 5);
    int rd = INSN2(11, 8);
    int rn = INSN1(3, 0);
    int s = INSN1(4, 4);
    int isExit = (rd == 15)?1:0;
    struct irRegister *op1 = read_reg(context, ir, rn);
    struct irRegister *result = NULL;
    struct irRegister *nextCpsr;

    if (s) {
        struct irRegister *params[4];

        params[0] = ir->add_or_32(ir,
                                  ir->add_mov_const_32(ir, opcode),
                                  read_sco(context, ir));
        params[1] = op1; //default value
        switch(opcode) {
            case 2: case 3:
                if (rn == 15)
                    params[1] = mk_32(ir, 0);
                break;
        }
        params[2] = op2;
        params[3] = read_cpsr(context, ir);

        nextCpsr = ir->add_call_32(ir, "thumb_hlp_t2_modified_compute_next_flags",
                                   ir->add_mov_const_64(ir, (uint64_t) thumb_hlp_t2_modified_compute_next_flags),
                                   params);
    }
    switch(opcode) {
        case 0://and / test
            if (rd == 15)
                isExit = 0;
            else
                result = ir->add_and_32(ir, op1, op2);
            break;
        case 1://bic
            result = ir->add_and_32(ir,
                                    op1,
                                    ir->add_xor_32(ir, op2, mk_32(ir, 0xffffffff)));
            break;
        case 2://mov / orr
            if (rn == 15)
                result = op2;
            else
                result = ir->add_or_32(ir, op1, op2);
            break;
        case 3://mvn / orn
            if (rn == 15)
                result = ir->add_xor_32(ir, op2, mk_32(ir, 0xffffffff));
            else
                result = ir->add_or_32(ir, op1,
                                           ir->add_xor_32(ir, op2, mk_32(ir, 0xffffffff)));
            break;
        case 4:// eor/teq
            if (rd == 15) {
                assert(s == 1);
                isExit = 0;
            } else
                result = ir->add_xor_32(ir, op1, op2);
            break;
        case 8:// add/cmn
            if (rd == 15) {
                assert(s == 1);
                isExit = 0;
            } else
                result = ir->add_add_32(ir, op1, op2);
            break;
        case 10://adc
            {
                struct irRegister *c = ir->add_and_32(ir,
                                                      ir->add_shr_32(ir, read_cpsr(context, ir), mk_8(ir, 29)),
                                                      mk_32(ir, 1));
                result = ir->add_add_32(ir, op1, ir->add_add_32(ir, op2, c));
            }
            break;
        case 11: //sbc
            {
                struct irRegister *c = ir->add_and_32(ir,
                                                      ir->add_shr_32(ir, read_cpsr(context, ir), mk_8(ir, 29)),
                                                      mk_32(ir, 1));
                result = ir->add_add_32(ir, op1, ir->add_add_32(ir, ir->add_xor_32(ir, op2, mk_32(ir, 0xffffffff)), c));
            }
            break;
        case 13://sub/cmp
            if (rd == 15) {
                assert(s == 1);
                isExit = 0;
            } else
                result = ir->add_sub_32(ir, op1, op2);
            break;
        case 14://rsb
            result = ir->add_sub_32(ir, op2, op1);
            break;
        default:
            fatal("insn=0x%x op=%d(0x%x)\n", insn, opcode, opcode);
    }

    assert(isExit == 0);
    if (result)
        write_reg(context, ir, rd, result);
    if (s)
        write_cpsr(context, ir, nextCpsr);

    return isExit;
}

static struct irRegister *mk_ror_imm_32(struct irInstructionAllocator *ir, struct irRegister *op, int rotation)
{
    return ir->add_or_32(ir,
                         ir->add_shr_32(ir, op, mk_8(ir, rotation)),
                         ir->add_shl_32(ir, op, mk_8(ir, 32-rotation)));
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

static int mk_load_store_byte_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir,
                                                    int l, int rt, int rn, uint32_t imm32)
{
    struct irRegister *address;

    address = ir->add_add_32(ir, read_reg(context, ir, rn), mk_32(ir, imm32));
    if (l) {
        write_reg(context, ir, rt, ir->add_8U_to_32(ir, ir->add_load_8(ir, address)));
    } else {
        ir->add_store_8(ir, ir->add_32_to_8(ir, read_reg(context, ir, rt)), address);
    }

    return 0;
}

static int mk_load_store_word_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir,
                                                    int l, int rt, int rn, uint32_t imm32)
{
    struct irRegister *address;

    address = ir->add_add_32(ir, read_reg(context, ir, rn), mk_32(ir, imm32));
    if (l) {
        write_reg(context, ir, rt, ir->add_load_32(ir, address));
    } else {
        ir->add_store_32(ir, read_reg(context, ir, rt), address);
    }

    return 0;
}

static int mk_t1_add_sub_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir,
                                   int isAdd, int rd, int rn, uint32_t imm32)
{
    int s = !inItBlock(context);
    struct irRegister *nextCpsr;
    int opcode = INSN(13, 9);

    if (s) {
        struct irRegister *params[4];

        params[0] = ir->add_or_32(ir,
                                  ir->add_mov_const_32(ir, opcode),
                                  read_sco(context, ir));
        params[1] = read_reg(context, ir, rn);
        params[2] = mk_32(ir, imm32);
        params[3] = read_cpsr(context, ir);

        nextCpsr = ir->add_call_32(ir, "thumb_hlp_compute_next_flags",
                                   ir->add_mov_const_64(ir, (uint64_t) thumb_hlp_compute_next_flags),
                                   params);
    }

    if (isAdd)
        write_reg(context, ir, rd, ir->add_add_32(ir, read_reg(context, ir, rn), mk_32(ir, imm32)));
    else
        write_reg(context, ir, rd, ir->add_sub_32(ir, read_reg(context, ir, rn), mk_32(ir, imm32)));

    if (s) {
        write_cpsr(context, ir, nextCpsr);
    }

    return 0;
}

static struct irRegister *mk_mul_u_lsb(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *op1, struct irRegister *op2)
{
    struct irRegister *param[4] = {op1, op2, NULL, NULL};

    return ir->add_call_32(ir, "arm_hlp_multiply_unsigned_lsb",
                           ir->add_mov_const_64(ir, (uint64_t) arm_hlp_multiply_unsigned_lsb),
                           param);
}

static struct irRegister *mk_mul_u_msb(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *op1, struct irRegister *op2)
{
    struct irRegister *param[4] = {op1, op2, NULL, NULL};

    return ir->add_call_32(ir, "arm_hlp_multiply_unsigned_msb",
                           ir->add_mov_const_64(ir, (uint64_t) arm_hlp_multiply_unsigned_msb),
                           param);
}

/* disassembler to ir */
 /**/

 /* code generators */
static int dis_t2_b_t3(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int cond = INSN1(9, 6);
    int32_t imm32 = (INSN1(10, 10) << 20) | (INSN2(11, 11) << 19) | (INSN2(13, 13) << 18) | (INSN1(5, 0) << 12) | (INSN2(10, 0) << 1);
    uint32_t branch_target;

    dump_state(context, ir);
    /* early exit */
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
    } else
        fatal("cond(%d) >= 14 ?\n", cond);

    /* sign extend */
    imm32 = (imm32 << 11) >> 11;

    branch_target = context->pc + 4 + imm32;
    write_reg(context, ir, 15, ir->add_mov_const_32(ir, branch_target));
    ir->add_exit(ir, ir->add_mov_const_64(ir, branch_target));

    return 1;
}

static int dis_t1_push(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int reglist = INSN(7, 0) + (INSN(8,8)?0x4000:0);
    struct irRegister *start_address;
    int offset = 0;
    int bitNb = 0;
    int i;

    /* compute start address */
    for(i=0;i<16;i++) {
        if ((reglist >> i) & 1)
            bitNb++;
    }
    start_address = ir->add_sub_32(ir, read_reg(context, ir, 13), mk_32(ir, 4 * bitNb));

    /* push registers */
    for(i = 0; i < 15; i++) {
        if ((reglist >> i) & 1) {
            ir->add_store_32(ir, read_reg(context, ir, i),
                                 ir->add_add_32(ir, start_address, mk_32(ir, offset)));
            offset += 4;
        }
    }

    /* update sp */
    write_reg(context, ir, 13, start_address);

    return 0;
}

static int dis_t1_pop(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int reglist = INSN(7, 0);
    int p = INSN(8,8);
    int offset = 0;
    struct irRegister *start_address;
    struct irRegister *newPc = NULL;
    int isExit = 0;
    int i;

    start_address = read_reg(context, ir, 13);
    /* pop registers */
    for(i = 0; i < 8; i++) {
        if ((reglist >> i) & 1) {
            write_reg(context, ir, i,
                      ir->add_load_32(ir,
                                      ir->add_add_32(ir, start_address, mk_32(ir, offset))));
            offset += 4;
        }
    }
    if (p) {
        isExit = 1;
        newPc = ir->add_load_32(ir, ir->add_add_32(ir, start_address, mk_32(ir, offset)));
        write_reg(context, ir, 15, newPc);
        offset += 4;
    }
    /* update sp */
    write_reg(context, ir, 13, ir->add_add_32(ir, start_address, mk_32(ir, offset)));

    if (isExit)
        ir->add_exit(ir, ir->add_32U_to_64(ir, newPc));

    return isExit;
}

static int dis_t1_sp_relative_addr(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(10, 8);
    uint32_t imm32 = INSN(7, 0) << 2;

    write_reg(context, ir, rd, ir->add_add_32(ir, read_reg(context, ir, 13), mk_32(ir, imm32)));

    return 0;
}

static int dis_t1_sp_plus_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    uint32_t imm32 = INSN(6, 0) << 2;

    write_reg(context, ir, 13, ir->add_add_32(ir, read_reg(context, ir, 13), mk_32(ir, imm32)));

    return 0;
}

static int dis_t1_sp_minus_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    uint32_t imm32 = INSN(6, 0) << 2;

    write_reg(context, ir, 13, ir->add_sub_32(ir, read_reg(context, ir, 13), mk_32(ir, imm32)));

    return 0;
}

static int dis_t1_sxth(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN(5, 3);
    int rd = INSN(2, 0);

    write_reg(context, ir, rd, ir->add_16S_to_32(ir, ir->add_32_to_16(ir, read_reg(context, ir, rm))));

    return 0;
}

static int dis_t1_uxtb(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN(5, 3);
    int rd = INSN(2, 0);

    write_reg(context, ir, rd, ir->add_and_32(ir, read_reg(context, ir, rm), mk_32(ir, 0xff)));

    return 0;
}

static int dis_t1_mov_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(10, 8);
    uint32_t imm32 = INSN(7, 0);
    int s = !inItBlock(context);
    struct irRegister *nextCpsr;
    int opcode = INSN(13, 9);

    if (s) {
        struct irRegister *params[4];

        params[0] = ir->add_or_32(ir,
                                  ir->add_mov_const_32(ir, opcode),
                                  read_sco(context, ir));
        params[1] = 0;
        params[2] = mk_32(ir, imm32);
        params[3] = read_cpsr(context, ir);

        nextCpsr = ir->add_call_32(ir, "thumb_hlp_compute_next_flags",
                                   ir->add_mov_const_64(ir, (uint64_t) thumb_hlp_compute_next_flags),
                                   params);
    }
    write_reg(context, ir, rd, mk_32(ir, imm32));
    if (s) {
        write_cpsr(context, ir, nextCpsr);
    }

    return 0;
}

static int dis_t1_cmp_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rn = INSN(10, 8);
    uint32_t imm32 = INSN(7, 0);
    int s = 1;
    struct irRegister *nextCpsr;
    int opcode = INSN(13, 9);

    if (s) {
        struct irRegister *params[4];

        params[0] = ir->add_or_32(ir,
                                  ir->add_mov_const_32(ir, opcode),
                                  read_sco(context, ir));
        params[1] = read_reg(context, ir, rn);
        params[2] = mk_32(ir, imm32);
        params[3] = read_cpsr(context, ir);

        nextCpsr = ir->add_call_32(ir, "thumb_hlp_compute_next_flags",
                                   ir->add_mov_const_64(ir, (uint64_t) thumb_hlp_compute_next_flags),
                                   params);
    }
    if (s) {
        write_cpsr(context, ir, nextCpsr);
    }

    return 0;
}

static int dis_t1_add_3_bits_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(2, 0);
    int rn = INSN(5, 3);
    uint32_t imm32 = INSN(8, 6);

    return mk_t1_add_sub_immediate(context, insn, ir, 1/*isAdd*/, rd, rn, imm32);
}

static int dis_t1_sub_3_bits_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(2, 0);
    int rn = INSN(5, 3);
    uint32_t imm32 = INSN(8, 6);

    return mk_t1_add_sub_immediate(context, insn, ir, 0/*isAdd*/, rd, rn, imm32);
}

static int dis_t1_add_8_bits_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rdn = INSN(10, 8);
    uint32_t imm32 = INSN(7, 0);

    return mk_t1_add_sub_immediate(context, insn, ir, 1/*isAdd*/, rdn, rdn, imm32);
}

static int dis_t1_sub_8_bits_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rdn = INSN(10, 8);
    uint32_t imm32 = INSN(7, 0);

    return mk_t1_add_sub_immediate(context, insn, ir, 0/*isAdd*/, rdn, rdn, imm32);
}

static int dis_t1_lsl_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN(5, 3);
    int rd = INSN(2, 0);
    int imm5 = INSN(10, 6);
    int s = !inItBlock(context);
    struct irRegister *rm_reg = read_reg(context, ir, rm);
    struct irRegister *nextCpsr;
    int opcode = INSN(13, 9);
    struct irRegister *result;

    result = ir->add_shl_32(ir, rm_reg, mk_8(ir, imm5));
    if (s) {
        write_sco(context, ir, mk_sco(context, ir,
                                               mk_32(ir, 0/* shift mode */),
                                               rm_reg,
                                               mk_32(ir, imm5)));
    }
    if (s) {
        struct irRegister *params[4];

        params[0] = ir->add_or_32(ir,
                                  ir->add_mov_const_32(ir, opcode),
                                  read_sco(context, ir));
        params[1] = result;
        params[2] = result;
        params[3] = read_cpsr(context, ir);

        nextCpsr = ir->add_call_32(ir, "thumb_hlp_compute_next_flags",
                                   ir->add_mov_const_64(ir, (uint64_t) thumb_hlp_compute_next_flags),
                                   params);
    }

    write_reg(context, ir, rd, result);

    if (s) {
        write_cpsr(context, ir, nextCpsr);
    }

    return 0;
}

static int dis_t1_lsr_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN(5, 3);
    int rd = INSN(2, 0);
    int imm5 = INSN(10, 6);
    int s = !inItBlock(context);
    struct irRegister *rm_reg = read_reg(context, ir, rm);
    struct irRegister *nextCpsr;
    int opcode = INSN(13, 9);
    struct irRegister *result;

    result = ir->add_shr_32(ir, rm_reg, mk_8(ir, imm5));
    if (s) {
        write_sco(context, ir, mk_sco(context, ir,
                                               mk_32(ir, 1/* shift mode */),
                                               rm_reg,
                                               mk_32(ir, imm5)));
    }
    if (s) {
        struct irRegister *params[4];

        params[0] = ir->add_or_32(ir,
                                  ir->add_mov_const_32(ir, opcode),
                                  read_sco(context, ir));
        params[1] = result;
        params[2] = result;
        params[3] = read_cpsr(context, ir);

        nextCpsr = ir->add_call_32(ir, "thumb_hlp_compute_next_flags",
                                   ir->add_mov_const_64(ir, (uint64_t) thumb_hlp_compute_next_flags),
                                   params);
    }

    write_reg(context, ir, rd, result);

    if (s) {
        write_cpsr(context, ir, nextCpsr);
    }

    return 0;
}

static int dis_t1_asr_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN(5, 3);
    int rd = INSN(2, 0);
    int imm5 = INSN(10, 6);
    int s = !inItBlock(context);
    struct irRegister *rm_reg = read_reg(context, ir, rm);
    struct irRegister *nextCpsr;
    int opcode = INSN(13, 9);
    struct irRegister *result;

    result = ir->add_asr_32(ir, rm_reg, mk_8(ir, imm5));
    if (s) {
        write_sco(context, ir, mk_sco(context, ir,
                                               mk_32(ir, 2/* shift mode */),
                                               rm_reg,
                                               mk_32(ir, imm5)));
    }
    if (s) {
        struct irRegister *params[4];

        params[0] = ir->add_or_32(ir,
                                  ir->add_mov_const_32(ir, opcode),
                                  read_sco(context, ir));
        params[1] = result;
        params[2] = result;
        params[3] = read_cpsr(context, ir);

        nextCpsr = ir->add_call_32(ir, "thumb_hlp_compute_next_flags",
                                   ir->add_mov_const_64(ir, (uint64_t) thumb_hlp_compute_next_flags),
                                   params);
    }

    write_reg(context, ir, rd, result);

    if (s) {
        write_cpsr(context, ir, nextCpsr);
    }

    return 0;
}

static int dis_t1_add_register(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(2, 0);
    int rn = INSN(5, 3);
    int rm = INSN(8, 6);
    int s = !inItBlock(context);
    struct irRegister *nextCpsr;
    int opcode = INSN(13, 9);

    if (s) {
        struct irRegister *params[4];

        params[0] = ir->add_or_32(ir,
                                  ir->add_mov_const_32(ir, opcode),
                                  read_sco(context, ir));
        params[1] = read_reg(context, ir, rn);
        params[2] = read_reg(context, ir, rm);
        params[3] = read_cpsr(context, ir);

        nextCpsr = ir->add_call_32(ir, "thumb_hlp_compute_next_flags",
                                   ir->add_mov_const_64(ir, (uint64_t) thumb_hlp_compute_next_flags),
                                   params);
    }

    write_reg(context, ir, rd, ir->add_add_32(ir,
                                              read_reg(context, ir, rn),
                                              read_reg(context, ir, rm)));

    if (s) {
        write_cpsr(context, ir, nextCpsr);
    }

    return 0;
}

static int dis_t1_sub_register(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(2, 0);
    int rn = INSN(5, 3);
    int rm = INSN(8, 6);
    int s = !inItBlock(context);
    struct irRegister *nextCpsr;
    int opcode = INSN(13, 9);

    if (s) {
        struct irRegister *params[4];

        params[0] = ir->add_or_32(ir,
                                  ir->add_mov_const_32(ir, opcode),
                                  read_sco(context, ir));
        params[1] = read_reg(context, ir, rn);
        params[2] = read_reg(context, ir, rm);
        params[3] = read_cpsr(context, ir);

        nextCpsr = ir->add_call_32(ir, "thumb_hlp_compute_next_flags",
                                   ir->add_mov_const_64(ir, (uint64_t) thumb_hlp_compute_next_flags),
                                   params);
    }

    write_reg(context, ir, rd, ir->add_sub_32(ir,
                                              read_reg(context, ir, rn),
                                              read_reg(context, ir, rm)));

    if (s) {
        write_cpsr(context, ir, nextCpsr);
    }

    return 0;
}

static int dis_t1_load_store_byte_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int l = INSN(11, 11);
    int rt = INSN(2, 0);
    int rn = INSN(5, 3);
    uint32_t imm32 = INSN(10, 6);

    return mk_load_store_byte_immediate(context, insn, ir, l, rt, rn, imm32);
}

static int dis_t1_load_store_word_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int l = INSN(11, 11);
    int rt = INSN(2, 0);
    int rn = INSN(5, 3);
    uint32_t imm32 = INSN(10, 6) << 2;

    return mk_load_store_word_immediate(context, insn, ir, l, rt, rn, imm32);
}

static int dis_t1_load_store_word_immediate_sp_relative(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int l = INSN(11, 11);
    int rt = INSN(10, 8);
    uint32_t imm32 = INSN(7, 0) << 2;

    return mk_load_store_word_immediate(context, insn, ir, l, rt, 13, imm32);
}

static int dis_t1_add_register_t2(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int rdn = (INSN(7, 7) << 3) | INSN(2, 0);
    int rm = INSN(6, 3);

    assert(rdn != 15 && "implement me");

    write_reg(context, ir, rdn, ir->add_add_32(ir,
                                               read_reg(context, ir, rdn),
                                               read_reg(context, ir, rm)));

    return isExit;
}

static int dis_t1_cmp_register_t2(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN(6, 3);
    int rn = (INSN(7, 7) << 3) | INSN(2, 0);
    struct irRegister *params[4];
    struct irRegister *nextCpsr;

    params[0] = mk_32(ir, 10);
    params[1] = read_reg(context, ir, rn);
    params[2] = read_reg(context, ir, rm);
    params[3] = read_cpsr(context, ir);

    nextCpsr = ir->add_call_32(ir, "thumb_hlp_compute_next_flags_data_processing",
                               ir->add_mov_const_64(ir, (uint64_t) thumb_hlp_compute_next_flags_data_processing),
                               params);
    write_cpsr(context, ir, nextCpsr);

    return 0;
}

static int dis_t1_mov_high_low_registers(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = (INSN(7, 7) << 3) + INSN(2, 0);
    int rm = INSN(6, 3);

    write_reg(context, ir, rd, read_reg(context, ir, rm));

    return 0;
}

static int dis_t1_svc(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};

    /* be sure pc as the correct value so clone syscall can use pc value */
    write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc));
    ir->add_call_void(ir, "arm_hlp_syscall",
                      ir->add_mov_const_64(ir, (uint64_t) arm_hlp_syscall),
                      param);

    write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc + 2));
    ir->add_exit(ir, ir->add_mov_const_64(ir, context->pc + 2));

    return 1;
}

static int dis_t1_bx(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN(6, 3);
    struct irRegister *dst = read_reg(context, ir, rm);

    dump_state(context, ir);
    write_reg(context, ir, 15, dst);
    ir->add_exit(ir, ir->add_32U_to_64(ir, dst));

    return 1;
}

static int dis_t1_blx(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN(6, 3);
    struct irRegister *dst = read_reg(context, ir, rm);

    write_reg(context, ir, 14, mk_32(ir, context->pc + 2));
    write_reg(context, ir, 15, dst);
    ir->add_exit(ir, ir->add_32U_to_64(ir, dst));

    return 1;
}

static int dis_t1_b_t1(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int32_t imm32 = INSN(7, 0);
    int cond = INSN(11, 8);
    uint32_t branch_target;

    dump_state(context, ir);
    /* early exit */
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
        write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc + 2));
        ir->add_exit_cond(ir, ir->add_mov_const_64(ir, context->pc + 2), pred);
        //Following code is useless except when dumping state
#if DUMP_STATE
        write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc));
#endif
    } else
        fatal("cond(%d) >= 14 ?\n", cond);

    /* sign extend */
    imm32 = (imm32 << 24) >> 23;

    branch_target = context->pc + 4 + imm32;
    write_reg(context, ir, 15, ir->add_mov_const_32(ir, branch_target));
    ir->add_exit(ir, ir->add_mov_const_64(ir, branch_target));

    return 1;
}

static int dis_t1_b_t2(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int32_t imm32 = INSN(10, 0);
    uint32_t branch_target;

    /* sign extend */
    imm32 = (imm32 << 21) >> 20;

    branch_target = context->pc + 4 + imm32;
    dump_state(context, ir);
    write_reg(context, ir, 15, ir->add_mov_const_32(ir, branch_target));
    ir->add_exit(ir, ir->add_mov_const_64(ir, branch_target));

    return 1;
}

static int dis_t2_movw(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN2(11, 8);
    uint32_t imm32 = (INSN1(3, 0) << 12) | (INSN1(10,10) << 11) | (INSN2(14,12) << 8) | (INSN2(7, 0));

    write_reg(context, ir, rd, mk_32(ir, imm32));

    return 0;
}

static int dis_t2_movt(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN2(11, 8);
    uint16_t imm16 = (INSN1(3, 0) << 12) | (INSN1(10,10) << 11) | (INSN2(14,12) << 8) | (INSN2(7, 0));

    /* only write msb part of rd */
    ir->add_write_context_16(ir, mk_16(ir, imm16), offsetof(struct arm_registers, r[rd]) + 2);

    return 0;
}

static int dis_t2_branch_with_link(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int s = INSN1(10, 10);
    int i1 = 1 - (INSN2(13, 13) ^ s);
    int i2 = 1 - (INSN2(11, 11) ^ s);
    int32_t imm32 = (s << 24) | ((i1 << 23) | (i2 << 22) | (INSN1(9, 0) << 12) | (INSN2(10, 0) << 1));
    uint32_t branch_target;

    /* sign extend */
    imm32 = (imm32 << 7) >> 7;
    branch_target = context->pc + 4 + imm32;
    write_reg(context, ir, 14, ir->add_mov_const_32(ir, context->pc + 4));
    dump_state(context, ir);
    write_reg(context, ir, 15, ir->add_mov_const_32(ir, branch_target));
    ir->add_exit(ir, ir->add_mov_const_64(ir, branch_target));

    return 1;
}

static int dis_t2_branch_with_link_exchange(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int s = INSN1(10, 10);
    int i1 = 1 - (INSN2(13, 13) ^ s);
    int i2 = 1 - (INSN2(11, 11) ^ s);
    int32_t imm32 = (s << 24) | ((i1 << 23) | (i2 << 22) | (INSN1(9, 0) << 12) | (INSN2(10, 1) << 2));
    uint32_t branch_target;

    /* sign extend */
    imm32 = (imm32 << 7) >> 7;
    branch_target = (context->pc + 4 + imm32) & ~3;
    write_reg(context, ir, 14, ir->add_mov_const_32(ir, context->pc + 4));
    dump_state(context, ir);
    write_reg(context, ir, 15, ir->add_mov_const_32(ir, branch_target));
    ir->add_exit(ir, ir->add_mov_const_64(ir, branch_target));

    return 1;
}

static int dis_t2_branch_t4(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int s = INSN1(10, 10);
    int i1 = 1 - (INSN2(13, 13) ^ s);
    int i2 = 1 - (INSN2(11, 11) ^ s);
    int32_t imm32 = (s << 24) | ((i1 << 23) | (i2 << 22) | (INSN1(9, 0) << 12) | (INSN2(10, 0) << 1));
    uint32_t branch_target;

    /* sign extend */
    imm32 = (imm32 << 7) >> 7;
    branch_target = (context->pc + 4 + imm32);
    dump_state(context, ir);
    write_reg(context, ir, 15, ir->add_mov_const_32(ir, branch_target));
    ir->add_exit(ir, ir->add_mov_const_64(ir, branch_target));

    return 1;
}

static int dis_t2_ldr_literal(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int u = INSN1(7, 7);
    int rt = INSN2(15, 12);
    uint32_t imm32 = INSN2(11, 0);
    uint32_t base = (context->pc + 4) & ~3;
    uint32_t address = u?base+imm32:base-imm32;

    assert(rt != 15 && "implement me");

    write_reg(context, ir, rt, ir->add_load_32(ir, mk_32(ir, address)));

    return isExit;
}

static int dis_t2_ldr_immediate_t3(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rt = INSN2(15, 12);
    int rn = INSN1(3, 0);
    uint32_t imm32 = INSN2(11, 0);

    return mk_load_store_word_immediate(context, insn, ir, 1, rt, rn, imm32);
}

static int dis_t2_ldr_register(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int rn = INSN1(3, 0);
    int rt = INSN2(15, 12);
    int rm = INSN2(3, 0);
    int shift_imm = INSN2(5, 4);
    struct irRegister *address;

    assert(rt != 15 && "implement me");

    address = ir->add_add_32(ir, read_reg(context, ir, rn),
                                 ir->add_shl_32(ir,
                                                read_reg(context, ir, rm),
                                                mk_8(ir, shift_imm)));

    write_reg(context, ir, rt, ir->add_load_32(ir, address));

    return isExit;
}

static int dis_t2_ldrt(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
}

static int dis_t2_ldr_immediate_t4(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rn = INSN1(3, 0);
    int rt = INSN2(15, 12);
    int p = INSN2(10, 10);
    int u = INSN2(9, 9);
    int w = INSN2(8, 8);
    uint32_t imm32 = INSN2(7, 0);
    struct irRegister *wb;
    struct irRegister *address;
    int isExit = 0;

    /* compute load address */
    if (p) {
        if (u)
            address = ir->add_add_32(ir, read_reg(context, ir, rn), mk_32(ir, imm32));
        else
            address = ir->add_sub_32(ir, read_reg(context, ir, rn), mk_32(ir, imm32));
    } else
        address = read_reg(context, ir, rn);
    /* process write back case */
    if (w) {
        if (u)
            wb = ir->add_add_32(ir, read_reg(context, ir, rn), mk_32(ir, imm32));
        else
            wb = ir->add_sub_32(ir, read_reg(context, ir, rn), mk_32(ir, imm32));
        write_reg(context, ir, rn, wb);
    }

    /* update destination register */
    if (rt == 15) {
        struct irRegister *newPc;

        newPc = ir->add_load_32(ir, address);
        write_reg(context, ir, 15, newPc);
        ir->add_exit(ir, ir->add_32U_to_64(ir, newPc));
        isExit = 1;
    } else
        write_reg(context, ir, rt, ir->add_load_32(ir, address));

    return isExit;
}

static int dis_t2_load_store_double_immediate_offset(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int p = INSN1(8, 8);
    int u = INSN1(7, 7);
    int w = INSN1(5, 5);
    int l = INSN1(4, 4);
    int rn = INSN1(3, 0);
    int rt = INSN2(15,12);
    int rt2 = INSN2(11,8);
    int offset = INSN2(7, 0) << 2;
    struct irRegister *address;

    assert(rt != 15 && "implement it");
    assert(rt2 != 15 && "implement it");
    assert(rn != 15);
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
        write_reg(context, ir, rt, ir->add_load_32(ir, address));
        write_reg(context, ir, rt2, ir->add_load_32(ir, ir->add_add_32(ir, address, mk_32(ir, 4))));
    } else {
        ir->add_store_32(ir, read_reg(context, ir, rt), address);
        ir->add_store_32(ir, read_reg(context, ir, rt2), ir->add_add_32(ir, address, mk_32(ir, 4)));
    }
    /* write-back if needed */
    if (p && w) {
        //pre-indexed
        write_reg(context, ir, rn, address);
    } else if (!p && w) {
        //post-indexed
        struct irRegister *newVal;

        newVal = address_register_offset(context, ir, rn, (u==1?offset:-offset));
        write_reg(context, ir, rn, newVal);
    }

    return 0;
}

static int dis_t2_ldrb_t2(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rn = INSN1(3, 0);
    int rt = INSN2(15, 12);
    uint32_t imm32 = INSN2(11, 0);
    struct irRegister *address;

    assert(rt != 15);
    assert(rn != 15);

    address = ir->add_add_32(ir, read_reg(context, ir, rn), mk_32(ir, imm32));
    write_reg(context, ir, rt,
                           ir->add_8U_to_32(ir,
                                            ir->add_load_8(ir, address)));

    return 0;
}

static int dis_t2_ldrh_t2(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rn = INSN1(3, 0);
    int rt = INSN2(15, 12);
    uint32_t imm32 = INSN2(11, 0);
    struct irRegister *address;

    assert(rt != 15);
    assert(rn != 15);

    address = ir->add_add_32(ir, read_reg(context, ir, rn), mk_32(ir, imm32));
    write_reg(context, ir, rt,
                           ir->add_16U_to_32(ir,
                                            ir->add_load_16(ir, address)));

    return 0;
}

static int dis_t2_ldrb_t3(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rn = INSN1(3, 0);
    int rt = INSN2(15, 12);
    int p = INSN2(10, 10);
    int u = INSN2(9, 9);
    int w = INSN2(8, 8);
    int offset = INSN2(7, 0);
    struct irRegister *address;

     /* compute address of access */
    if (p) {
        //either offset or pre-indexedregs
        address = address_register_offset(context, ir, rn, (u==1?offset:-offset));
    } else {
        //post indexed
        address = address_register_offset(context, ir, rn, 0);
    }
    /* do load byte */
    write_reg(context, ir, rt, ir->add_8U_to_32(ir, ir->add_load_8(ir, address)));

    /* write-back if needed */
    if (p && w) {
        //pre-indexed
        write_reg(context, ir, rn, address);
    } else if (!p && w) {
        //post-indexed
        struct irRegister *newVal;

        newVal = address_register_offset(context, ir, rn, (u==1?offset:-offset));
        write_reg(context, ir, rn, newVal);
    }

    return 0;
}

static int dis_t2_data_processing_register_shift(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int shift_mode = INSN1(6, 5);
    int s = INSN1(4, 4);
    int rn = INSN1(3, 0);
    int rd = INSN2(11, 8);
    int rm = INSN2(3, 0);
    struct irRegister *result;
    struct irRegister *rn_reg;
    struct irRegister *rm_reg;

    assert(rn != 13 && rn !=15);
    assert(rd != 13 && rd !=15);
    assert(rm != 13 && rm !=15);

    assert(s == 0);

    rn_reg = read_reg(context, ir, rn);
    rm_reg = read_reg(context, ir, rm);
    switch(shift_mode) {
        case 0:
            result = ir->add_shl_32(ir,
                                    rn_reg,
                                    ir->add_32_to_8(ir, rm_reg));
            break;
        case 1:
            result = ir->add_shr_32(ir,
                                    rn_reg,
                                    ir->add_32_to_8(ir, rm_reg));
            break;
        case 2:
            result = ir->add_asr_32(ir,
                                    rn_reg,
                                    ir->add_32_to_8(ir, rm_reg));
            break;
        case 3:
            //ror
            result = mk_ror_reg_32(ir, rn_reg, rm_reg);
            break;
    default:
            assert(0);
    }
    write_reg(context, ir, rd, result);

    return 0;
}

static int dis_t1_itt_A_hints(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int opa = INSN(7, 4);
    int opb = INSN(3, 0);

    if (opb) {
        context->disa_itstate = INSN(7, 0);
        write_itstate(context, ir, mk_32(ir, context->disa_itstate));
    } else {
        //hints. nothing to do.
        ;
    }

    return 0;
}

static int dis_t2_sel(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rn = INSN1(3, 0);
    int rm = INSN2(3, 0);
    int rd = INSN2(11, 8);
    struct irRegister *params[4];
    struct irRegister *result;

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

static int dis_t2_clz(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN1(3, 0);
    int rd = INSN2(11, 8);
    int rm2 = INSN2(3, 0);
    struct irRegister *params[4];
    struct irRegister *result;

    assert(rm != 15);
    assert(rd != 15);
    assert(rm == rm2);

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

/* FIXME: boohhh. smell bad. reverse pred and conditionnal exit to branch destination */
static int dis_t1_cbxz(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int op = INSN(11, 11);
    int rn = INSN(2, 0);
    uint32_t imm32 = (INSN(9, 9) << 6) | (INSN(7, 3) << 1);
    struct irRegister *pred;
    uint32_t branch_target;

    if (op)
        pred = ir->add_cmpeq_32(ir, read_reg(context, ir, rn), mk_32(ir, 0));
    else
        pred = ir->add_cmpne_32(ir, read_reg(context, ir, rn), mk_32(ir, 0));
    /* just skip insn */
    write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc + 2));
    ir->add_exit_cond(ir, ir->add_mov_const_64(ir, context->pc + 2), pred);
    //Following code is useless except when dumping state
#if DUMP_STATE
    write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc));
#endif

    /* encode branch */
    branch_target = context->pc + 4 + imm32;
    write_reg(context, ir, 15, ir->add_mov_const_32(ir, branch_target));
    ir->add_exit(ir, ir->add_mov_const_64(ir, branch_target));

    return 1;
}

static int dis_t1_rev(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(2, 0);
    int rn = INSN(5, 3);
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

static int dis_t2_str_imm12(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int size = INSN1(6, 5);
    int rn = INSN1(3, 0);
    int rt = INSN2(15, 12);
    uint32_t imm32 = INSN2(11, 0);
    struct irRegister *address;

    //compute addresses
    address = ir->add_add_32(ir, read_reg(context, ir, rn), mk_32(ir, imm32));
    //do the store
    switch(size) {
        case 0://byte
            ir->add_store_8(ir, ir->add_32_to_8(ir, read_reg(context, ir, rt)), address);
            break;
        case 1://half-word
            ir->add_store_16(ir, ir->add_32_to_16(ir, read_reg(context, ir, rt)), address);
            break;
        case 2://word
            ir->add_store_32(ir, read_reg(context, ir, rt), address);
            break;
        default:
            assert(0);
    }

    return 0;
}

static int dis_t2_str_imm8(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int size = INSN1(6, 5);
    int rn = INSN1(3, 0);
    int rt = INSN2(15, 12);
    int p = INSN2(10, 10);
    int u = INSN2(9, 9);
    int w = INSN2(8, 8);
    uint32_t imm32 = INSN2(7, 0);
    struct irRegister *offset_addr;
    struct irRegister *address;
    struct irRegister *rn_reg;

    assert(rn != 15);
    assert(rt != 15);

    rn_reg = read_reg(context, ir, rn);
    //compute addresses
    if (u)
        offset_addr = ir->add_add_32(ir, rn_reg, mk_32(ir, imm32));
    else
        offset_addr = ir->add_sub_32(ir, rn_reg, mk_32(ir, imm32));
    if (p)
        address = offset_addr;
    else
        address = rn_reg;
    //do the store
    switch(size) {
        case 0://byte
            ir->add_store_8(ir, ir->add_32_to_8(ir, read_reg(context, ir, rt)), address);
            break;
        case 1://half-word
            ir->add_store_16(ir, ir->add_32_to_16(ir, read_reg(context, ir, rt)), address);
            break;
        case 2://word
            ir->add_store_32(ir, read_reg(context, ir, rt), address);
            break;
        default:
            assert(0);
    }

    //update rn if needed
    if (w)
        write_reg(context, ir, rn, offset_addr);

    return 0;
}

static int dis_t1_load_literal(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int rt = INSN(10, 8);
    uint32_t imm32 = INSN(7, 0) << 2;
    uint32_t address = ((context->pc + 4) & ~3) + imm32;

    write_reg(context, ir, rt, ir->add_load_32(ir, mk_32(ir, address)));

    return isExit;
}

static int dis_t2_load_multiple(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int rn = INSN1(3, 0);
    int w = INSN1(5, 5);
    int m = INSN1(7, 7);
    int reglist = (INSN2(15, 14) << 14) + INSN2(12, 0);
    int bitNb = 0;
    int offset = 0;
    struct irRegister *start_address;
    int i;
    struct irRegister *newPc = NULL;

    assert(rn != 15);
    assert(w != 1 || ((reglist >> rn) & 1) == 0);
    for(i=0;i<16;i++) {
        if ((reglist >> i) & 1)
            bitNb++;
    }
    assert(bitNb >= 2);

    /* compute start address */
    if (m) {
        start_address = read_reg(context, ir, rn);
    } else {
        start_address = ir->add_sub_32(ir, read_reg(context, ir, rn), mk_32(ir, 4 * bitNb));
    }
    /* restore regs */
    for(i=0;i<15;i++) {
        if ((reglist >> i) & 1) {
            write_reg(context, ir, i,
                      ir->add_load_32(ir,
                                      ir->add_add_32(ir, start_address, mk_32(ir, offset))));
            offset += 4;
        }
    }
    if ((reglist >> 15) & 1) {
        //pc will be update => block exit
        newPc = ir->add_load_32(ir, ir->add_add_32(ir, start_address, mk_32(ir, offset)));

        write_reg(context, ir, 15, newPc);
        isExit = 1;
        offset += 4;
    }

    /* write back rn */
    if (w) {
        if (m)
            write_reg(context, ir, rn, ir->add_add_32(ir, start_address, mk_32(ir, offset)));
        else
            write_reg(context, ir, rn, start_address);
    }

    if (isExit)
        ir->add_exit(ir, ir->add_32U_to_64(ir, newPc));

    return isExit;
}

static int dis_t2_store_multiple(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int rn = INSN1(3, 0);
    int w = INSN1(5, 5);
    int m = INSN1(7, 7);
    int reglist = (INSN2(14, 14) << 14) + INSN2(12, 0);
    int bitNb = 0;
    int offset = 0;
    struct irRegister *start_address;
    int i;

    assert(rn != 15);
    assert(w != 1 || ((reglist >> rn) & 1) == 0);
    for(i=0;i<16;i++) {
        if ((reglist >> i) & 1)
            bitNb++;
    }
    assert(bitNb >= 2);

    if (m) {
        start_address = read_reg(context, ir, rn);
    } else {
        start_address = ir->add_sub_32(ir, read_reg(context, ir, rn), mk_32(ir, 4 * bitNb));
    }

    for(i=0;i<16;i++) {
        if ((reglist >> i) & 1) {
            ir->add_store_32(ir, read_reg(context, ir, i),
                             ir->add_add_32(ir, start_address, mk_32(ir, offset)));
            offset += 4;
        }
    }

    if (w) {
        if (m)
            write_reg(context, ir, rn, ir->add_add_32(ir, start_address, mk_32(ir, offset)));
        else
            write_reg(context, ir, rn, start_address);
    }

    return isExit;
}

static int dis_t2_tbx(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rn = INSN1(3, 0);
    int rm = INSN2(3, 0);
    int h = INSN2(4, 4);
    struct irRegister *offset;
    struct irRegister *jump_target;

    if (h) {
        offset = ir->add_16U_to_32(ir,
                                   ir->add_load_16(ir, ir->add_add_32(ir,
                                                                      read_reg(context, ir, rn),
                                                                      ir->add_shl_32(ir,
                                                                                     read_reg(context, ir, rm),
                                                                                     mk_8(ir, 1)))));
    } else {
        offset = ir->add_8U_to_32(ir,
                                  ir->add_load_8(ir, ir->add_add_32(ir,
                                                                    read_reg(context, ir, rn),
                                                                    read_reg(context, ir, rm))));
    }
    jump_target = ir->add_add_32(ir,
                                 mk_32(ir, context->pc + 4),
                                 ir->add_shl_32(ir,
                                                offset,
                                                mk_8(ir, 1)));


    write_reg(context, ir, 15, jump_target);
    ir->add_exit(ir, ir->add_32U_to_64(ir, jump_target));

    return 1;
}

static int dis_t1_data_processing(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rdn = INSN(2, 0);
    int rm = INSN(5, 3);
    int opcode = INSN(9, 6);
    int s = !inItBlock(context);
    struct irRegister *params[4];
    struct irRegister *nextCpsr;
    struct irRegister *result = NULL;
    struct irRegister *op1 = read_reg(context, ir, rdn);
    struct irRegister *op2 = read_reg(context, ir, rm);

    if (s) {
        params[0] = mk_32(ir, opcode);
        params[1] = op1;
        params[2] = op2;
        params[3] = read_cpsr(context, ir);

        nextCpsr = ir->add_call_32(ir, "thumb_hlp_compute_next_flags_data_processing",
                                   ir->add_mov_const_64(ir, (uint64_t) thumb_hlp_compute_next_flags_data_processing),
                                   params);
    }

    switch(opcode) {
        case 0://and
            result = ir->add_and_32(ir, op1, op2);
            break;
        case 1://eor
            result = ir->add_xor_32(ir, op1, op2);
            break;
        case 2://lsl
            result = ir->add_shl_32(ir,
                                    op1,
                                    ir->add_32_to_8(ir, op2));
            break;
        case 8://tst
        case 10://cmp
            break;
        case 12://orr
            result = ir->add_or_32(ir, op1, op2);
            break;
        case 15://mvn
            result = ir->add_xor_32(ir, op2, mk_32(ir, 0xffffffff));
            break;
        default:
            fatal("opcode = %d(0x%x)\n", opcode, opcode);
    }

    if (result)
        write_reg(context, ir, rdn, result);
    if (s)
        write_cpsr(context, ir, nextCpsr);

    return 0;
}

static int dis_t2_mul(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN2(11, 8);
    int rn = INSN1(3, 0);
    int rm = INSN2(3, 0);

    write_reg(context, ir, rd,
              mk_mul_u_lsb(context, ir, read_reg(context, ir, rn), read_reg(context, ir, rm)));

    return 0;
}

static int dis_t2_mrc(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int opcode_1 = INSN1(7, 5);
    int crn = INSN1(3, 0);
    int rd = INSN2(15, 12);
    int cp_num = INSN2(11, 8);
    int opcode_2 = INSN2(7, 5);
    int crm = INSN2(3, 0);

    /* only read of TPIDRPRW is possible */
    assert(opcode_1 == 0 && crn == 13 && cp_num == 15 && opcode_2 == 3 && crm == 0);

    write_reg(context, ir, rd,
                           ir->add_read_context_32(ir, offsetof(struct arm_registers, c13_tls2)));

    return 0;
}

static int dis_t1_str_register(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN(8, 6);
    int rn = INSN(5, 3);
    int rt = INSN(2, 0);

    ir->add_store_32(ir, read_reg(context, ir, rt),
                         ir->add_add_32(ir,
                                        read_reg(context, ir, rn),
                                        read_reg(context, ir, rm)));

    return 0;
}

static int dis_t1_ldr_register(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN(8, 6);
    int rn = INSN(5, 3);
    int rt = INSN(2, 0);

    write_reg(context, ir, rt, ir->add_load_32(ir,
                                               ir->add_add_32(ir,
                                                              read_reg(context, ir, rn),
                                                              read_reg(context, ir, rm))));

    return 0;
}

static int dis_t2_bfc(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN2(11, 8);
    int msb = INSN2(4, 0);
    int lsb = (INSN2(14, 12) << 2) + INSN2(7, 6);
    int width = msb - lsb + 1;
    int mask = ~(((1 << width) - 1) << lsb);

    assert(rd != 15);

    write_reg(context, ir, rd, ir->add_and_32(ir,
                                              read_reg(context, ir, rd),
                                              mk_32(ir, mask)));

    return 0;
}

static int dis_t2_bfi(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
}

static int dis_t2_ubfx(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int widthm1 = INSN2(4, 0);
    int rd = INSN2(11, 8);
    int lsb = (INSN2(14, 12) << 2) | INSN2(7, 6);
    int rn = INSN1(3, 0);
    uint32_t mask = (1 << (widthm1 + 1)) - 1;

    write_reg(context, ir, rd, ir->add_and_32(ir,
                                              ir->add_shr_32(ir, read_reg(context, ir, rn), mk_8(ir, lsb)),
                                              mk_32(ir, mask)));

    return 0;
}

static int dis_t1_dmb(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
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

static int dis_t2_ldrex(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rn = INSN1(3, 0);
    int rt = INSN2(15, 12);
    int imm32 = INSN2(7, 0) << 2;
    struct irRegister *params[4];
    struct irRegister *result;

    assert(rn != 15);
    assert(rt != 15);

    params[0] = ir->add_add_32(ir, read_reg(context, ir, rn), mk_32(ir, imm32));
    params[1] = mk_32(ir, 4);
    params[2] = NULL;
    params[3] = NULL;

    result = ir->add_call_32(ir, "arm_hlp_ldrex",
                             mk_64(ir, (uint64_t) arm_hlp_ldrex),
                             params);

    write_reg(context, ir, rt, result);

    return 0;
}

static int dis_t2_strex(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rn = INSN1(3, 0);
    int rt = INSN2(15, 12);
    int rd = INSN2(11, 8);
    int imm32 = INSN2(7, 0) << 2;
    struct irRegister *params[4];
    struct irRegister *result;

    assert(rn != 15);
    assert(rd != 15);
    assert(rt != 15);

    params[0] = ir->add_add_32(ir, read_reg(context, ir, rn), mk_32(ir, imm32));
    params[1] = mk_32(ir, 4);
    params[2] = read_reg(context, ir, rt);
    params[3] = NULL;

    result = ir->add_call_32(ir, "arm_hlp_strex",
                             mk_64(ir, (uint64_t) arm_hlp_strex),
                             params);

    write_reg(context, ir, rd, result);

    return 0;
}

static int dis_t2_umull(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rdhi = INSN2(11, 8);
    int rdlo = INSN2(15, 12);
    int rn = INSN1(3, 0);
    int rm = INSN2(3, 0);
    struct irRegister *rm_reg = read_reg(context, ir, rm);
    struct irRegister *rn_reg = read_reg(context, ir, rn);
    struct irRegister *mul_lsb;
    struct irRegister *mul_msb;

    mul_lsb = mk_mul_u_lsb(context, ir, rn_reg, rm_reg);
    mul_msb = mk_mul_u_msb(context, ir, rn_reg, rm_reg);

    write_reg(context, ir, rdhi, mul_msb);
    write_reg(context, ir, rdlo, mul_lsb);

    return 0;
}

 /* pure disassembler */
static int dis_t1_shift_A_add_A_substract_A_move_A_compare(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int opcode = INSN(13, 9);
    int isExit = 0;

    switch(opcode) {
        case 0 ... 3:
            isExit = dis_t1_lsl_immediate(context, insn, ir);
            break;
        case 4 ... 7:
            isExit = dis_t1_lsr_immediate(context, insn, ir);
            break;
        case 8 ... 11:
            isExit = dis_t1_asr_immediate(context, insn, ir);
            break;
        case 12:
            isExit = dis_t1_add_register(context, insn, ir);
            break;
        case 13:
            isExit = dis_t1_sub_register(context, insn, ir);
            break;
        case 14:
            isExit = dis_t1_add_3_bits_immediate(context, insn, ir);
            break;
        case 15:
            isExit = dis_t1_sub_3_bits_immediate(context, insn, ir);
            break;
        case 16 ... 19:
            isExit = dis_t1_mov_immediate(context, insn, ir);
            break;
        case 20 ... 23:
            isExit = dis_t1_cmp_immediate(context, insn, ir);
            break;
        case 24 ... 27:
            isExit = dis_t1_add_8_bits_immediate(context, insn, ir);
            break;
        case 28 ... 31:
            isExit = dis_t1_sub_8_bits_immediate(context, insn, ir);
            break;
        default:
            fatal("insn = 0x%x | opcode = %d(0x%x)\n", insn, opcode, opcode);
    }

    return isExit;
}

static int dis_t1_misc_16_bits(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    uint32_t opcode = INSN(11, 5);
    int isExit = 0;

    switch(opcode) {
        case 0 ... 3:
            isExit = dis_t1_sp_plus_immediate(context, insn, ir);
            break;
        case 4 ... 7:
            isExit = dis_t1_sp_minus_immediate(context, insn, ir);
            break;
        case 16 ... 17:
            isExit = dis_t1_sxth(context, insn, ir);
            break;
        case 22 ... 23:
            isExit = dis_t1_uxtb(context, insn, ir);
            break;
        case 8 ... 15:
        case 24 ... 31:
        case 72 ... 79:
        case 88 ... 95:
            isExit = dis_t1_cbxz(context, insn, ir);
            break;
        case 32 ... 47:
            isExit = dis_t1_push(context, insn, ir);
            break;
        case 80 ... 81:
            isExit = dis_t1_rev(context, insn, ir);
            break;
        case 96 ... 111:
            isExit = dis_t1_pop(context, insn, ir);
            break;
        case 120 ... 127:
            isExit = dis_t1_itt_A_hints(context, insn, ir);
            break;
        default:
            fatal("insn = %x | opcode = %d(0x%x)\n", insn, opcode, opcode);
    }

    return isExit;
}

static int dis_t1_load_store_single_data_item(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int opa = INSN(15, 12);
    int opb = INSN(11, 9);
    int isExit = 0;

    if (opa == 0x5) {
        switch(opb) {
            case 0:
                isExit = dis_t1_str_register(context, insn, ir);
                break;
            case 4:
                isExit = dis_t1_ldr_register(context, insn, ir);
                break;
            default:
                fatal("opb = %d\n", opb);
        }
    } else if (opa == 0x6) {
        isExit = dis_t1_load_store_word_immediate(context, insn, ir);
    } else if (opa == 0x7) {
        isExit = dis_t1_load_store_byte_immediate(context, insn, ir);
    } else if (opa == 0x9) {
        isExit = dis_t1_load_store_word_immediate_sp_relative(context, insn, ir);
    } else {
        fatal("opa = %d\n", opa);
    }

    return isExit;
}

static int dis_t1_special_data_A_branch_and_exchange(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int opcode = INSN(9, 6);
    int isExit = 0;

    switch(opcode) {
        case 0:
            isExit = dis_t1_add_register_t2(context, insn, ir);
            break;
        case 1 ... 3:
            isExit = dis_t1_add_register_t2(context, insn, ir);
            break;
        case 5 ... 7:
            isExit = dis_t1_cmp_register_t2(context, insn, ir);
            break;
        case 8:
            isExit = dis_t1_mov_high_low_registers(context, insn, ir);
            break;
        case 9 ... 11:
            isExit = dis_t1_mov_high_low_registers(context, insn, ir);
            break;
        case 12 ... 13:
            isExit = dis_t1_bx(context, insn, ir);
            break;
        case 14 ... 15:
            isExit = dis_t1_blx(context, insn, ir);
            break;
        default:
            fatal("opcode = %d\n", opcode);
    }

    return isExit;
}

static int dis_t1_cond_branch_A_svc(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int opcode = INSN(11, 8);
    int isExit = 0;

    if (opcode == 15) {
        isExit = dis_t1_svc(context, insn, ir);
    } else if (opcode == 14){
        //undefined
        assert(0);
    } else {
        isExit = dis_t1_b_t1(context, insn, ir);
    }

    return isExit;
}

static int disassemble_thumb1_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int inIt = inItBlock(context);
    uint32_t opcode = INSN(15, 10);
    int isExit = 0;

    if (inIt) {
        uint32_t cond = (context->disa_itstate >> 4);
        struct irRegister *params[4];
        struct irRegister *pred;

        params[0] = mk_32(ir, cond);
        params[1] = read_cpsr(context, ir);
        params[2] = NULL;
        params[3] = NULL;

        pred = ir->add_call_32(ir, "arm_hlp_compute_flags_pred",
                               mk_64(ir, (uint64_t) arm_hlp_compute_flags_pred),
                               params);
        write_itstate(context, ir, mk_32(ir, nextItState(context)));
        write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc + 2));
        ir->add_exit_cond(ir, ir->add_mov_const_64(ir, context->pc + 2), pred);
        //Following code is useless except when dumping state
#if DUMP_STATE
        write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc));
#endif
    }
    switch(opcode) {
        case 0 ... 15:
            isExit = dis_t1_shift_A_add_A_substract_A_move_A_compare(context, insn, ir);
            break;
        case 16:
            isExit = dis_t1_data_processing(context, insn, ir);
            break;
        case 17:
            isExit = dis_t1_special_data_A_branch_and_exchange(context, insn, ir);
            break;
        case 18 ... 19:
            isExit = dis_t1_load_literal(context, insn, ir);
            break;
        case 20 ... 39:
            isExit = dis_t1_load_store_single_data_item(context, insn, ir);
            break;
        case 42 ... 43:
            isExit = dis_t1_sp_relative_addr(context, insn, ir);
            break;
        case 44 ... 47:
            isExit = dis_t1_misc_16_bits(context, insn, ir);
            break;
        case 52 ... 55:
            isExit = dis_t1_cond_branch_A_svc(context, insn, ir);
            break;
        case 56 ... 57:
            isExit = dis_t1_b_t2(context, insn, ir);
            break;
        default:
            fatal("insn = %x | opcode = %d(0x%x)\n", insn, opcode, opcode);
    }

    return isExit;
}

static int dis_t2_misc_control_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op = INSN2(7, 4);

    switch(op) {
        case 5://dmb
            isExit = dis_t1_dmb(context, insn, ir);
            break;
        default:
            fatal("op = %d(0x%x)\n", op, op);
    }

    return isExit;
}

static int dis_t2_branches_A_misc(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int op1 = INSN2(14, 12);
    int op = INSN1(10, 4);
    int op2 = INSN2(11, 8);
    int isExit = 0;

    if (op1 == 5 || op1 == 7) {
        isExit = dis_t2_branch_with_link(context, insn, ir);
    } else if (op1 == 0 || op1 == 2) {
        if ((op & 0x38) == 0x38) {
            switch(op) {
                case 59:
                    isExit = dis_t2_misc_control_insn(context, insn, ir);
                    break;
                default:
                    fatal("op = %d(0x%x)\n", op, op);
            }
        } else
            isExit = dis_t2_b_t3(context, insn, ir);
    } else if (op1 == 4 || op1 == 6) {
        isExit = dis_t2_branch_with_link_exchange(context, insn, ir);
    } else if (op1 == 1 || op1 == 3) {
        isExit = dis_t2_branch_t4(context, insn, ir);
    } else {
        fatal("op1 = %d(0x%x)\n", op1, op1);
    }

    return isExit;
}

static int dis_t2_data_processing_plain_binary_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int op = INSN1(8, 4);
    int rn = INSN1(3, 0);
    int isExit = 0;

    switch(op) {
        case 4:
            isExit = dis_t2_movw(context, insn, ir);
            break;
        case 12:
            isExit = dis_t2_movt(context, insn, ir);
            break;
        case 22:
            if (rn == 15)
                isExit = dis_t2_bfc(context, insn, ir);
            else
                isExit = dis_t2_bfi(context, insn, ir);
            break;
        case 28:
            isExit = dis_t2_ubfx(context, insn, ir);
            break;
        default:
            fatal("op = %d(0x%x)\n", op, op);
    }

    return isExit;
}

static int dis_t2_data_processing_modified_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int s = INSN1(4, 4);
    uint32_t imm12 = (INSN1(10, 10) << 11) | (INSN2(14, 12) << 8) | INSN2(7, 0);
    uint32_t imm32 = ThumbExpandimm_C_imm32(imm12);
    struct irRegister *op;

    op = mk_32(ir, imm32);
    if (s) {
        if (imm32 >> 10) {
             write_sco(context, ir, mk_32(ir, (imm32 >> 2) & 0x20000000));
        } else {
             write_sco(context, ir, ir->add_and_32(ir,
                                                   read_cpsr(context, ir),
                                                   mk_32(ir, 0x20000000)));
        }
    }

    return mk_data_processing_modified(context, ir, insn, op);
}

static int dis_t2_data_processing_shifted_register(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int s = INSN1(4, 4);
    int shift_imm = (INSN2(14, 12) << 2) + INSN2(7, 6);
    int shift_mode = INSN2(5, 4);
    int rm = INSN2(3, 0);
    struct irRegister *op;
    struct irRegister *rm_reg;

    rm_reg = read_reg(context, ir, rm);

    if (s) {
        write_sco(context, ir, mk_sco_t2(context, ir,
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

    return mk_data_processing_modified(context, ir, insn, op);
}

static int dis_t2_ldr(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rn = INSN1(3, 0);
    int op1 = INSN1(8, 7);
    int op2 = INSN2(11, 6);
    int isExit = 0;

    if (rn == 15) {
        isExit = dis_t2_ldr_literal(context, insn, ir);
    } else if (op1 == 1) {
        isExit = dis_t2_ldr_immediate_t3(context, insn, ir);
    } else if(op2 == 0) {
        isExit = dis_t2_ldr_register(context, insn, ir);
    } else if ((op2 & 0x3c) == 0x38) {
        isExit = dis_t2_ldrt(context, insn, ir);
    } else {
        isExit = dis_t2_ldr_immediate_t4(context, insn, ir);
    }

    return isExit;
}

static int dis_t2_ldr_byte_A_hints(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op1 = INSN1(8, 7);
    int op2 = INSN2(11, 6);
    int rn = INSN1(3, 0);
    int rt = INSN2(15, 12);

    if (rt == 15) {
        //memory hint, we do nothing
    } else {
        if (rn == 15) {
            assert(0);
        } else {
            switch(op1) {
                case 0:
                    if (op2 == 0) {
                        assert(0 && "ldrb_register");
                    } else if ((op2 & 0x3c) == 0x38) {
                        assert(0 && "ldrbt");
                    } else {
                        isExit = dis_t2_ldrb_t3(context, insn, ir);
                    }
                    break;
                case 1:
                    if (rn == 15)
                        assert(0 && "ldrb literal");
                    else
                        isExit = dis_t2_ldrb_t2(context, insn, ir);
                    break;
                default:
                    fatal("op1 = %d / op2 = %d(0x%x)\n", op1, op2, op2);
            }
        }
    }

    return isExit;
}

static int dis_t2_ldr_halfword_A_unallocated_hints(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op1 = INSN1(8, 7);
    int op2 = INSN2(11, 6);
    int rn = INSN1(3, 0);
    int rt = INSN2(15, 12);

    if (rt == 15) {
        assert(0);
    } else {
        if (rn == 15) {
            assert(0);
        } else {
            switch(op1) {
                case 1:
                    isExit = dis_t2_ldrh_t2(context, insn, ir);
                    break;
                default:
                    fatal("op1 = %d / op2 = %d(0x%x) / rn = %d\n", op1, op2, op2, rn);
            }
        }
    }

    return isExit;
}

static int dis_t2_ldrd_strd_A_ldrex_strex_A_table_branch(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op1 = INSN1(8, 7);
    int op2 = INSN1(5, 4);
    int op3 = INSN2(7, 4);
    int rn = INSN1(3, 0);

    if (op1 == 1 && (op2 & 2) ==0) {
        switch(op3) {
            case 0 ... 1:
                isExit = dis_t2_tbx(context, insn, ir);
                break;
            default:
                fatal("op3 = 0x%x(%d)\n", op3, op3);
        }
    } else if (op1 == 0 && op2 == 0) {
        isExit = dis_t2_strex(context, insn, ir);
    } else if (op1 == 0 && op2 == 1) {
        isExit = dis_t2_ldrex(context, insn, ir);
    } else if (((op1 & 2) == 0 && op2 == 2) || ((op1 & 2) == 2 && (op2 & 1) == 0)) {
        isExit = dis_t2_load_store_double_immediate_offset(context, insn, ir);
    } else {
        if (rn == 15) {
            //ldrd literral
            assert(0);
        } else {
            isExit = dis_t2_load_store_double_immediate_offset(context, insn, ir);
        }
    }

    return isExit;
}

static int dis_t2_signed_parallel(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
}

static int dis_t2_unsigned_parallel(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "thumb_hlp_t2_unsigned_parallel",
                           ir->add_mov_const_64(ir, (uint64_t) thumb_hlp_t2_unsigned_parallel),
                           params);

    return 0;
}

static int dis_t2_misc(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op1 = INSN1(5, 4);
    int op2 = INSN2(5, 4);
    int op = (op1 << 2) | op2;

    switch(op) {
        case 8:
            isExit = dis_t2_sel(context, insn, ir);
            break;
        case 12:
            isExit = dis_t2_clz(context, insn, ir);
            break;
        default:
            fatal("op = %d\n", op);
    }

    return isExit;
}

static int dis_t2_data_processing_register(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op1 = INSN1(7, 4);
    int op2 = INSN2(7, 4);

    if (op1 & 0x8) {
        if ((op2 & 0xc) == 0) {
            isExit = dis_t2_signed_parallel(context, insn, ir);
        } else if ((op2 & 0xc) == 4) {
            isExit = dis_t2_unsigned_parallel(context, insn, ir);
        } else if ((op2 & 0xc) == 8) {
            isExit = dis_t2_misc(context, insn, ir);
        } else {
            assert(0);
        }
    } else if (op2 == 0) {
        isExit = dis_t2_data_processing_register_shift(context, insn, ir);
        //sign extend
    } else {
        assert(0);
    }

    return isExit;
}

static int dis_t2_str_single_data(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op1 = INSN1(7, 5);
    int op2 = INSN2(11, 6);

    if (op1 & 4) {
        isExit = dis_t2_str_imm12(context, insn, ir);
    } else if (op2 & 0x20) {
        if ((op2 & 0x3c) == 0x38) {
            assert(0);
            //isExit = dis_t2_str_unprivileged(context, insn, ir);
        } else {
            isExit = dis_t2_str_imm8(context, insn, ir);
        }
    } else {
        assert(0);
        //isExit = dis_t2_str_register(context, insn, ir);
    }

    return isExit;
}

static int dis_t2_load_store_multiple(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op = INSN1(8, 7);
    int l = INSN1(4, 4);

    switch(op) {
        case 0:
        case 3:
            //srs/rfe
            assert(0);
            break;
        case 1 ... 2:
            if (l)
                isExit = dis_t2_load_multiple(context, insn, ir);
            else
                isExit = dis_t2_store_multiple(context, insn, ir);
            break;
    }

    return isExit;
}

static int dis_t2_mult_A_mult_acc_A_abs_diff(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op1 = INSN1(6, 4);
    int op2 = INSN1(5, 4);
    int ra = INSN1(15, 12);

    switch(op1) {
        case 0:
            if (op2) {
                assert(0 && "mls");
            } else if (ra == 15) {
                isExit = dis_t2_mul(context, insn, ir);
            } else {
                assert(0 && "mla");
            }
            break;
        default:
            fatal("op1 = %d(0x%x)\n", op1, op1);
    }

    return isExit;
}

static int dis_t2_long_mult_A_long_mult_acc_A_div(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op1 = INSN1(6, 4);
    int op2 = INSN2(7, 4);

    switch(op1) {
        case 2:
            isExit = dis_t2_umull(context, insn, ir);
            break;
        default:
            fatal("op1 = %d\n", op1);
    }

    return isExit;
}

static int dis_t2_coprocessor_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op1 = INSN1(9, 4);
    int op = INSN2(4, 4);
    int coproc = INSN1(11, 8);
    int rn = INSN2(3, 0);

    if ((op1 & 0x30) == 0x20) {
        if (op) {
            if ((coproc & 0xe) == 0xa) {
                assert(0);
            } else {
                if (op1 & 1) {
                    isExit = dis_t2_mrc(context, insn, ir);
                } else {
                    assert(0);
                }
            }
        } else {
            assert(0);
        }
    } else {
        fatal("op1 = 0x%x\n", op1);
    }

    return isExit;
}

static int disassemble_thumb2_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int inIt = inItBlock(context);
    int op1 = INSN1(12, 11);
    int op2 = INSN1(10, 4);
    int op = INSN2(15, 15);
    int isExit = 0;

    if (inIt) {
        uint32_t cond = (context->disa_itstate >> 4);
        struct irRegister *params[4];
        struct irRegister *pred;

        params[0] = mk_32(ir, cond);
        params[1] = read_cpsr(context, ir);
        params[2] = NULL;
        params[3] = NULL;

        pred = ir->add_call_32(ir, "arm_hlp_compute_flags_pred",
                               mk_64(ir, (uint64_t) arm_hlp_compute_flags_pred),
                               params);
        write_itstate(context, ir, mk_32(ir, nextItState(context)));
        write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc + 4));
        ir->add_exit_cond(ir, ir->add_mov_const_64(ir, context->pc + 4), pred);
        //Following code is useless except when dumping state
#if DUMP_STATE
        write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc));
#endif
    }
    switch(op1) {
        case 1:
            if (op2 & 0x40) {
                isExit = dis_t2_coprocessor_insn(context, insn, ir);
            } else if (op2 & 0x60) {
                //data processing shift reg
                isExit = dis_t2_data_processing_shifted_register(context, insn, ir);
            } else if (op2 & 0x64) {
                //load/store double or exclusive or table branch
                isExit = dis_t2_ldrd_strd_A_ldrex_strex_A_table_branch(context, insn, ir);
            } else {
                isExit = dis_t2_load_store_multiple(context, insn, ir);
            }
            break;
        case 2:
            if (op)
                isExit = dis_t2_branches_A_misc(context, insn, ir);
            else {
                if (INSN1(9,9))
                    isExit = dis_t2_data_processing_plain_binary_immediate(context, insn, ir);
                else
                    isExit = dis_t2_data_processing_modified_immediate(context, insn, ir);
            }
            break;
        case 3:
            if (op2 & 0x40) {
                isExit = dis_t2_coprocessor_insn(context, insn, ir);
            } else if ((op2 & 0x70) == 0x20) {
                isExit = dis_t2_data_processing_register(context, insn, ir);
            } else if ((op2 & 0x78) == 0x30) {
                isExit = dis_t2_mult_A_mult_acc_A_abs_diff(context, insn, ir);
            } else if ((op2 & 0x78) == 0x38) {
                isExit = dis_t2_long_mult_A_long_mult_acc_A_div(context, insn, ir);
            } else if ((op2 & 0x67) == 0x01) {
                isExit = dis_t2_ldr_byte_A_hints(context, insn, ir);
            } else if ((op2 & 0x67) == 0x03) {
                isExit = dis_t2_ldr_halfword_A_unallocated_hints(context, insn, ir);
            } else if ((op2 & 0x67) == 0x05) {
                isExit = dis_t2_ldr(context, insn, ir);
            } else if ((op2 & 0x67) == 0x07) {
                //dis_t2_undefined
                assert(0);
            } else if ((op2 & 0x71) == 0x00) {
                isExit = dis_t2_str_single_data(context, insn, ir);
            } else if ((op2 & 0x71) == 0x10) {
                //dis_t2_adv_simd_O_ldr_str_structure
                assert(0);
            } else
                assert(0);
            break;
        default:
            fatal("Unvalid value\n");
    }

    return isExit;
}

/* api */
void disassemble_thumb(struct target *target, struct irInstructionAllocator *ir, uint64_t pc, int maxInsn)
{
    struct arm_target *context = container_of(target, struct arm_target, target);
    int i;
    int isExit; //unconditionnal exit
    uint16_t *pc_ptr = (uint16_t *) (pc & ~1);

    assert((pc & 1) == 1);
    context->disa_itstate = context->regs.reg_itstate;
    for(i = 0; i < maxInsn; i++) {
        int inIt = inItBlock(context);

        context->pc = (uint32_t) (uint64_t)pc_ptr + 1;
        if ((*pc_ptr >> 11) == 0x1d || (*pc_ptr >> 11) == 0x1e || (*pc_ptr >> 11) == 0x1f) {
            //fprintf(stderr, "0x%lx => insn = 0x%04x%04x\n", pc, *pc_ptr, *(pc_ptr+1));
            isExit = disassemble_thumb2_insn(context, (*pc_ptr++ << 16) | (*pc_ptr++), ir);
        } else {
            //fprintf(stderr, "0x%lx => insn = 0x%04x\n", pc, *pc_ptr);
            isExit = disassemble_thumb1_insn(context, *pc_ptr++, ir);
        }
        dump_state(context, ir);
        if (inIt)
            itAdvance(context);
        if (isExit)
            break;
    }
    if (!isExit) {
        context->pc = (uint32_t) (uint64_t)pc_ptr + 1;
        write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc));
        ir->add_exit(ir, ir->add_mov_const_64(ir, context->pc));
    }
}
