#include <stdio.h>
#include <assert.h>
#include <stddef.h>

#include "arm_private.h"
#include "arm_helpers.h"

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

void write_reg(struct arm_target *context, struct irInstructionAllocator *ir, int index, struct irRegister *value)
{
    ir->add_write_context_32(ir, value, offsetof(struct arm_registers, r[index]));
}

static struct irRegister *read_cpsr(struct arm_target *context, struct irInstructionAllocator *ir)
{
    return ir->add_read_context_32(ir, offsetof(struct arm_registers, cpsr));
}

void write_cpsr(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *value)
{
    ir->add_write_context_32(ir, value, offsetof(struct arm_registers, cpsr));
}

static struct irRegister *read_sco(struct arm_target *context, struct irInstructionAllocator *ir)
{
    return ir->add_read_context_32(ir, offsetof(struct arm_registers, shifter_carry_out));
}

void write_sco(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *value)
{
    ir->add_write_context_32(ir, value, offsetof(struct arm_registers, shifter_carry_out));
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
    struct irRegister *nextCpsr;
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

/* disassembler to ir */
 /**/

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
    dump_state(context, ir);
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
    struct irRegister *rn_initial_value;
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
                                          ir->add_add_32(ir, start_address, mk_32(ir, offset))));
                offset += 4;
            }
        }
        if ((reglist >> 15) & 1) {
            //pc will be update => block exit
            newPc = ir->add_load_32(ir, ir->add_add_32(ir, start_address, mk_32(ir, offset)));

            write_reg(context, ir, 15, newPc);
            isExit = 1;
        }
    } else {
        for(i=0;i<16;i++) {
            if ((reglist >> i) & 1) {
                ir->add_store_32(ir, read_reg(context, ir, i),
                                 ir->add_add_32(ir, start_address, mk_32(ir, offset)));
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
            write_reg(context, ir, rd, ir->add_8U_to_32(ir, ir->add_load_8(ir, address)));
        else
            write_reg(context, ir, rd, ir->add_load_32(ir, address));
    } else {
        //store
        if (b)
            ir->add_store_8(ir, ir->add_32_to_8(ir, read_reg(context, ir, rd)), address);
        else
            ir->add_store_32(ir, read_reg(context, ir, rd), address);
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
            assert(0);
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
            write_reg(context, ir, rd, ir->add_8U_to_32(ir, ir->add_load_8(ir, address)));
        else
            write_reg(context, ir, rd, ir->add_load_32(ir, address));
    } else {
        //store
        if (b)
            ir->add_store_8(ir, ir->add_32_to_8(ir, read_reg(context, ir, rd)), address);
        else
            ir->add_store_32(ir, read_reg(context, ir, rd), address);
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
    write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc));
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

 /* pure disassembler */
static int dis_msr_imm_and_hints(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
}

static int dis_sync_primitive(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
}

static int dis_mult_and_mult_acc_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
}

static int dis_extra_load_store_unprivilege_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
}

static int dis_extra_load_store_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
}

static int dis_halfword_mult_and_mult_acc_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
}

static int dis_misc_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    uint32_t op = INSN(22,21);
    uint32_t op1 = INSN(19,16);
    uint32_t op2 = INSN(6,4);
    int isExit = 0;

    switch(op2) {
        case 1:
            if (op == 1)
                isExit = dis_bx(context, insn, ir);
            else
                assert(0);
            break;
        default:
            assert(0);
    }

    return isExit;
}

static int dis_data_processing_register_shifted_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
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
    assert(0);
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
        dis_swi(context, insn, ir);
    } else {
        assert(0);
    }

    return isExit;
}

static int disassemble_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    uint32_t cond = INSN(31, 28);
    int isExit = 0;

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
    }

    /* now really disassemble instruction */
    if (cond == 15) {
        assert(0 && "dis_unconditional_insn");
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
    int isExit; //unconditionnal exit
    uint32_t *pc_ptr = (uint32_t *) pc;

    assert((pc & 3) == 0);
    for(i = 0; i < 1/*maxInsn*/; i++) {
        context->pc = (uint32_t) (uint64_t)pc_ptr;
        if (context->pc == 0xffff0fc0) {
            isExit = vdso_cmpxchg(context, ir);
        } else if (context->pc == 0xffff0fa0) {
            isExit = vdso_memory_barrier(context, ir);
        } else if (context->pc == 0xffff0fe0) {
            isExit = vdso_get_tls(context, ir);
        } else {
            isExit = disassemble_insn(context, *pc_ptr++, ir);
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
