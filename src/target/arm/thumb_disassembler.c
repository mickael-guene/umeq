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
    /* FIXME: implement it block stuff */
    return 0;
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

/* disassembler to ir */
 /**/

 /* code generators */
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

static int dis_t1_sp_minus_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    uint32_t imm32 = INSN(6, 0) << 2;

    write_reg(context, ir, 13, ir->add_sub_32(ir, read_reg(context, ir, 13), mk_32(ir, imm32)));

    return 0;
}

static int dis_t1_mov_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(10, 8);
    uint32_t imm32 = INSN(7, 0);
    int s = !inItBlock(context);
    struct irRegister *nextCpsr;

    if (s) {
        struct irRegister *params[4];

        params[0] = mk_32(ir, 13);
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

    if (s) {
        struct irRegister *params[4];

        params[0] = mk_32(ir, 10);
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

static int dis_t1_add_8_bits_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rdn = INSN(10, 8);
    uint32_t imm32 = INSN(7, 0);
    int s = !inItBlock(context);
    struct irRegister *nextCpsr;

    if (s) {
        struct irRegister *params[4];

        params[0] = mk_32(ir, 4);
        params[1] = read_reg(context, ir, rdn);
        params[2] = mk_32(ir, imm32);
        params[3] = read_cpsr(context, ir);

        nextCpsr = ir->add_call_32(ir, "thumb_hlp_compute_next_flags",
                                   ir->add_mov_const_64(ir, (uint64_t) thumb_hlp_compute_next_flags),
                                   params);
    }

    write_reg(context, ir, rdn, ir->add_add_32(ir, read_reg(context, ir, rdn), mk_32(ir, imm32)));

    if (s) {
        write_cpsr(context, ir, nextCpsr);
    }

    return 0;
}

static int dis_t1_sub_8_bits_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rdn = INSN(10, 8);
    uint32_t imm32 = INSN(7, 0);
    int s = !inItBlock(context);
    struct irRegister *nextCpsr;

    if (s) {
        struct irRegister *params[4];

        params[0] = mk_32(ir, 2);
        params[1] = read_reg(context, ir, rdn);
        params[2] = mk_32(ir, imm32);
        params[3] = read_cpsr(context, ir);

        nextCpsr = ir->add_call_32(ir, "thumb_hlp_compute_next_flags",
                                   ir->add_mov_const_64(ir, (uint64_t) thumb_hlp_compute_next_flags),
                                   params);
    }

    write_reg(context, ir, rdn, ir->add_sub_32(ir, read_reg(context, ir, rdn), mk_32(ir, imm32)));

    if (s) {
        write_cpsr(context, ir, nextCpsr);
    }

    return 0;
}

static int dis_t1_load_store_word_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int l = INSN(11, 11);
    int rt = INSN(2, 0);
    int rn = INSN(5, 3);
    uint32_t imm32 = INSN(10, 6) << 2;
    struct irRegister *address;

    address = ir->add_add_32(ir, read_reg(context, ir, rn), mk_32(ir, imm32));
    if (l) {
        write_reg(context, ir, rt, ir->add_load_32(ir, address));
    } else {
        ir->add_store_32(ir, read_reg(context, ir, rt), address);
    }

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
    int32_t imm32 = ((i1 << 23) | (i2 << 22) | (INSN1(9, 0) << 12) | (INSN2(10, 0) << 1));
    uint32_t branch_target;

    /* sign extend */
    imm32 = (imm32 << 8) >> 8;
    branch_target = context->pc + 4 + imm32;
    write_reg(context, ir, 14, ir->add_mov_const_32(ir, context->pc + 4));
    dump_state(context, ir);
    write_reg(context, ir, 15, ir->add_mov_const_32(ir, branch_target));
    ir->add_exit(ir, ir->add_mov_const_64(ir, branch_target));

    return 1;
}

static int dis_t2_ldr_literal(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
}

static int dis_t2_ldr_immediate_t3(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
}

static int dis_t2_ldr_register(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
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

static int dis_t2_mov_t2(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN2(11, 8);
    int s = INSN1(4, 4);
    uint32_t imm12 = (INSN1(10, 10) << 11) | (INSN2(14, 12) << 8) | INSN2(7, 0);
    uint32_t imm32 = ThumbExpandimm_C_imm32(imm12);

    /* FIXME: implement flag update but take care of thumbexpandimm_c which update carry */
    assert(s == 0);

    write_reg(context, ir, rd, mk_32(ir, imm32));

    return 0;
}

 /* pure disassembler */
static int dis_t1_shift_A_add_A_substract_A_move_A_compare(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int opcode = INSN(13, 9);
    int isExit = 0;

    switch(opcode) {
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
        case 4 ... 7:
            isExit = dis_t1_sp_minus_immediate(context, insn, ir);
            break;
        case 32 ... 47:
            isExit = dis_t1_push(context, insn, ir);
            break;
        case 96 ... 111:
            isExit = dis_t1_pop(context, insn, ir);
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
            default:
                fatal("opb = %d\n", opb);
        }
    } else if (opa == 0x6) {
        isExit = dis_t1_load_store_word_immediate(context, insn, ir);
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
        case 8:
            isExit = dis_t1_mov_high_low_registers(context, insn, ir);
            break;
        case 9 ... 11:
            isExit = dis_t1_mov_high_low_registers(context, insn, ir);
            break;
        case 12 ... 13:
            isExit = dis_t1_bx(context, insn, ir);
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
    uint32_t opcode = INSN(15, 10);
    int isExit = 0;

    switch(opcode) {
        case 0 ... 15:
            isExit = dis_t1_shift_A_add_A_substract_A_move_A_compare(context, insn, ir);
            break;
        case 17:
            isExit = dis_t1_special_data_A_branch_and_exchange(context, insn, ir);
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

static int dis_t2_branches_A_misc(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int op1 = INSN2(14, 12);
    int op = INSN1(10, 4);
    int op2 = INSN2(11, 8);
    int isExit = 0;

    switch(op1) {
        case 5: case 7:
            isExit = dis_t2_branch_with_link(context, insn, ir);
            break;
        default:
            assert(0);
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
        default:
            assert(0);
    }

    return isExit;
}

static int dis_t2_data_processing_modified_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int op = INSN1(8, 5);
    int rn = INSN1(3, 0);
    int isExit = 0;

    switch(op) {
        case 2:
            if (rn == 15)
                dis_t2_mov_t2(context, insn, ir);
            else
                assert(0);
            break;
        default:
            fatal("insn=0x%x op=%d(0x%x)\n", insn, op, op);
    }


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

static int disassemble_thumb2_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int op1 = INSN1(12, 11);
    int op2 = INSN1(10, 4);
    int op = INSN2(15, 15);
    int isExit = 0;

    switch(op1) {
        case 1:
            assert(0);
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
                //dis_t2_coprocessor_insn
                assert(0);
            } else if ((op2 & 0x70) == 0x20) {
                //dis_t2_data_processing_register
                assert(0);
            } else if ((op2 & 0x78) == 0x30) {
                //dis_t2_mult_A_mult_acc_A_abs_diff
                assert(0);
            } else if ((op2 & 0x78) == 0x38) {
                //dis_t2_long_mult_A_long_mult_acc_A_div
                assert(0);
            } else if ((op2 & 0x67) == 0x01) {
                //dis_t2_ldr_byte_A_hints
                assert(0);
            } else if ((op2 & 0x67) == 0x03) {
                //dis_t2_ldr_halfword_A_unallocated_hints
                assert(0);
            } else if ((op2 & 0x67) == 0x05) {
                isExit = dis_t2_ldr(context, insn, ir);
            } else if ((op2 & 0x67) == 0x07) {
                //dis_t2_undefined
                assert(0);
            } else if ((op2 & 0x71) == 0x00) {
                //dis_t2_str_single_data
                assert(0);
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
    for(i = 0; i < maxInsn; i++) {
        context->pc = (uint32_t) (uint64_t)pc_ptr + 1;
        if ((*pc_ptr >> 11) == 0x1d || (*pc_ptr >> 11) == 0x1e || (*pc_ptr >> 11) == 0x1f) {
            isExit = disassemble_thumb2_insn(context, (*pc_ptr++ << 16) | (*pc_ptr++), ir);
        } else {
            isExit = disassemble_thumb1_insn(context, *pc_ptr++, ir);
        }
        dump_state(context, ir);
        if (isExit)
            break;
    }
    if (!isExit) {
        context->pc = (uint32_t) (uint64_t)pc_ptr + 1;
        write_reg(context, ir, 15, ir->add_mov_const_32(ir, context->pc));
        ir->add_exit(ir, ir->add_mov_const_64(ir, context->pc));
    }
}
