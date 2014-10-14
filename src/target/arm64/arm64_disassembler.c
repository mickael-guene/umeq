#include <assert.h>

#include "target64.h"
#include "arm64_private.h"
#include "runtime.h"
#include "arm64_helpers.h"

#define ZERO_REG    1
#define SP_REG      0

#define DUMP_STATE  1
#define INSN(msb, lsb) ((insn >> (lsb)) & ((1 << ((msb) - (lsb) + 1))-1))

/* sequence shortcut */
static void dump_state(struct arm64_target *context, struct irInstructionAllocator *ir)
{
#if DUMP_STATE
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};

    ir->add_call_void(ir, "arm64_hlp_dump",
                      ir->add_mov_const_64(ir, (uint64_t) arm64_hlp_dump),
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

static struct irRegister *read_x(struct irInstructionAllocator *ir, int index, int is_zero)
{
    if (index == 31 && is_zero)
        return mk_64(ir, 0);
    else
        return ir->add_read_context_64(ir, offsetof(struct arm64_registers, r[index]));
}

static void write_x(struct irInstructionAllocator *ir, int index, struct irRegister *value, int is_zero)
{
    if (index == 31 && is_zero)
        ;//do nothing
    else
        ir->add_write_context_64(ir, value, offsetof(struct arm64_registers, r[index]));
}

static struct irRegister *read_w(struct irInstructionAllocator *ir, int index, int is_zero)
{
    if (index == 31 && is_zero)
        return mk_32(ir, 0);
    else
        return ir->add_read_context_32(ir, offsetof(struct arm64_registers, r[index]));
}

static void write_w(struct irInstructionAllocator *ir, int index, struct irRegister *value, int is_zero)
{
    if (index == 31 && is_zero)
        ;//do nothing
    else
        ir->add_write_context_64(ir, ir->add_32U_to_64(ir, value), offsetof(struct arm64_registers, r[index]));
}

static void write_w_sign_extend(struct irInstructionAllocator *ir, int index, struct irRegister *value, int is_zero)
{
    if (index == 31 && is_zero)
        ;//do nothing
    else
        ir->add_write_context_64(ir, ir->add_32S_to_64(ir, value), offsetof(struct arm64_registers, r[index]));
}

static void write_pc(struct irInstructionAllocator *ir,struct irRegister *value)
{
    ir->add_write_context_64(ir, value, offsetof(struct arm64_registers, pc));
}

static struct irRegister *read_nzcv(struct irInstructionAllocator *ir)
{
    return ir->add_read_context_32(ir, offsetof(struct arm64_registers, nzcv));
}

static void write_nzcv(struct irInstructionAllocator *ir, struct irRegister *value)
{
    ir->add_write_context_32(ir, value, offsetof(struct arm64_registers, nzcv));
}

static struct irRegister *mk_ror_imm_64(struct irInstructionAllocator *ir, struct irRegister *op, int rotation)
{
    return ir->add_or_64(ir,
                         ir->add_shr_64(ir, op, mk_8(ir, rotation)),
                         ir->add_shl_64(ir, op, mk_8(ir, 64-rotation)));
}

static struct irRegister *mk_ror_imm_32(struct irInstructionAllocator *ir, struct irRegister *op, int rotation)
{
    return ir->add_or_64(ir,
                         ir->add_shr_32(ir, op, mk_8(ir, rotation)),
                         ir->add_shl_32(ir, op, mk_8(ir, 32-rotation)));
}

static struct irRegister *mk_next_nzcv_64(struct irInstructionAllocator *ir, enum ops ops, struct irRegister *op1, struct irRegister *op2)
{
        struct irRegister *params[4];
        struct irRegister *nextCpsr;

        params[0] = mk_32(ir, ops);
        params[1] = op1;
        params[2] = op2;
        params[3] = read_nzcv(ir);

        nextCpsr = ir->add_call_32(ir, "arm64_hlp_compute_next_nzcv_64",
                                   ir->add_mov_const_64(ir, (uint64_t) arm64_hlp_compute_next_nzcv_64),
                                   params);

        return nextCpsr;
}

static struct irRegister *mk_next_nzcv_32(struct irInstructionAllocator *ir, enum ops ops, struct irRegister *op1, struct irRegister *op2)
{
        struct irRegister *params[4];
        struct irRegister *nextCpsr;

        params[0] = mk_32(ir, ops);
        params[1] = op1;
        params[2] = op2;
        params[3] = read_nzcv(ir);

        nextCpsr = ir->add_call_32(ir, "arm64_hlp_compute_next_nzcv_32",
                                   ir->add_mov_const_64(ir, (uint64_t) arm64_hlp_compute_next_nzcv_32),
                                   params);

        return nextCpsr;
}

/* FIXME: not optimal */
static struct irRegister *mk_extend_reg_64(struct irInstructionAllocator *ir, int rm, int type, int shift)
{
    struct irRegister *unshifted;
    struct irRegister *res;

    /* do the extraction */
    switch(type) {
        case 0:
            unshifted = ir->add_and_64(ir, read_x(ir, rm, ZERO_REG), mk_64(ir, 0xffUL));
            break;
        case 1:
            unshifted = ir->add_and_64(ir, read_x(ir, rm, ZERO_REG), mk_64(ir, 0xffffUL));
            break;
        case 2:
            unshifted = ir->add_and_64(ir, read_x(ir, rm, ZERO_REG), mk_64(ir, 0xffffffffUL));
            break;
        case 3:
            unshifted = read_x(ir, rm, ZERO_REG);
            break;
        case 4:
            unshifted = ir->add_asr_64(ir,
                                       ir->add_shl_64(ir, read_x(ir, rm, ZERO_REG), mk_8(ir, 56)),
                                       mk_8(ir, 56));
            break;
        case 5:
            unshifted = ir->add_asr_64(ir,
                                       ir->add_shl_64(ir, read_x(ir, rm, ZERO_REG), mk_8(ir, 48)),
                                       mk_8(ir, 48));
            break;
        case 6:
            unshifted = ir->add_asr_64(ir,
                                       ir->add_shl_64(ir, read_x(ir, rm, ZERO_REG), mk_8(ir, 32)),
                                       mk_8(ir, 32));
            break;
        case 7:
            unshifted = read_x(ir, rm, ZERO_REG);
            break;
    }
    /* do the shift */
    if (shift)
        res = ir->add_shl_64(ir, unshifted, mk_8(ir, shift));
    else
        res = unshifted;

    return res;
}

/* FIXME: not optimal */
static struct irRegister *mk_extend_reg_32(struct irInstructionAllocator *ir, int rm, int type, int shift)
{
    struct irRegister *unshifted;
    struct irRegister *res;

    /* do the extraction */
    switch(type) {
        case 0:
            unshifted = ir->add_and_32(ir, read_w(ir, rm, ZERO_REG), mk_32(ir, 0xff));
            break;
        case 1:
            unshifted = ir->add_and_32(ir, read_w(ir, rm, ZERO_REG), mk_32(ir, 0xffff));
            break;
        case 2:
            unshifted = read_w(ir, rm, ZERO_REG);
            break;
        case 3:
            unshifted = read_w(ir, rm, ZERO_REG);
            break;
        case 4:
            unshifted = ir->add_asr_32(ir,
                                       ir->add_shl_32(ir, read_w(ir, rm, ZERO_REG), mk_8(ir, 24)),
                                       mk_8(ir, 24));
            break;
        case 5:
            unshifted = ir->add_asr_32(ir,
                                       ir->add_shl_32(ir, read_w(ir, rm, ZERO_REG), mk_8(ir, 16)),
                                       mk_8(ir, 16));
            break;
        case 6:
            unshifted = read_w(ir, rm, ZERO_REG);
            break;
        case 7:
            unshifted = read_w(ir, rm, ZERO_REG);
            break;
    }
    /* do the shift */
    if (shift)
        res = ir->add_shl_32(ir, unshifted, mk_8(ir, shift));
    else
        res = unshifted;

    return res;
}

static struct irRegister *mk_mul_u_lsb_64(struct irInstructionAllocator *ir, struct irRegister *op1, struct irRegister *op2)
{
    struct irRegister *param[4] = {op1, op2, NULL, NULL};

    return ir->add_call_64(ir, "arm64_hlp_umul_lsb_64",
                           ir->add_mov_const_64(ir, (uint64_t) arm64_hlp_umul_lsb_64),
                           param);
}

static struct irRegister *mk_mul_u_lsb_32(struct irInstructionAllocator *ir, struct irRegister *op1, struct irRegister *op2)
{
    struct irRegister *param[4] = {op1, op2, NULL, NULL};

    return ir->add_call_32(ir, "arm64_hlp_umul_lsb_32",
                           ir->add_mov_const_64(ir, (uint64_t) arm64_hlp_umul_lsb_32),
                           param);
}

/* op code generation */
static int dis_load_store_pair(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31, 31);
    int is_signed = INSN(30, 30);
    int is_load = INSN(22, 22);
    int64_t imm7 = INSN(21, 15);
    int rt2 = INSN(14, 10);
    int rn = INSN(9, 5);
    int rt = INSN(4, 0);
    struct irRegister *address1;
    struct irRegister *address2;
    int64_t offset;
    int is_wb = INSN(23,23);
    int is_not_postindex = INSN(24,24);

    /* compute address */
    offset = ((imm7 << 57) >> 57);
    offset = offset << (2 + is_64);
    if (is_not_postindex)
        address1 = ir->add_add_64(ir, read_x(ir, rn, SP_REG), mk_64(ir, offset));
    else
        address1 = read_x(ir, rn, SP_REG);
    address2 = ir->add_add_64(ir, address1, mk_64(ir, is_64?8:4));

    /* read write reg */
    if (is_load) {
        if (is_64) {
            write_x(ir, rt, ir->add_load_64(ir, address1), ZERO_REG);
            write_x(ir, rt2, ir->add_load_64(ir, address2), ZERO_REG);
        } else {
            if (is_signed) {
                write_w_sign_extend(ir, rt, ir->add_load_32(ir, address1), ZERO_REG);
                write_w_sign_extend(ir, rt2, ir->add_load_32(ir, address2), ZERO_REG);
            } else {
                write_w(ir, rt, ir->add_load_32(ir, address1), ZERO_REG);
                write_w(ir, rt2, ir->add_load_32(ir, address2), ZERO_REG);
            }
        }
    } else {
        if (is_64) {
            ir->add_store_64(ir, read_x(ir, rt, ZERO_REG), address1);
            ir->add_store_64(ir, read_x(ir, rt2, ZERO_REG), address2);
        } else {
            ir->add_store_32(ir, read_w(ir, rt, ZERO_REG), address1);
            ir->add_store_32(ir, read_w(ir, rt2, ZERO_REG), address2);
        }
    }

    /* write back */
    if (is_wb) {
        if (is_not_postindex)
            write_x(ir, rn, address1, SP_REG);
        else
            write_x(ir, rn, ir->add_add_64(ir, address1, mk_64(ir, offset)), SP_REG);
    }

    return 0;
}

static int dis_load_store_pair_pre_indexed(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    return dis_load_store_pair(context, insn, ir);
}

static int dis_load_store_pair_offset(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    return dis_load_store_pair(context, insn, ir);
}

static int dis_load_store_pair_post_indexed(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    return dis_load_store_pair(context, insn, ir);
}

static int dis_add_sub(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir,
                       struct irRegister *op1, struct irRegister *op2, int is_rd_zero_reg)
{
    int is_64 = INSN(31, 31);
    int is_sub = INSN(30, 30);
    int S = INSN(29, 29);
    int rd = INSN(4, 0);
    struct irRegister *res;

    /* do the ops */
    if (is_sub) {
        if (is_64)
            res = ir->add_sub_64(ir, op1, op2);
        else
            res = ir->add_sub_32(ir, op1, op2);
    } else {
        if (is_64)
            res = ir->add_add_64(ir, op1, op2);
        else
            res = ir->add_add_32(ir, op1, op2);
    }
    /* update flags */
    if (S) {
        if (is_64)
            write_nzcv(ir, mk_next_nzcv_64(ir, is_sub?OPS_SUB:OPS_ADD, op1, op2));
        else
            write_nzcv(ir, mk_next_nzcv_32(ir, is_sub?OPS_SUB:OPS_ADD, op1, op2));
    }

    /* write reg */
    if (is_64)
        write_x(ir, rd, res, S?ZERO_REG:is_rd_zero_reg);
    else
        write_w(ir, rd, res, S?ZERO_REG:is_rd_zero_reg);

    return 0;
}

static int dis_add_sub_immediate_insn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31, 31);
    uint32_t shift = INSN(23, 22);
    int imm12 = INSN(21, 10);
    int rn = INSN(9, 5);
    struct irRegister *op1;
    struct irRegister *op2;

    assert(shift != 2 && shift != 3);
    if (shift == 1)
        imm12 = imm12 << 12;

    /* prepare ops */
    if (is_64) {
        op1 = read_x(ir, rn, SP_REG);
        op2 = mk_64(ir, imm12);
    } else {
        op1 = read_w(ir, rn, SP_REG);
        op2 = mk_32(ir, imm12);
    }

    return dis_add_sub(context, insn, ir, op1, op2, SP_REG);
}

static int dis_add_sub_shifted_register_insn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31, 31);
    uint32_t shift = INSN(23, 22);
    int imm6 = INSN(15, 10);
    int rn = INSN(9, 5);
    int rm = INSN(20, 16);
    struct irRegister *op1;
    struct irRegister *op2;

    /* prepare ops */
    if (is_64) {
        op1 = read_x(ir, rn, ZERO_REG);
        switch(shift) {
            case 0://lsl
                op2 = ir->add_shl_64(ir, read_x(ir, rm, ZERO_REG), mk_8(ir, imm6));
                break;
            case 1:
                op2 = ir->add_shr_64(ir, read_x(ir, rm, ZERO_REG), mk_8(ir, imm6));
                break;
            case 2:
                op2 = ir->add_asr_64(ir, read_x(ir, rm, ZERO_REG), mk_8(ir, imm6));
                break;
            default:
                assert(0);
        }
    } else {
        op1 = read_w(ir, rn, ZERO_REG);
        switch(shift) {
            case 0://lsl
                op2 = ir->add_shl_32(ir, read_w(ir, rm, ZERO_REG), mk_8(ir, imm6));
                break;
            case 1:
                op2 = ir->add_shr_32(ir, read_w(ir, rm, ZERO_REG), mk_8(ir, imm6));
                break;
            case 2:
                op2 = ir->add_asr_32(ir, read_w(ir, rm, ZERO_REG), mk_8(ir, imm6));
                break;
            default:
                assert(0);
        }
    }

    return dis_add_sub(context, insn, ir, op1, op2, ZERO_REG);
}

static int dis_add_sub_extended_register_insn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31, 31);
    int rn = INSN(9, 5);
    int rm = INSN(20, 16);
    int option = INSN(15,13);
    int imm3 = INSN(12,10);
    struct irRegister *op1;
    struct irRegister *op2;

    /* prepare ops */
    if (is_64) {
        op1 = read_x(ir, rn, SP_REG);
        op2 = mk_extend_reg_64(ir, rm, option, imm3);
    } else {
        op1 = read_w(ir, rn, SP_REG);
        op2 = mk_extend_reg_32(ir, rm, option, imm3);
    }

    return dis_add_sub(context, insn, ir, op1, op2, SP_REG);
}

static int dis_adrp(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int64_t imm = (INSN(23, 5) << 14) + (INSN(30, 29) << 12);
    int rd = INSN(4, 0);

    imm = (imm << 31) >> 31;

    write_x(ir, rd, mk_64(ir, (context->pc & ~0xfffUL) + imm), ZERO_REG);

    return 0;
}

static int dis_adr(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int64_t imm = (INSN(23, 5) << 2) + INSN(30, 29);
    int rd = INSN(4, 0);

    imm = (imm << 43) >> 43;

    write_x(ir, rd, mk_64(ir, context->pc + imm), ZERO_REG);

    return 0;
}

static int dis_movn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    uint64_t imm16 = INSN(20,5);
    int rd = INSN(4,0);
    int pos = INSN(22,21) << 4;

    imm16 = ~(imm16 << pos);
    if (is_64)
        write_x(ir, rd, mk_64(ir, imm16), ZERO_REG);
    else
        write_w(ir, rd, mk_32(ir, imm16), ZERO_REG);

    return 0;
}

static int dis_movz(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    uint64_t imm16 = INSN(20,5);
    int rd = INSN(4,0);
    int pos = INSN(22,21) << 4;

    imm16 = imm16 << pos;
    if (is_64)
        write_x(ir, rd, mk_64(ir, imm16), ZERO_REG);
    else
        write_w(ir, rd, mk_32(ir, imm16), ZERO_REG);

    return 0;
}

static int dis_movk(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    uint64_t imm16 = INSN(20,5);
    int rd = INSN(4,0);
    int pos = INSN(22,21) << 4;
    uint64_t mask = 0xffffUL << pos;
    struct irRegister *res;

    imm16 = imm16 << pos;

    res = ir->add_or_64(ir,
                        ir->add_and_64(ir,
                                       read_x(ir, rd, ZERO_REG),
                                       mk_64(ir, ~mask)),
                        mk_64(ir, imm16));

    if (is_64)
        write_x(ir, rd, res, ZERO_REG);
    else
        write_w(ir, rd, ir->add_64_to_32(ir, res), ZERO_REG);

    return 0;
}

static int dis_logical_shifted_register(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int opc = INSN(30,29);
    int n = INSN(21,21);
    int shift = INSN(23,22);
    int imm6 = INSN(15,10);
    int rm = INSN(20,16);
    int rn = INSN(9,5);
    int rd = INSN(4,0);
    struct irRegister *res;
    struct irRegister *op2;
    struct irRegister *res_not_inverted;

    /* compute op2 */
    switch(shift) {
        case 0://LSL
            op2 = ir->add_shl_64(ir, read_x(ir, rm, ZERO_REG), mk_8(ir, imm6));
            break;
        case 1://LSR
            op2 = ir->add_shr_64(ir, read_x(ir, rm, ZERO_REG), mk_8(ir, imm6));
            break;
        case 2://ASR
            if (is_64)
                op2 = ir->add_asr_64(ir, read_x(ir, rm, ZERO_REG), mk_8(ir, imm6));
            else
                op2 = ir->add_32U_to_64(ir, ir->add_asr_32(ir, read_w(ir, rm, ZERO_REG), mk_8(ir, imm6)));
            break;
        case 3://ROR
            if (is_64)
                op2 = mk_ror_imm_64(ir, read_x(ir, rm, ZERO_REG), imm6);
            else
                op2 = ir->add_32U_to_64(ir, mk_ror_imm_32(ir, read_x(ir, rm, ZERO_REG), imm6));
            break;
    }

    /* do the op */
    switch(opc) {
        case 0://and
            res_not_inverted = ir->add_and_64(ir, read_x(ir, rn, ZERO_REG), op2);
            break;
        case 1://orr
            res_not_inverted = ir->add_or_64(ir, read_x(ir, rn, ZERO_REG), op2);
            break;
        case 2://eor
            res_not_inverted = ir->add_xor_64(ir, read_x(ir, rn, ZERO_REG), op2);
            break;
        default:
            fatal("opc = %d\n", opc);
    }
    /* invert */
    if (n)
        res = ir->add_xor_64(ir, res_not_inverted, mk_64(ir, ~0UL));
    else
        res = res_not_inverted;

    /* write res */
    if (is_64)
        write_x(ir, rd, res, ZERO_REG);
    else
        write_w(ir, rd, ir->add_64_to_32(ir, res), ZERO_REG);


    return 0;
}

static int dis_b_A_bl(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_l = INSN(31,31);
    int64_t imm26 = INSN(25, 0) << 2;

    imm26 = (imm26 << 36) >> 36;

    if (is_l)
        write_x(ir, 30, mk_64(ir, context->pc + 4), ZERO_REG);
    dump_state(context, ir);
    write_pc(ir, mk_64(ir, context->pc + imm26));
    ir->add_exit(ir, mk_64(ir, context->pc + imm26));

    return 1;
}

static int dis_svc(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *param[4] = {NULL, NULL, NULL, NULL};

    /* be sure pc as the correct value so clone syscall can use pc value */
    write_pc(ir, ir->add_mov_const_64(ir, context->pc + 4));
    ir->add_call_void(ir, "arm64_hlp_syscall",
                      ir->add_mov_const_64(ir, (uint64_t) arm64_hlp_syscall),
                      param);
#ifdef DUMP_STATE
    write_pc(ir, ir->add_mov_const_64(ir, context->pc));
    dump_state(context, ir);
    write_pc(ir, ir->add_mov_const_64(ir, context->pc + 4));
#endif
    ir->add_exit(ir, ir->add_mov_const_64(ir, context->pc + 4));

    return 1;
}

static int dis_br(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rn = INSN(9, 5);
    struct irRegister *ret;

    dump_state(context, ir);
    ret = read_x(ir, rn, ZERO_REG);
    write_pc(ir, ret);
    ir->add_exit(ir, ret);

    return 1;
}

static int dis_blr(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rn = INSN(9, 5);
    struct irRegister *ret;

    ret = read_x(ir, rn, ZERO_REG);
    write_x(ir, 30, mk_64(ir, context->pc + 4), ZERO_REG);
    dump_state(context, ir);
    write_pc(ir, ret);
    ir->add_exit(ir, ret);

    return 1;
}

static int dis_ret(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rn = INSN(9, 5);
    struct irRegister *ret;

    dump_state(context, ir);
    ret = read_x(ir, rn, ZERO_REG);
    write_pc(ir, ret);
    ir->add_exit(ir, ret);

    return 1;
}

static int dis_conditionnal_branch_immediate_insn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int cond = INSN(3,0);
    int64_t imm19 = INSN(23, 5) << 2;
    struct irRegister *params[4];
    struct irRegister *pred;

    imm19 = (imm19 << 43) >> 43;


    params[0] = mk_32(ir, cond);
    params[1] = read_nzcv(ir);
    params[2] = NULL;
    params[3] = NULL;

    pred = ir->add_call_32(ir, "arm64_hlp_compute_flags_pred",
                           mk_64(ir, (uint64_t) arm64_hlp_compute_flags_pred),
                           params);
    dump_state(context, ir);
    /* if cond is true then do the branch */
    write_pc(ir, mk_64(ir, context->pc + imm19));
    ir->add_exit_cond(ir, mk_64(ir, context->pc + imm19), pred);
    /* else we jump to next insn */
    write_pc(ir, mk_64(ir, context->pc + 4));
    ir->add_exit(ir, mk_64(ir, context->pc + 4));

    return 1;
}

static int dis_load_register_literal(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int opc = INSN(31,30);
    int64_t imm19 = INSN(23,5) << 2;
    int rt = INSN(4,0);
    struct irRegister *address;

    /* early exit for prefetch */
    if (opc == 3)
        return 0;

    imm19 = (imm19 << 43) >> 43;
    address = mk_64(ir, context->pc + imm19);

    switch(opc) {
        case 0:
            write_w(ir, rt, ir->add_load_32(ir, address), ZERO_REG);
            break;
        case 1:
            write_x(ir, rt, ir->add_load_64(ir, address), ZERO_REG);
            break;
        case 2:
            write_w_sign_extend(ir, rt, ir->add_load_32(ir, address), ZERO_REG);
            break;
        default:
            assert(0);
    }

    return 0;
}

static int dis_cbz_A_cbnz(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int64_t imm19 = INSN(23,5) << 2;
    int rt = INSN(4,0);
    struct irRegister *pred;
    int is_cmp_not_zero = INSN(24,24);

    imm19 = (imm19 << 43) >> 43;
    /* compute pred */
    if (is_cmp_not_zero) {
        if (is_64)
            pred = ir->add_cmpne_64(ir, read_x(ir, rt, ZERO_REG), mk_64(ir, 0));
        else
            pred = ir->add_cmpne_32(ir, read_w(ir, rt, ZERO_REG), mk_32(ir, 0));
    } else {
        if (is_64)
            pred = ir->add_cmpeq_64(ir, read_x(ir, rt, ZERO_REG), mk_64(ir, 0));
        else
            pred = ir->add_cmpeq_32(ir, read_w(ir, rt, ZERO_REG), mk_32(ir, 0));
    }

    /* branch to destination if pred */
    write_pc(ir, mk_64(ir, context->pc + imm19));
    ir->add_exit_cond(ir, mk_64(ir, context->pc + imm19), pred);
    /* if pred is false jump to next */
    write_pc(ir, mk_64(ir, context->pc + 4));
    ir->add_exit(ir, mk_64(ir, context->pc + 4));

    return 1;
}

static int dis_cbnz(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    return dis_cbz_A_cbnz(context, insn, ir);
}

static int dis_cbz(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    return dis_cbz_A_cbnz(context, insn, ir);
}

static int dis_cond_select(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir, struct irRegister *op2)
{
    int is_64 = INSN(31,31);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int cond = INSN(15,12);
    struct irRegister *params[4];
    struct irRegister *pred;
    struct irRegister *res;

    params[0] = mk_32(ir, cond);
    params[1] = read_nzcv(ir);
    params[2] = NULL;
    params[3] = NULL;
    pred = ir->add_call_32(ir, "arm64_hlp_compute_flags_pred",
                           mk_64(ir, (uint64_t) arm64_hlp_compute_flags_pred),
                           params);

    /* compute 64 bits result */
    res = ir->add_ite_64(ir, ir->add_32U_to_64(ir, pred), //is jitter assertion on pred type is util ?
                             read_x(ir, rn, ZERO_REG),
                             op2);

    /* write reg */
    if (is_64)
        write_x(ir, rd, res, ZERO_REG);
    else
        write_w(ir, rd, ir->add_64_to_32(ir, res), ZERO_REG);

    return 0;
}

static int dis_csneg(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
    return 0;
}

static int dis_csinv(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
    return 0;
}

static int dis_csinc(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN(20,16);

    return dis_cond_select(context, insn, ir,
                           ir->add_add_64(ir, read_x(ir, rm, ZERO_REG), mk_64(ir, 1)));
}

static int dis_csel(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN(20,16);

    return dis_cond_select(context, insn, ir, read_x(ir, rm, ZERO_REG));
}

static void dis_load_store_register(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir,
                                    int size, int opc, int rt, struct irRegister *address)
{
    struct irRegister *res = NULL;

    if (opc == 0) {
        //store
        switch(size) {
            case 0:
                ir->add_store_8(ir, ir->add_64_to_8(ir, read_x(ir, rt, ZERO_REG)), address);
                break;
            case 1:
                ir->add_store_16(ir, ir->add_64_to_16(ir, read_x(ir, rt, ZERO_REG)), address);
                break;
            case 2:
                ir->add_store_32(ir, read_w(ir, rt, ZERO_REG), address);
                break;
            case 3:
                ir->add_store_64(ir, read_x(ir, rt, ZERO_REG), address);
                break;
        }
    } else if (opc == 1) {
        //load
        switch(size) {
            case 0:
                res = ir->add_8U_to_64(ir, ir->add_load_8(ir, address));
                break;
            case 1:
                res = ir->add_16U_to_64(ir, ir->add_load_16(ir, address));
                break;
            case 2:
                res = ir->add_32U_to_64(ir, ir->add_load_32(ir, address));
                break;
            case 3:
                res = ir->add_load_64(ir, address);
                break;
        }
        write_x(ir, rt, res, ZERO_REG);
    } else if (opc == 2) {
        //load signed extend to 64 bits
        switch(size) {
            case 0:
                res = ir->add_8S_to_64(ir, ir->add_load_8(ir, address));
                break;
            case 1:
                res = ir->add_16S_to_64(ir, ir->add_load_16(ir, address));
                break;
            case 2:
                res = ir->add_32S_to_64(ir, ir->add_load_32(ir, address));
                break;
            default:
                assert(0);
        }
        write_x(ir, rt, res, ZERO_REG);
    } else {
        //load signed extend to 32 bits
        switch(size) {
            case 0:
                res = ir->add_8S_to_32(ir, ir->add_load_8(ir, address));
                break;
            case 1:
                res = ir->add_16S_to_32(ir, ir->add_load_16(ir, address));
                break;
            default:
                assert(0);
        }
        write_w(ir, rt, res, ZERO_REG);
    }
}

static int dis_load_store_register_unsigned_offset(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int size = INSN(31,30);
    int opc = INSN(23,22);
    int imm12 = INSN(21, 10);
    int rn = INSN(9, 5);
    int rt = INSN(4, 0);
    struct irRegister *address;

    /* prfm is translated into nop */
    if (size == 3 && opc == 2)
        return 0;

    /* compute address */
    address = ir->add_add_64(ir, read_x(ir, rn, SP_REG), mk_64(ir, imm12 << size));

    /* do load store */
    dis_load_store_register(context, insn, ir, size, opc, rt, address);

    return 0;
}

static int dis_load_store_register_unsigned_offset_simd(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
}

static int dis_load_store_register_immediate_post_indexed(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int size = INSN(31,30);
    int opc = INSN(23,22);
    int64_t imm9 = INSN(20, 12);
    int rn = INSN(9, 5);
    int rt = INSN(4, 0);
    struct irRegister *address;

    imm9 = (imm9 << 55) >> 55;

    /* compute address */
    address = read_x(ir, rn, SP_REG);

    /* do load store */
    dis_load_store_register(context, insn, ir, size, opc, rt, address);

    /* write back rn */
    write_x(ir, rn, ir->add_add_64(ir, address, mk_64(ir, imm9)), SP_REG);

    return 0;
}

static int dis_load_store_register_immediate_post_indexed_simd(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
}

static int dis_load_store_register_register_offset(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int size = INSN(31,30);
    int opc = INSN(23,22);
    int rn = INSN(9, 5);
    int rt = INSN(4, 0);
    int rm = INSN(20,16);
    int option = INSN(15,13);
    int S = INSN(12,12);
    struct irRegister *offset_not_scale;
    struct irRegister *offset;
    struct irRegister *address;

    /* prfm is translated into nop */
    if (size == 3 && opc == 2)
        return 0;

    /* compute address */
    if (option == 3 || option == 7) {
        offset_not_scale = read_x(ir, rm, ZERO_REG);
    } else if (option == 2) {
        offset_not_scale = ir->add_32U_to_64(ir, read_w(ir, rm, ZERO_REG));
    } else if (option == 6) {
        offset_not_scale = ir->add_32S_to_64(ir, read_w(ir, rm, ZERO_REG));
    } else
        assert(0);
    if (S && size) {
        offset = ir->add_shl_64(ir, offset_not_scale, mk_8(ir, size));
    } else {
        offset = offset_not_scale;
    }

    address = ir->add_add_64(ir, read_x(ir, rn, SP_REG), offset);

    /* do load store */
    dis_load_store_register(context, insn, ir, size, opc, rt, address);

    return 0;
}

static int dis_load_store_register_register_offset_simd(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
}

static int dis_load_store_register_immediate_pre_indexed(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int size = INSN(31,30);
    int opc = INSN(23,22);
    int64_t imm9 = INSN(20, 12);
    int rn = INSN(9, 5);
    int rt = INSN(4, 0);
    struct irRegister *address;

    imm9 = (imm9 << 55) >> 55;

    /* compute address */
    address = ir->add_add_64(ir, read_x(ir, rn, SP_REG), mk_64(ir, imm9));

    /* do load store */
    dis_load_store_register(context, insn, ir, size, opc, rt, address);

    /* write back rn */
    write_x(ir, rn, address, SP_REG);

    return 0;
}

static int dis_load_store_register_immediate_pre_indexed_simd(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
}

static int dis_load_register_literal_simd(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
}

static uint64_t rotateRight(uint64_t value, int rotate, int width)
{
    rotate &= 63;

    return ((value & ((1UL << rotate) - 1UL)) << (width - rotate)) | (value >> rotate);
}

static int dis_logical_immediate(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int opc = INSN(30,29);
    int rn = INSN(9,5);
    int rd = INSN(4,0);
    int n = INSN(22,22);
    int immr = INSN(21,16);
    int imms = INSN(15,10);
    uint64_t immediate = 0;
    struct irRegister *res;
    int is_setflags = 0;

    /* compute immediate ... */
    if (n == 1) {
        immediate = (1UL << (imms + 1)) - 1;
        immediate = rotateRight(immediate, immr, 64);
    } else {
        unsigned int width;
        for (width = 0x20; width >= 2; width >>= 1) {
            if ((imms & width) == 0) {
                unsigned int i;
                unsigned int mask = width - 1;
                uint64_t pattern = (1UL << ((imms & mask) + 1)) - 1;

                pattern = rotateRight(pattern, immr & mask, width);
                immediate = pattern & ((1UL << width) - 1UL);
                for (i = width; i < (is_64?64:32); i *= 2) {
                    immediate |= (immediate << i);
                }
                break;
            }
        }
    }

    /* do the ops */
    switch(opc) {
        case 3:
            is_setflags = 1;
            //fallthrought
        case 0:
            res = ir->add_and_64(ir, read_x(ir, rn, ZERO_REG), mk_64(ir, immediate));
            break;
        case 1:
            res = ir->add_or_64(ir, read_x(ir, rn, ZERO_REG), mk_64(ir, immediate));
            break;
        case 2:
            res = ir->add_xor_64(ir, read_x(ir, rn, ZERO_REG), mk_64(ir, immediate));
            break;
    }

    if (is_setflags) {
        if (is_64)
            write_nzcv(ir, mk_next_nzcv_64(ir, OPS_LOGICAL, res, mk_64(ir, 0)));
        else
            write_nzcv(ir, mk_next_nzcv_32(ir, OPS_LOGICAL, ir->add_64_to_32(ir, res), mk_32(ir, 0)));
    }

    if (is_64)
        write_x(ir, rd, res, SP_REG);
    else
        write_w(ir, rd, ir->add_64_to_32(ir, res), SP_REG);

    return 0;
}

static int dis_bitfield_insn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    struct irRegister *params[4];
    struct irRegister *res;
    int is_64 = INSN(31,31);
    int rn = INSN(9,5);
    int rd = INSN(4,0);

    params[0] = mk_32(ir, insn);
    params[1] = read_x(ir, rn, ZERO_REG);
    params[2] = read_x(ir, rd, ZERO_REG);
    params[3] = NULL;

    res = ir->add_call_64(ir, "arm64_hlp_compute_bitfield",
                               ir->add_mov_const_64(ir, (uint64_t) arm64_hlp_compute_bitfield),
                               params);

    if (is_64)
        write_x(ir, rd, res, ZERO_REG);
    else
        write_w(ir, rd, ir->add_64_to_32(ir, res), ZERO_REG);

    return isExit;
}

static int dis_udiv(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    struct irRegister *params[4];
    struct irRegister *res;

    if (is_64) {
        params[0] = read_x(ir, rn, ZERO_REG);
        params[1] = read_x(ir, rm, ZERO_REG);
        params[2] = NULL;
        params[3] = NULL;

        res = ir->add_call_32(ir, "arm64_hlp_udiv_64",
                               ir->add_mov_const_64(ir, (uint64_t) arm64_hlp_udiv_64),
                               params);
        write_x(ir, rd, res, ZERO_REG);
    } else {
        params[0] = read_w(ir, rn, ZERO_REG);
        params[1] = read_w(ir, rm, ZERO_REG);
        params[2] = NULL;
        params[3] = NULL;

        res = ir->add_call_32(ir, "arm64_hlp_udiv_32",
                               ir->add_mov_const_64(ir, (uint64_t) arm64_hlp_udiv_32),
                               params);
        write_w(ir, rd, res, ZERO_REG);
    }

    return 0;
}

static int dis_lslv(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int rm = INSN(20,16);
    int rn = INSN(9,5);
    int rd = INSN(4,0);
    struct irRegister *res;
    int mask = is_64?0x3f:0x1f;

    res = ir->add_shl_64(ir,
                         read_x(ir, rn, ZERO_REG),
                         ir->add_64_to_8(ir,
                                         ir->add_and_64(ir,
                                                        read_x(ir, rm, ZERO_REG),
                                                        mk_64(ir, mask))));


    if (is_64)
        write_x(ir, rd, res, ZERO_REG);
    else
        write_w(ir, rd, ir->add_64_to_32(ir, res), ZERO_REG);

    return 0;
}

static int dis_tbz_A_tbnz(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int bit_pos = (INSN(31,31) << 5) | INSN(23,19);
    int64_t imm14 = INSN(18, 5) << 2;
    int rt = INSN(4,0);
    int is_tbnz = INSN(24,24);
    struct irRegister *res;
    struct irRegister *pred;

    imm14 = (imm14 << 48) >> 48;

    /* mask bit */
    res = ir->add_and_64(ir, read_x(ir, rt, ZERO_REG), mk_64(ir, 1UL << bit_pos));
    /* compare with zero */
    if (is_tbnz)
        pred = ir->add_cmpne_64(ir, res, mk_64(ir, 0));
    else
        pred = ir->add_cmpeq_64(ir, res, mk_64(ir, 0));
    /* branch to jump address if predicate hold */
    write_pc(ir, mk_64(ir, context->pc + imm14));
    ir->add_exit_cond(ir, mk_64(ir, context->pc + imm14), pred);
    /* if pred is false jump to next */
    write_pc(ir, mk_64(ir, context->pc + 4));
    ir->add_exit(ir, mk_64(ir, context->pc + 4));

    return 1;
}

static int dis_madd(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int rd = INSN(4,0);
    int ra = INSN(14,10);
    int rn = INSN(9,5);
    int rm = INSN(20,16);

    if (is_64) {
        write_x(ir, rd,
                ir->add_add_64(ir,
                               read_x(ir, ra, ZERO_REG),
                               mk_mul_u_lsb_64(ir,
                                               read_x(ir, rn, ZERO_REG),
                                               read_x(ir, rm, ZERO_REG))),
                ZERO_REG);
    } else {
        write_w(ir, rd,
                ir->add_add_32(ir,
                               read_w(ir, ra, ZERO_REG),
                               mk_mul_u_lsb_32(ir,
                                               read_w(ir, rn, ZERO_REG),
                                               read_w(ir, rm, ZERO_REG))),
                ZERO_REG);
    }

    return 0;
}

static int dis_msr_register(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int op0 = 2 + INSN(19,19);
    int op1 = INSN(18,16);
    int op2 = INSN(7,5);
    int crn = INSN(15,12);
    int crm = INSN(11,8);
    int rt = INSN(4,0);

    if (op0 == 3 && op1 == 3 && crn == 0xd && crm == 0 && op2 == 2) {
        ir->add_write_context_64(ir, read_x(ir, rt, ZERO_REG), offsetof(struct arm64_registers, tpidr_el0));
    } else {
        assert(0);
    }

    return 0;
}

static int dis_mrs_register(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int op0 = 2 + INSN(19,19);
    int op1 = INSN(18,16);
    int op2 = INSN(7,5);
    int crn = INSN(15,12);
    int crm = INSN(11,8);
    int rt = INSN(4,0);

    if (op0 == 3 && op1 == 3 && crn == 0xd && crm == 0 && op2 == 2) {
        write_x(ir, rt, ir->add_read_context_64(ir, offsetof(struct arm64_registers, tpidr_el0)), ZERO_REG);
    } else {
        assert(0);
    }

    return 0;
}

/* disassemblers */

static int dis_load_store_insn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op_29__28 = INSN(29, 28);

    switch(op_29__28) {
        case 0:
            if (INSN(28, 24) == 0x08) {
                fatal("load_store_exclusive\n");
            } else {
                fatal("advanced_simd_load_store\n");
            }
            break;
        case 1:
            if (INSN(26,26))
                isExit = dis_load_register_literal_simd(context, insn, ir);
            else
                isExit = dis_load_register_literal(context, insn, ir);
            break;
        case 2:
            {
                int op_24__23 = INSN(24, 23);

                switch(op_24__23) {
                    case 0:
                        fatal("load_store_no_allocate_pair\n");
                        break;
                    case 1:
                        if (INSN(26, 26))
                            fatal("load_store_register_pair_post_indexed_simd_vfp\n");
                        else
                            isExit = dis_load_store_pair_post_indexed(context, insn, ir);
                        break;
                    case 2:
                        if (INSN(26,26))
                            fatal("load_store_register_pair_offset_simd_vfp\n");
                        else
                            isExit = dis_load_store_pair_offset(context, insn, ir);
                        break;
                    case 3:
                        if (INSN(26, 26))
                            fatal("load_store_register_pair_pre_indexed_simd_vfp\n");
                        else
                            isExit = dis_load_store_pair_pre_indexed(context, insn, ir);
                        break; 
                }
            }
            break;
        case 3:
            if (INSN(24,24)) {
                if (INSN(26,26))
                    isExit = dis_load_store_register_unsigned_offset_simd(context, insn, ir);
                else
                    isExit = dis_load_store_register_unsigned_offset(context, insn, ir);
            } else {
                switch(INSN(11,10)) {
                    case 0:
                        fatal("load_store_register_unscaled_immedaite\n");
                        break;
                    case 1:
                        if (INSN(26,26))
                            isExit = dis_load_store_register_immediate_post_indexed_simd(context, insn, ir);
                        else
                            isExit = dis_load_store_register_immediate_post_indexed(context, insn, ir);
                        break;
                    case 2:
                        if (INSN(21,21)) {
                            if (INSN(26,26))
                                isExit = dis_load_store_register_register_offset_simd(context, insn, ir);
                            else
                                isExit = dis_load_store_register_register_offset(context, insn, ir);
                        } else {
                            fatal("load_store_register_unprivileged\n");
                        }
                        break;
                    case 3:
                        if (INSN(26,26))
                            isExit = dis_load_store_register_immediate_pre_indexed_simd(context, insn, ir);
                        else
                            isExit = dis_load_store_register_immediate_pre_indexed(context, insn, ir);
                        break;
                }
            }
            break;
    }

    return isExit;
}

static int dis_data_processing_immediate_insn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op_25__23 = INSN(25, 23);

    switch(op_25__23) {
        case 0 ... 1:
            if (INSN(31, 31))
                isExit = dis_adrp(context, insn, ir);
            else
                isExit = dis_adr(context, insn, ir);
            break;
        case 2 ... 3:
            isExit = dis_add_sub_immediate_insn(context, insn, ir);
            break;
        case 4:
            isExit = dis_logical_immediate(context, insn, ir);
            break;
        case 5:
            switch(INSN(30, 29)) {
                case 0:
                    isExit = dis_movn(context, insn, ir);
                    break;
                case 2:
                    isExit = dis_movz(context, insn, ir);
                    break;
                case 3:
                    isExit = dis_movk(context, insn, ir);
                    break;
                default:
                    assert(0);
            }
            break;
        case 6:
            isExit = dis_bitfield_insn(context, insn, ir);
            break;
        case 7:
            fatal("extract\n");
            break;
    }

    return isExit;
}

static int dis_conditional_select(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;

    int op = INSN(30,30);
    int op2 = INSN(11,10);

    if (op) {
        if (op2)
            isExit = dis_csneg(context, insn, ir);
        else
            isExit = dis_csinv(context, insn, ir);
    } else {
        if (op2)
            isExit = dis_csinc(context, insn, ir);
        else
            isExit = dis_csel(context, insn, ir);
    }

    return isExit;
}

static int dis_data_processing_2_source(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int opcode = INSN(15,10);

    switch(opcode) {
        case 2:
            isExit = dis_udiv(context, insn, ir);
            break;
        case 8:
            isExit = dis_lslv(context, insn, ir);
            break;
        default:
            fatal("opcode = %d(0x%x)\n", opcode, opcode);
    }

    return isExit;
}

static int dis_data_processing_3_source(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op31_o0 = (INSN(23,21) << 1) | INSN(15,15);

    switch(op31_o0) {
        case 0:
            isExit = dis_madd(context, insn, ir);
            break;
        default:
            fatal("op31_o0 = %d(0x%x)\n", op31_o0, op31_o0);
    }

    return isExit;
}

static int dis_data_processing_register_insn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;

    if (INSN(28,28)) {
        if (INSN(24,24)) {
            isExit = dis_data_processing_3_source(context, insn, ir);
        } else {
            switch(INSN(23,21)) {
                case 0:
                    fatal("add_sub_with_carry\n");
                    break;
                case 2:
                    if (INSN(11,11))
                        fatal("cond_compare_immediate\n");
                    else
                        fatal("cond_compare_register\n");
                    break;
                case 4:
                    isExit = dis_conditional_select(context, insn, ir);
                    break;
                case 6:
                    if (INSN(30,30))
                        fatal("data_processing_1_source\n");
                    else
                        isExit = dis_data_processing_2_source(context, insn, ir);
                    break;
                default:
                    assert(0);
            }
        }
    } else {
        if (INSN(24,24)) {
            if (INSN(21,21))
                isExit = dis_add_sub_extended_register_insn(context, insn, ir);
            else
                isExit = dis_add_sub_shifted_register_insn(context, insn, ir);
        } else {
            isExit = dis_logical_shifted_register(context, insn, ir);
        }
    }

    return isExit;
}

static int dis_exception_insn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int opc = INSN(23,21);
    int ll = INSN(1,0);

    if (opc == 0 && ll == 1)
        isExit = dis_svc(context, insn, ir);
    else
        assert(0);

    return isExit;
}

static int dis_unconditionnal_branch_register(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int opc = INSN(24,21);

    switch(opc) {
        case 0:
            isExit = dis_br(context, insn, ir);
            break;
        case 1:
            isExit = dis_blr(context, insn, ir);
            break;
        case 2:
            isExit = dis_ret(context, insn, ir);
            break;
        default:
            fatal("opc = %d\n", opc);
    }

    return isExit;
}

static int dis_system(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int l = INSN(21,21);
    int op0 = INSN(20,19);

    if (l) {
        if (op0 == 2 || op0 == 3)
            isExit = dis_mrs_register(context, insn, ir);
        else
            assert(0);
    } else {
        switch(op0) {
            case 2 ...3:
                isExit = dis_msr_register(context, insn, ir);
                break;
            default:
                fatal("op0 = %d\n", op0);
        }
    }

    return isExit;
}

static int dis_branch_A_exception_A_system_insn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;

    switch(INSN(30,29)) {
        case 0:
            isExit = dis_b_A_bl(context, insn, ir);
            break;
        case 1:
            if (INSN(25,25))
                isExit = dis_tbz_A_tbnz(context, insn, ir);
            else {
                if (INSN(24,24))
                    isExit = dis_cbnz(context, insn, ir);
                else
                    isExit = dis_cbz(context, insn, ir);
            }
            break;
        case 2:
            if (INSN(31,31)) {
                if (INSN(25,25)) {
                    isExit = dis_unconditionnal_branch_register(context, insn, ir);
                } else {
                    if (INSN(24,24))
                        isExit = dis_system(context, insn, ir);
                    else
                        isExit = dis_exception_insn(context, insn, ir);
                }
            } else {
                isExit = dis_conditionnal_branch_immediate_insn(context, insn, ir);
            }
            break;
        default:
            assert(0);
    }

    return isExit;
}

static int disassemble_insn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;

    if (INSN(28, 27) == 0) {
        //unallocated
        fatal("unallocated ins\n");
    } else if (INSN(28, 26) == 4) {
        isExit = dis_data_processing_immediate_insn(context, insn, ir);
    } else if (INSN(28, 26) == 5) {
        isExit = dis_branch_A_exception_A_system_insn(context, insn, ir);
    } else if (INSN(27,27) == 1 && INSN(25, 25) == 0) {
        isExit = dis_load_store_insn(context, insn, ir);
    } else if (INSN(27, 25) == 5) {
        isExit = dis_data_processing_register_insn(context, insn, ir);
    } else if (INSN(28, 25) == 7) {
        //data_processing_simd_and_fpu
        fatal("data_processing_simd_and_fpu\n");
    } else if (INSN(28, 25) == 15) {
        //data_processing_simd_and_fpu
        fatal("data_processing_simd_and_fpu\n");
    } else
        fatal("Unknow insn = 0x%08x\n", insn);

    return isExit;
}

/* api */
void disassemble_arm64(struct target *target, struct irInstructionAllocator *ir, uint64_t pc, int maxInsn)
{
    struct arm64_target *context = container_of(target, struct arm64_target, target);
    int i;
    int isExit; //unconditionnal exit
    uint32_t *pc_ptr = (uint32_t *) g_2_h_64(pc);

    assert((pc & 3) == 0);
    for(i = 0; i < 1/*maxInsn*/; i++) {
        context->pc = h_2_g_64(pc_ptr);
        isExit = disassemble_insn(context, *pc_ptr, ir);
        pc_ptr++;
        if (!isExit)
            dump_state(context, ir);
        if (isExit)
            break;
    }
    if (!isExit) {
        write_pc(ir, ir->add_mov_const_64(ir, context->pc + 4));
        ir->add_exit(ir, ir->add_mov_const_64(ir, context->pc + 4));
    }
}

