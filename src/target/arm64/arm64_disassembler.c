#include <assert.h>

#include "target64.h"
#include "arm64_private.h"
#include "runtime.h"
#include "arm64_helpers.h"
#include "arm64_helpers_simd.h"
#include "breakpoints.h"

#define ZERO_REG    1
#define SP_REG      0

//#define DUMP_STATE  1
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

static void write_v_lsb(struct irInstructionAllocator *ir, int index, struct irRegister *value)
{
    ir->add_write_context_64(ir, value, offsetof(struct arm64_registers, v[index].v.lsb));
}

static void write_v_msb(struct irInstructionAllocator *ir, int index, struct irRegister *value)
{
    ir->add_write_context_64(ir, value, offsetof(struct arm64_registers, v[index].v.msb));
}

static struct irRegister *read_v_lsb(struct irInstructionAllocator *ir, int index)
{
    return ir->add_read_context_64(ir, offsetof(struct arm64_registers, v[index].v.lsb));
}

static struct irRegister *read_v_msb(struct irInstructionAllocator *ir, int index)
{
    return ir->add_read_context_64(ir, offsetof(struct arm64_registers, v[index].v.msb));
}

static void write_d(struct irInstructionAllocator *ir, int index, struct irRegister *value)
{
    ir->add_write_context_64(ir, value, offsetof(struct arm64_registers, v[index].d[0]));
}

static struct irRegister *read_d(struct irInstructionAllocator *ir, int index)
{
    return ir->add_read_context_64(ir, offsetof(struct arm64_registers, v[index].d[0]));
}

static void write_s(struct irInstructionAllocator *ir, int index, struct irRegister *value)
{
    ir->add_write_context_64(ir, ir->add_32U_to_64(ir, value), offsetof(struct arm64_registers, v[index].d[0]));
}

static struct irRegister *read_s(struct irInstructionAllocator *ir, int index)
{
    return ir->add_read_context_32(ir, offsetof(struct arm64_registers, v[index].s[0]));
}

static void write_h(struct irInstructionAllocator *ir, int index, struct irRegister *value)
{
    ir->add_write_context_64(ir, ir->add_16U_to_64(ir, value), offsetof(struct arm64_registers, v[index].d[0]));
}

static struct irRegister *read_h(struct irInstructionAllocator *ir, int index)
{
    return ir->add_read_context_16(ir, offsetof(struct arm64_registers, v[index].h[0]));
}

static void write_b(struct irInstructionAllocator *ir, int index, struct irRegister *value)
{
    ir->add_write_context_64(ir, ir->add_8U_to_64(ir, value), offsetof(struct arm64_registers, v[index].d[0]));
}

static struct irRegister *read_b(struct irInstructionAllocator *ir, int index)
{
    return ir->add_read_context_8(ir, offsetof(struct arm64_registers, v[index].b[0]));
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
    return ir->add_or_32(ir,
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

static struct irRegister *mk_mul_u_msb_64(struct irInstructionAllocator *ir, struct irRegister *op1, struct irRegister *op2)
{
    struct irRegister *param[4] = {op1, op2, NULL, NULL};

    return ir->add_call_64(ir, "arm64_hlp_umul_msb_64",
                           ir->add_mov_const_64(ir, (uint64_t) arm64_hlp_umul_msb_64),
                           param);
}

static struct irRegister *mk_mul_s_msb_64(struct irInstructionAllocator *ir, struct irRegister *op1, struct irRegister *op2)
{
    struct irRegister *param[4] = {op1, op2, NULL, NULL};

    return ir->add_call_64(ir, "arm64_hlp_smul_msb_64",
                           ir->add_mov_const_64(ir, (uint64_t) arm64_hlp_smul_msb_64),
                           param);
}

static struct irRegister *mk_mul_s_lsb_64(struct irInstructionAllocator *ir, struct irRegister *op1, struct irRegister *op2)
{
    struct irRegister *param[4] = {op1, op2, NULL, NULL};

    return ir->add_call_64(ir, "arm64_hlp_smul_lsb_64",
                           ir->add_mov_const_64(ir, (uint64_t) arm64_hlp_smul_lsb_64),
                           param);
}

static void mk_barrier(struct irInstructionAllocator *ir)
{
    struct irRegister *params[4] = {NULL, NULL, NULL, NULL};

    ir->add_call_void(ir, "arm64_hlp_memory_barrier",
                           mk_64(ir, (uint64_t) arm64_hlp_memory_barrier),
                           params);
}

static uint64_t simd_immediate(int op, int cmode, int imm8_p)
{
    uint64_t imm8 = imm8_p;
    uint64_t imm64 = 0;

    switch(cmode >> 1) {
    case 0:
        imm64 = (imm8 << 32) | (imm8 << 0);
        break;
    case 1:
        imm64 = (imm8 << 40) | (imm8 << 8);
        break;
    case 2:
        imm64 = (imm8 << 48) | (imm8 << 16);
        break;
    case 3:
        imm64 = (imm8 << 56) | (imm8 << 24);
        break;
    case 4:
        imm64 = (imm8 << 48) | (imm8 << 32) | (imm8 << 16) | (imm8 << 0);
        break;
    case 5:
        imm64 = (imm8 << 56) | (imm8 << 40) | (imm8 << 24) | (imm8 << 8);
        break;
    case 6:
        if (cmode & 1)
            imm64 = (imm8 << 48) | (0xffffUL << 32) | (imm8 << 16) | 0xffff;
        else
            imm64 = (imm8 << 40) | (0xffUL << 32) | (imm8 << 8) | 0xff;
        break;
    case 7:
        if ((cmode & 1) == 0 && op == 0) {
            imm64 = (imm8 << 56) | (imm8 << 48) | (imm8 << 40) | (imm8 << 32) | (imm8 << 24) |  (imm8 << 16) | (imm8 << 8) | (imm8 << 0);
        } else if ((cmode & 1) == 0 && op == 1) {
            int i;

            for(i=0;i<8;i++)
                imm64 |= (imm8&(1<<i))?(0xffUL << (8*i)):0;
        } else {
            fatal("cmode0 = %d / op = %d\n", (cmode & 1), op);
        }
        break;
    default:
        assert(0);
    }

    return imm64;
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

static int dis_load_store_no_allocate_pair_offset(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31, 31);
    int is_load = INSN(22, 22);
    int64_t imm7 = INSN(21, 15);
    int rt2 = INSN(14, 10);
    int rn = INSN(9, 5);
    int rt = INSN(4, 0);
    struct irRegister *address1;
    struct irRegister *address2;
    int64_t offset;

    /* compute address */
    offset = ((imm7 << 57) >> 57);
    offset = offset << (2 + is_64);
    address1 = ir->add_add_64(ir, read_x(ir, rn, SP_REG), mk_64(ir, offset));
    address2 = ir->add_add_64(ir, address1, mk_64(ir, is_64?8:4));

    /* read write reg */
    if (is_load) {
        if (is_64) {
            write_x(ir, rt, ir->add_load_64(ir, address1), ZERO_REG);
            write_x(ir, rt2, ir->add_load_64(ir, address2), ZERO_REG);
        } else {
            write_w(ir, rt, ir->add_load_32(ir, address1), ZERO_REG);
            write_w(ir, rt2, ir->add_load_32(ir, address2), ZERO_REG);
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

static int dis_load_store_pair_simd_vfp(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int opc = INSN(31,30);
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
    offset = offset << (2 + opc);
    if (is_not_postindex)
        address1 = ir->add_add_64(ir, read_x(ir, rn, SP_REG), mk_64(ir, offset));
    else
        address1 = read_x(ir, rn, SP_REG);
    address2 = ir->add_add_64(ir, address1, mk_64(ir, 4 << opc));

    /* read write reg */
    switch(opc) {
        case 0:
            if (is_load) {
                write_s(ir, rt, ir->add_load_32(ir, address1));
                write_s(ir, rt2, ir->add_load_32(ir, address2));
            } else {
                ir->add_store_32(ir, read_s(ir, rt), address1);
                ir->add_store_32(ir, read_s(ir, rt2), address2);
            }
            break;
        case 1:
            if (is_load) {
                write_d(ir, rt, ir->add_load_64(ir, address1));
                write_d(ir, rt2, ir->add_load_64(ir, address2));
            } else {
                ir->add_store_64(ir, read_d(ir, rt), address1);
                ir->add_store_64(ir, read_d(ir, rt2), address2);
            }
            break;
        case 2:
            if (is_load) {
                write_v_lsb(ir, rt, ir->add_load_64(ir, address1));
                write_v_msb(ir, rt, ir->add_load_64(ir, ir->add_add_64(ir, address1, mk_64(ir, 8))));
                write_v_lsb(ir, rt2, ir->add_load_64(ir, address2));
                write_v_msb(ir, rt2, ir->add_load_64(ir, ir->add_add_64(ir, address2, mk_64(ir, 8))));
            } else {
                ir->add_store_64(ir, read_v_lsb(ir, rt), address1);
                ir->add_store_64(ir, read_v_msb(ir, rt), ir->add_add_64(ir, address1, mk_64(ir, 8)));
                ir->add_store_64(ir, read_v_lsb(ir, rt2), address2);
                ir->add_store_64(ir, read_v_msb(ir, rt2), ir->add_add_64(ir, address2, mk_64(ir, 8)));
            }
            break;
        default:
            assert(0);
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

static int dis_load_store_pair_pre_indexed_simd_vfp(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    return dis_load_store_pair_simd_vfp(context, insn, ir);
}

static int dis_load_store_pair_offset_simd_vfp(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    return dis_load_store_pair_simd_vfp(context, insn, ir);
}

static int dis_load_store_pair_post_indexed_simd_vfp(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    return dis_load_store_pair_simd_vfp(context, insn, ir);
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
    int64_t imm = ((int64_t)INSN(23, 5) << 14) + (INSN(30, 29) << 12UL);
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
    int is_setflags = 0;
    struct irRegister *res;
    struct irRegister *op2;
    struct irRegister *op2_inverted;

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
                op2 = ir->add_32U_to_64(ir, mk_ror_imm_32(ir, read_w(ir, rm, ZERO_REG), imm6));
            break;
    }

    if (n)
        op2_inverted = ir->add_xor_64(ir, op2, mk_64(ir, ~0UL));
    else
        op2_inverted = op2;

    /* do the op */
    switch(opc) {
        case 3:
            is_setflags = 1;
            //fallthrought
        case 0://and
            res = ir->add_and_64(ir, read_x(ir, rn, ZERO_REG), op2_inverted);
            break;
        case 1://orr
            res = ir->add_or_64(ir, read_x(ir, rn, ZERO_REG), op2_inverted);
            break;
        case 2://eor
            res = ir->add_xor_64(ir, read_x(ir, rn, ZERO_REG), op2_inverted);
            break;
        default:
            fatal("opc = %d\n", opc);
    }

    /* update nzcv if needed */
    if (is_setflags) {
        if (is_64)
            write_nzcv(ir, mk_next_nzcv_64(ir, OPS_LOGICAL, res, mk_64(ir, 0)));
        else
            write_nzcv(ir, mk_next_nzcv_32(ir, OPS_LOGICAL, ir->add_64_to_32(ir, res), mk_32(ir, 0)));
    }

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

static int dis_ccmp_A_ccmn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir, struct irRegister *op2)
{
    int is_64 = INSN(31,31);
    int op = INSN(30,30);
    int cond = INSN(15,12);
    int rn = INSN(9,5);
    int nzcv = INSN(4,0);
    struct irRegister *params[4];
    struct irRegister *pred;
    struct irRegister *res_true;
    struct irRegister *res_false;
    struct irRegister *res;

    /* compute pred */
    params[0] = mk_32(ir, cond);
    params[1] = read_nzcv(ir);
    params[2] = NULL;
    params[3] = NULL;
    pred = ir->add_call_32(ir, "arm64_hlp_compute_flags_pred",
                           mk_64(ir, (uint64_t) arm64_hlp_compute_flags_pred),
                           params);

    /* compute true value */
    if (is_64) {
        params[0] = mk_32(ir, op?OPS_SUB:OPS_ADD);
        params[1] = read_x(ir, rn, ZERO_REG);
        params[2] = op2;
        params[3] = read_nzcv(ir);
        res_true = ir->add_call_32(ir, "arm64_hlp_compute_next_nzcv_64",
                               mk_64(ir, (uint64_t) arm64_hlp_compute_next_nzcv_64),
                               params);
    } else {
        params[0] = mk_32(ir, op?OPS_SUB:OPS_ADD);
        params[1] = read_w(ir, rn, ZERO_REG);
        params[2] = op2;
        params[3] = read_nzcv(ir);
        res_true = ir->add_call_32(ir, "arm64_hlp_compute_next_nzcv_32",
                               mk_64(ir, (uint64_t) arm64_hlp_compute_next_nzcv_32),
                               params);
    }
    /* compute false value */
    res_false = mk_32(ir, nzcv << 28);

    /* compute 32 bits result */
    res = ir->add_ite_32(ir, pred,
                             res_true,
                             res_false);

    /* write reg */
    write_nzcv(ir, res);

    return 0;
}

static int dis_ccmp_A_ccmn_immediate(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int imm5 = INSN(20,16);
    struct irRegister *op2;

    op2 = is_64?mk_64(ir, imm5):mk_32(ir, imm5);

    return dis_ccmp_A_ccmn(context, insn, ir, op2);
}

static int dis_ccmp_A_ccmn_register(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int rm = INSN(20,16);
    struct irRegister *op2;

    op2 = is_64?read_x(ir, rm, ZERO_REG):read_w(ir, rm, ZERO_REG);

    return dis_ccmp_A_ccmn(context, insn, ir, op2);
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
    int rm = INSN(20,16);

    return dis_cond_select(context, insn, ir,
                           ir->add_sub_64(ir, mk_64(ir, 0UL), read_x(ir, rm, ZERO_REG)));
}

static int dis_csinv(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN(20,16);

    return dis_cond_select(context, insn, ir,
                           ir->add_xor_64(ir, read_x(ir, rm, ZERO_REG), mk_64(ir, ~0UL)));
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
    int size = (INSN(23,23) << 2) | INSN(31,30);
    int is_load = INSN(22,22);
    int imm12 = INSN(21, 10);
    int rn = INSN(9, 5);
    int rt = INSN(4, 0);
    struct irRegister *address;

    /* compute address */
    address = ir->add_add_64(ir, read_x(ir, rn, SP_REG), mk_64(ir, imm12 << size));

    /* do load store */
    if (is_load) {
        switch(size) {
            case 0:
                write_b(ir, rt, ir->add_load_8(ir, address));
                break;
            case 1:
                write_h(ir, rt, ir->add_load_16(ir, address));
                break;
            case 2:
                write_s(ir, rt, ir->add_load_32(ir, address));
                break;
            case 3:
                write_d(ir, rt, ir->add_load_64(ir, address));
                break;
            case 4:
                write_v_lsb(ir, rt, ir->add_load_64(ir, address));
                write_v_msb(ir, rt, ir->add_load_64(ir, ir->add_add_64(ir, address, mk_64(ir, 8))));
                break;
            default:
                fatal("size = %d\n", size);
        }
    } else {
        switch(size) {
            case 0:
                ir->add_store_8(ir, read_b(ir, rt), address);
                break;
            case 1:
                ir->add_store_16(ir, read_h(ir, rt), address);
                break;
            case 2:
                ir->add_store_32(ir, read_s(ir, rt), address);
                break;
            case 3:
                ir->add_store_64(ir, read_d(ir, rt), address);
                break;
            case 4:
                ir->add_store_64(ir, read_v_lsb(ir, rt), address);
                ir->add_store_64(ir, read_v_msb(ir, rt), ir->add_add_64(ir, address, mk_64(ir, 8)));
                break;
            default:
                fatal("size = %d\n", size);
        }
    }

    return 0;
}

static int dis_load_store_register_unscaled_immediate(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int size = INSN(31,30);
    int opc = INSN(23,22);
    int64_t imm9 = INSN(20, 12);
    int rn = INSN(9, 5);
    int rt = INSN(4, 0);
    struct irRegister *address;

    /* prfm is translated into nop */
    if (size == 3 && opc == 2)
        return 0;

    imm9 = (imm9 << 55) >> 55;

    /* compute address */
    address = ir->add_add_64(ir, read_x(ir, rn, SP_REG), mk_64(ir, imm9));

    /* do load store */
    dis_load_store_register(context, insn, ir, size, opc, rt, address);

    return 0;
}

static int dis_load_store_register_unscaled_immediate_simd(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
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

static int dis_load_store_register_unprivileged(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    return dis_load_store_register_unscaled_immediate(context, insn, ir);
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
    int opc = INSN(31,30);
    int64_t imm19 = INSN(23,5);
    int rt = INSN(4,0);
    struct irRegister *address;

    imm19 = (imm19 << 45) >> 43;
    /* compute address */
    address = mk_64(ir, context->pc + imm19);

    switch(opc) {
        case 0:
            write_s(ir, rt, ir->add_load_32(ir, address));
            break;
        case 1:
            write_d(ir, rt, ir->add_load_64(ir, address));
            break;
        case 2:
            {
                struct irRegister *address2 = mk_64(ir, context->pc + imm19 + 8);

                write_v_lsb(ir, rt, ir->add_load_64(ir, address));
                write_v_msb(ir, rt, ir->add_load_64(ir, address2));
            }
            break;
        default:
            assert(0);
    }

    return 0;
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
        write_x(ir, rd, res, is_setflags?ZERO_REG:SP_REG);
    else
        write_w(ir, rd, ir->add_64_to_32(ir, res), is_setflags?ZERO_REG:SP_REG);

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

static int dis_extr(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int imms = INSN(15,10);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    struct irRegister *res;

    if (is_64) {
        res = ir->add_or_64(ir,
                            ir->add_shr_64(ir, read_x(ir, rm, ZERO_REG), mk_8(ir, imms)),
                            ir->add_shl_64(ir, read_x(ir, rn, ZERO_REG), mk_8(ir, 64 - imms)));
        write_x(ir, rd, res, ZERO_REG);
    } else {
        res = ir->add_or_32(ir,
                            ir->add_shr_32(ir, read_w(ir, rm, ZERO_REG), mk_8(ir, imms)),
                            ir->add_shl_32(ir, read_w(ir, rn, ZERO_REG), mk_8(ir, 32 - imms)));
        write_w(ir, rd, res, ZERO_REG);
    }

    return 0;
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

static int dis_sdiv(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
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

        res = ir->add_call_32(ir, "arm64_hlp_sdiv_64",
                               ir->add_mov_const_64(ir, (uint64_t) arm64_hlp_sdiv_64),
                               params);
        write_x(ir, rd, res, ZERO_REG);
    } else {
        params[0] = read_w(ir, rn, ZERO_REG);
        params[1] = read_w(ir, rm, ZERO_REG);
        params[2] = NULL;
        params[3] = NULL;

        res = ir->add_call_32(ir, "arm64_hlp_sdiv_32",
                               ir->add_mov_const_64(ir, (uint64_t) arm64_hlp_sdiv_32),
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

static int dis_lsrv(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int rm = INSN(20,16);
    int rn = INSN(9,5);
    int rd = INSN(4,0);
    struct irRegister *res;
    int mask = is_64?0x3f:0x1f;
    uint64_t mask_res = is_64?~0UL:0xffffffffUL;

    res = ir->add_shr_64(ir,
                         ir->add_and_64(ir, read_x(ir, rn, ZERO_REG), mk_64(ir, mask_res)),
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

static int dis_asrv(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int rm = INSN(20,16);
    int rn = INSN(9,5);
    int rd = INSN(4,0);
    struct irRegister *res;
    int mask = is_64?0x3f:0x1f;

    if (is_64) {
        res = ir->add_asr_64(ir,
                             read_x(ir, rn, ZERO_REG),
                             ir->add_64_to_8(ir,
                                             ir->add_and_64(ir,
                                                            read_x(ir, rm, ZERO_REG),
                                                            mk_64(ir, mask))));
        write_x(ir, rd, res, ZERO_REG);
    } else {
        res = ir->add_asr_32(ir,
                             read_w(ir, rn, ZERO_REG),
                             ir->add_64_to_8(ir,
                                             ir->add_and_64(ir,
                                                            read_x(ir, rm, ZERO_REG),
                                                            mk_64(ir, mask))));
        write_w(ir, rd, res, ZERO_REG);
    }

    return 0;
}

static int dis_rorv(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int rm = INSN(20,16);
    int rn = INSN(9,5);
    int rd = INSN(4,0);
    struct irRegister *res;
    int mask = is_64?0x3f:0x1f;
    int sub = mask + 1;
    struct irRegister *rotation1;
    struct irRegister *rotation2;
    struct irRegister *rn_reg;

    /* compute rotation value */
    rotation1 = ir->add_64_to_8(ir, ir->add_and_64(ir, read_x(ir, rm, ZERO_REG), mk_64(ir, mask)));
    rotation2 = ir->add_sub_8(ir , mk_8(ir, sub), rotation1);

    /* rotate */
    if (is_64) {
        rn_reg = read_x(ir, rn, ZERO_REG);
        res = ir->add_or_64(ir,
                            ir->add_shr_64(ir, rn_reg, rotation1),
                            ir->add_shl_64(ir, rn_reg, rotation2));
        write_x(ir, rd, res, ZERO_REG);
    } else {
        rn_reg = read_w(ir, rn, ZERO_REG);
        res = ir->add_or_32(ir,
                            ir->add_shr_32(ir, rn_reg, rotation1),
                            ir->add_shl_32(ir, rn_reg, rotation2));
        write_w(ir, rd, res, ZERO_REG);
    }

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

static int dis_msub(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int rd = INSN(4,0);
    int ra = INSN(14,10);
    int rn = INSN(9,5);
    int rm = INSN(20,16);

    if (is_64) {
        write_x(ir, rd,
                ir->add_sub_64(ir,
                               read_x(ir, ra, ZERO_REG),
                               mk_mul_u_lsb_64(ir,
                                               read_x(ir, rn, ZERO_REG),
                                               read_x(ir, rm, ZERO_REG))),
                ZERO_REG);
    } else {
        write_w(ir, rd,
                ir->add_sub_32(ir,
                               read_w(ir, ra, ZERO_REG),
                               mk_mul_u_lsb_32(ir,
                                               read_w(ir, rn, ZERO_REG),
                                               read_w(ir, rm, ZERO_REG))),
                ZERO_REG);
    }

    return 0;
}

static int dis_smaddl(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(4,0);
    int ra = INSN(14,10);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    struct irRegister *mult;
    struct irRegister *res;

    mult = mk_mul_s_lsb_64(ir,
                           ir->add_32S_to_64(ir, read_w(ir, rn, ZERO_REG)),
                           ir->add_32S_to_64(ir, read_w(ir, rm, ZERO_REG)));
    res = ir->add_add_64(ir, read_x(ir, ra, ZERO_REG), mult);
    write_x(ir, rd, res, ZERO_REG);

    return 0;
}

static int dis_smsubl(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(4,0);
    int ra = INSN(14,10);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    struct irRegister *mult;
    struct irRegister *res;

    mult = mk_mul_s_lsb_64(ir,
                           ir->add_32S_to_64(ir, read_w(ir, rn, ZERO_REG)),
                           ir->add_32S_to_64(ir, read_w(ir, rm, ZERO_REG)));
    res = ir->add_sub_64(ir, read_x(ir, ra, ZERO_REG), mult);
    write_x(ir, rd, res, ZERO_REG);

    return 0;
}

static int dis_smulh(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN(20,16);
    int rn = INSN(9,5);
    int rd = INSN(4,0);

    write_x(ir, rd,
                mk_mul_s_msb_64(ir, read_x(ir, rn, ZERO_REG), read_x(ir, rm, ZERO_REG)),
                ZERO_REG);

    return 0;
}

static int dis_umaddl(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(4,0);
    int ra = INSN(14,10);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    struct irRegister *mult;
    struct irRegister *res;

    mult = mk_mul_u_lsb_64(ir,
                           ir->add_32U_to_64(ir, read_w(ir, rn, ZERO_REG)),
                           ir->add_32U_to_64(ir, read_w(ir, rm, ZERO_REG)));
    res = ir->add_add_64(ir, read_x(ir, ra, ZERO_REG), mult);
    write_x(ir, rd, res, ZERO_REG);

    return 0;
}

static int dis_umsubl(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(4,0);
    int ra = INSN(14,10);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    struct irRegister *mult;
    struct irRegister *res;

    mult = mk_mul_u_lsb_64(ir,
                           ir->add_32U_to_64(ir, read_w(ir, rn, ZERO_REG)),
                           ir->add_32U_to_64(ir, read_w(ir, rm, ZERO_REG)));
    res = ir->add_sub_64(ir, read_x(ir, ra, ZERO_REG), mult);
    write_x(ir, rd, res, ZERO_REG);

    return 0;
}

static int dis_umulh(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rm = INSN(20,16);
    int rn = INSN(9,5);
    int rd = INSN(4,0);

    write_x(ir, rd,
                mk_mul_u_msb_64(ir, read_x(ir, rn, ZERO_REG), read_x(ir, rm, ZERO_REG)),
                ZERO_REG);

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
    } else if (op0 == 3 && op1 == 3 && crn == 0x4 && crm == 2 && op2 == 0) {
        write_nzcv(ir, read_w(ir, rt, ZERO_REG));
    } else {
        fprintf(stderr, "op0=%d / op1=%d / crn=%d / crm=%d / op2=%d\n", op0, op1, crn, crm, op2);
        fprintf(stderr, "pc = 0x%016lx\n", context->pc);
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
    } else if (op0 == 3 && op1 == 3 && crn == 0x0 && crm == 0 && op2 == 7) {
        //dczid_el0. We declare instruction is prohibited
        write_x(ir, rt, mk_64(ir, (1 << 4) | 7 ), ZERO_REG);
    } else if (op0 == 3 && op1 == 3 && crn == 0x4 && crm == 2 && op2 == 0) {
        write_x(ir, rt, ir->add_32U_to_64(ir, read_nzcv(ir)), ZERO_REG);
    } else {
        fprintf(stderr, "op0=%d / op1=%d / crn=%d / crm=%d / op2=%d\n", op0, op1, crn, crm, op2);
        fprintf(stderr, "pc = 0x%016lx\n", context->pc);
        assert(0);
    }

    return 0;
}

static int dis_load_exclusive(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int size = INSN(31,30);
    int rt = INSN(4,0);
    int rn = INSN(9,5);
    struct irRegister *params[4];
    struct irRegister *result;

    params[0] = read_x(ir, rn, SP_REG);
    params[1] = mk_32(ir, size);
    params[2] = NULL;
    params[3] = NULL;

    result = ir->add_call_64(ir, "arm64_hlp_ldxr",
                             mk_64(ir, (uint64_t) arm64_hlp_ldxr),
                             params);

    write_x(ir, rt, result, ZERO_REG);

    return 0;
}

static int dis_load_aquire_exclusive(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int size = INSN(31,30);
    int rt = INSN(4,0);
    int rn = INSN(9,5);
    struct irRegister *params[4];
    struct irRegister *result;

    params[0] = read_x(ir, rn, SP_REG);
    params[1] = mk_32(ir, size);
    params[2] = NULL;
    params[3] = NULL;

    result = ir->add_call_64(ir, "arm64_hlp_ldaxr",
                             mk_64(ir, (uint64_t) arm64_hlp_ldaxr),
                             params);

    write_x(ir, rt, result, ZERO_REG);

    return 0;
}

static int dis_load_aquire_exclusive_pair(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "arm64_hlp_ldaxp_dirty",
                           mk_64(ir, (uint64_t) arm64_hlp_ldaxp_dirty),
                           params);

    return 0;
}

static int dis_load_exclusive_pair(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "arm64_hlp_ldxp_dirty",
                           mk_64(ir, (uint64_t) arm64_hlp_ldxp_dirty),
                           params);

    return 0;
}

static int dis_load_aquire(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int size = INSN(31,30);
    int rt = INSN(4,0);
    int rn = INSN(9,5);

    if (size == 3) {
        write_x(ir, rt, ir->add_load_64(ir, read_x(ir, rn, SP_REG)), ZERO_REG);
    } else if (size == 2) {
        write_w(ir, rt, ir->add_load_32(ir, read_x(ir, rn, SP_REG)), ZERO_REG);
    } else if (size == 1) {
        write_x(ir, rt, ir->add_16U_to_64(ir, ir->add_load_16(ir, read_x(ir, rn, SP_REG))), ZERO_REG);
    } else {
        write_x(ir, rt, ir->add_8U_to_64(ir, ir->add_load_8(ir, read_x(ir, rn, SP_REG))), ZERO_REG);
    }

    mk_barrier(ir);

    return 0;
}

static int dis_store_release(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int size = INSN(31,30);
    int rt = INSN(4,0);
    int rn = INSN(9,5);

    mk_barrier(ir);

    if (size == 3) {
        ir->add_store_64(ir, read_x(ir, rt, ZERO_REG), read_x(ir, rn, SP_REG));
    } else if (size == 2) {
        ir->add_store_32(ir, read_w(ir, rt, ZERO_REG), read_x(ir, rn, SP_REG));
    } else if (size == 1) {
        ir->add_store_16(ir, ir->add_64_to_16(ir, read_x(ir, rt, ZERO_REG)), read_x(ir, rn, SP_REG));
    } else {
        ir->add_store_8(ir, ir->add_64_to_8(ir, read_x(ir, rt, ZERO_REG)), read_x(ir, rn, SP_REG));
    }

    return 0;
}

static int dis_store_exclusive(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int size = INSN(31,30);
    int rs = INSN(20,16);
    int rt = INSN(4,0);
    int rn = INSN(9,5);
    struct irRegister *params[4];
    struct irRegister *result;

    params[0] = read_x(ir, rn, SP_REG);
    params[1] = mk_32(ir, size);
    params[2] = read_x(ir, rt, SP_REG);
    params[3] = NULL;

    result = ir->add_call_32(ir, "arm64_hlp_stxr",
                             mk_64(ir, (uint64_t) arm64_hlp_stxr),
                             params);

    write_w(ir, rs, result, ZERO_REG);

    return 0;
}

static int dis_store_exclusive_pair(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "arm64_hlp_stxp_dirty",
                           mk_64(ir, (uint64_t) arm64_hlp_stxp_dirty),
                           params);

    return 0;
}

static int dis_store_release_exclusive_pair(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "arm64_hlp_stlxp_dirty",
                           mk_64(ir, (uint64_t) arm64_hlp_stlxp_dirty),
                           params);

    return 0;
}

static int dis_store_release_exclusive(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int size = INSN(31,30);
    int rs = INSN(20,16);
    int rt = INSN(4,0);
    int rn = INSN(9,5);
    struct irRegister *params[4];
    struct irRegister *result;

    params[0] = read_x(ir, rn, SP_REG);
    params[1] = mk_32(ir, size);
    params[2] = read_x(ir, rt, SP_REG);
    params[3] = NULL;

    result = ir->add_call_32(ir, "arm64_hlp_stlxr",
                             mk_64(ir, (uint64_t) arm64_hlp_stlxr),
                             params);

    write_w(ir, rs, result, ZERO_REG);

    return 0;
}

static int dis_rbit_64(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    struct irRegister *x[7];
    uint64_t c[] = {0x5555555555555555, 0x3333333333333333,
                    0x0F0F0F0F0F0F0F0F, 0x00FF00FF00FF00FF,
                    0x0000FFFF0000FFFF, 0x00000000FFFFFFFF};
    int i;

    x[0] = read_x(ir, rn, ZERO_REG);
    for(i = 0; i < 6; i++) {
        x[i+1] = ir->add_or_64(ir,
                               ir->add_shl_64(ir,
                                              ir->add_and_64(ir,x[i], mk_64(ir, c[i])),
                                              mk_8(ir, 1 << i)),
                               ir->add_shr_64(ir,
                                              ir->add_and_64(ir,x[i], mk_64(ir, ~c[i])),
                                              mk_8(ir, 1 << i)));
    }
    write_x(ir, rd, x[6], ZERO_REG);

    return 0;
}

static int dis_rbit_32(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    struct irRegister *w[6];
    uint32_t c[] = {0x55555555, 0x33333333,
                    0x0F0F0F0F, 0x00FF00FF,
                    0x0000FFFF};
    int i;

    w[0] = read_w(ir, rn, ZERO_REG);
    for(i = 0; i < 5; i++) {
        w[i+1] = ir->add_or_32(ir,
                               ir->add_shl_32(ir,
                                              ir->add_and_32(ir,w[i], mk_32(ir, c[i])),
                                              mk_8(ir, 1 << i)),
                               ir->add_shr_32(ir,
                                              ir->add_and_32(ir,w[i], mk_32(ir, ~c[i])),
                                              mk_8(ir, 1 << i)));
    }
    write_w(ir, rd, w[5], ZERO_REG);

    return 0;
}

static int dis_rbit(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    if (INSN(31,31))
        return dis_rbit_64(context, insn, ir);
    else
        return dis_rbit_32(context, insn, ir);
}

static int dis_rev16(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    struct irRegister *rn_reg;
    struct irRegister *res;

    if (is_64) {
        rn_reg = read_x(ir, rn, ZERO_REG);
        res    = ir->add_or_64(ir,
                               ir->add_shl_64(ir,
                                              ir->add_and_64(ir, rn_reg, mk_64(ir, 0x00ff00ff00ff00ffUL)),
                                              mk_8(ir, 8)),
                               ir->add_shr_64(ir,
                                              ir->add_and_64(ir, rn_reg, mk_64(ir, 0xff00ff00ff00ff00UL)),
                                              mk_8(ir, 8)));
        write_x(ir, rd, res, ZERO_REG);
    } else {
        rn_reg = read_w(ir, rn, ZERO_REG);
        res    = ir->add_or_32(ir,
                               ir->add_shl_32(ir,
                                              ir->add_and_32(ir, rn_reg, mk_32(ir, 0x00ff00ff)),
                                              mk_8(ir, 8)),
                               ir->add_shr_32(ir,
                                              ir->add_and_32(ir, rn_reg, mk_32(ir, 0xff00ff00)),
                                              mk_8(ir, 8)));
        write_w(ir, rd, res, ZERO_REG);
    }

    return 0;
}

static int dis_rev32(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    struct irRegister *rn_reg;
    struct irRegister *res;
    struct irRegister *phase1;

    rn_reg = read_x(ir, rn, ZERO_REG);
    phase1 = ir->add_or_64(ir,
                           ir->add_shl_64(ir,
                                          ir->add_and_64(ir, rn_reg, mk_64(ir, 0x0000ffff0000ffffUL)),
                                          mk_8(ir, 16)),
                           ir->add_shr_64(ir,
                                          ir->add_and_64(ir, rn_reg, mk_64(ir, 0xffff0000ffff0000UL)),
                                          mk_8(ir, 16)));
    res    = ir->add_or_64(ir,
                           ir->add_shl_64(ir,
                                          ir->add_and_64(ir, phase1, mk_64(ir, 0x00ff00ff00ff00ffUL)),
                                          mk_8(ir, 8)),
                           ir->add_shr_64(ir,
                                          ir->add_and_64(ir, phase1, mk_64(ir, 0xff00ff00ff00ff00UL)),
                                          mk_8(ir, 8)));
    write_x(ir, rd, res, ZERO_REG);

    return 0;
}

static int dis_rev(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int rn = INSN(9,5);
    int rd = INSN(4,0);
    struct irRegister *phase1;
    struct irRegister *phase2;
    struct irRegister *res;
    struct irRegister *rn_reg;

    if (is_64) {
        rn_reg = read_x(ir, rn, ZERO_REG);
        phase1 = ir->add_or_64(ir,
                               ir->add_shl_64(ir,
                                              ir->add_and_64(ir, rn_reg, mk_64(ir, 0x00000000ffffffffUL)),
                                              mk_8(ir, 32)),
                               ir->add_shr_64(ir,
                                              ir->add_and_64(ir, rn_reg, mk_64(ir, 0xffffffff00000000UL)),
                                              mk_8(ir, 32)));
        phase2 = ir->add_or_64(ir,
                               ir->add_shl_64(ir,
                                              ir->add_and_64(ir, phase1, mk_64(ir, 0x0000ffff0000ffffUL)),
                                              mk_8(ir, 16)),
                               ir->add_shr_64(ir,
                                              ir->add_and_64(ir, phase1, mk_64(ir, 0xffff0000ffff0000UL)),
                                              mk_8(ir, 16)));
        res    = ir->add_or_64(ir,
                               ir->add_shl_64(ir,
                                              ir->add_and_64(ir, phase2, mk_64(ir, 0x00ff00ff00ff00ffUL)),
                                              mk_8(ir, 8)),
                               ir->add_shr_64(ir,
                                              ir->add_and_64(ir, phase2, mk_64(ir, 0xff00ff00ff00ff00UL)),
                                              mk_8(ir, 8)));
        write_x(ir, rd, res, ZERO_REG);
    } else {
        rn_reg = read_w(ir, rn, ZERO_REG);
        phase1 = ir->add_or_32(ir,
                               ir->add_shl_32(ir,
                                              ir->add_and_32(ir, rn_reg, mk_32(ir, 0x0000ffff)),
                                              mk_8(ir, 16)),
                               ir->add_shr_32(ir,
                                              ir->add_and_32(ir, rn_reg, mk_32(ir, 0xffff0000)),
                                              mk_8(ir, 16)));
        res    = ir->add_or_32(ir,
                               ir->add_shl_32(ir,
                                              ir->add_and_32(ir, phase1, mk_32(ir, 0x00ff00ff)),
                                              mk_8(ir, 8)),
                               ir->add_shr_32(ir,
                                              ir->add_and_32(ir, phase1, mk_32(ir, 0xff00ff00)),
                                              mk_8(ir, 8)));
        write_w(ir, rd, res, ZERO_REG);
    }

    return 0;
}

static int dis_clz(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int rn = INSN(9,5);
    int rd = INSN(4,0);
    struct irRegister *params[4];
    struct irRegister *res;

    params[0] = read_x(ir, rn, ZERO_REG);
    params[1] = mk_32(ir, is_64?63:31);
    params[2] = NULL;
    params[3] = NULL;


    res = ir->add_call_64(ir, "arm64_hlp_clz",
                             mk_64(ir, (uint64_t) arm64_hlp_clz),
                             params);

    if (is_64)
        write_x(ir, rd, res, ZERO_REG);
    else
        write_w(ir, rd, ir->add_64_to_32(ir, res), ZERO_REG);

    return 0;
}

static int dis_cls(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int rn = INSN(9,5);
    int rd = INSN(4,0);
    struct irRegister *params[4];
    struct irRegister *res;

    params[0] = read_x(ir, rn, ZERO_REG);
    params[1] = mk_32(ir, is_64?63:31);
    params[2] = NULL;
    params[3] = NULL;


    res = ir->add_call_64(ir, "arm64_hlp_cls",
                             mk_64(ir, (uint64_t) arm64_hlp_cls),
                             params);

    if (is_64)
        write_x(ir, rd, res, ZERO_REG);
    else
        write_w(ir, rd, ir->add_64_to_32(ir, res), ZERO_REG);

    return 0;
}

static int dis_clrex(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = NULL;
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "arm64_hlp_clrex",
                           mk_64(ir, (uint64_t) arm64_hlp_clrex),
                           params);

    return 0;
}

static int dis_dsb(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    mk_barrier(ir);

    return 0;
}

static int dis_dmb(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    mk_barrier(ir);

    return 0;
}

static int dis_isb(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    mk_barrier(ir);

    return 0;
}

static int dis_fmov(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int sf = INSN(31,31);
    int type = INSN(23,22);
    int rmode = INSN(20,19);
    int opcode = INSN(18,16);
    int rn = INSN(9,5);
    int rd = INSN(4,0);

    if (sf == 0 && type == 0 && rmode == 0 && opcode == 7) {
        //fmov Sd, Wn
        write_s(ir, rd, read_w(ir, rn, ZERO_REG));
    } else if (sf == 0 && type == 0 && rmode == 0 && opcode == 6) {
        //fmov Wd, Sn
        write_w(ir, rd, read_s(ir, rn), ZERO_REG);
    } else if (sf == 1 && type == 1 && rmode == 0 && opcode == 7) {
        //fmov Dd, Xn
        write_d(ir, rd, read_x(ir, rn, ZERO_REG));
    } else if (sf == 1 && type == 1 && rmode == 0 && opcode == 6) {
        //fmov Xd, Dn
        write_x(ir, rd, read_d(ir, rn), ZERO_REG);
    } else if (sf == 1 && type == 2 && rmode == 1 && opcode == 6) {
        //fmod Xd, Vn.D[1]
        write_x(ir, rd, read_v_msb(ir, rn), ZERO_REG);
    } else
        fatal("sf=%d / type=%d / rmode=%d / opcode=%d\n", sf, type, rmode, opcode);

    return 0;
}

static int dis_fmov_register(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_d = INSN(22,22);
    int rd = INSN(4,0);
    int rn = INSN(9,5);

    if (is_d)
        write_d(ir, rd, read_d(ir, rn));
    else
        write_s(ir, rd, read_s(ir, rn));

    return 0;
}

static int dis_umov(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int q_imm5 = (INSN(30,30) << 5) | INSN(20,16);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int index;
    struct irRegister *res;

    if (q_imm5 & 1) {
        index = q_imm5 >> 1;
        res = ir->add_8U_to_64(ir, ir->add_read_context_8(ir, offsetof(struct arm64_registers, v[rn].b[index])));
    } else if (q_imm5 & 2) {
        index = q_imm5 >> 2;
        res = ir->add_16U_to_64(ir, ir->add_read_context_16(ir, offsetof(struct arm64_registers, v[rn].h[index])));
    } else if (q_imm5 & 4) {
        index = q_imm5 >> 3;
        res = ir->add_32U_to_64(ir, ir->add_read_context_32(ir, offsetof(struct arm64_registers, v[rn].s[index])));
    } else if (q_imm5 & 8) {
        index = (q_imm5 >> 4) & 1;
        res = ir->add_read_context_64(ir, offsetof(struct arm64_registers, v[rn].d[index]));
    } else
        assert(0);
    write_x(ir, rd, res, ZERO_REG);

    return 0;
}

static int dis_dup_general(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4] = {NULL, NULL, NULL, NULL};

    params[0] = mk_32(ir, insn);

    ir->add_call_void(ir, "arm64_hlp_dirty_simd_dup_general",
                           mk_64(ir, (uint64_t) arm64_hlp_dirty_simd_dup_general),
                           params);

    return 0;
}

static int dis_add_simd_vector(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4] = {NULL, NULL, NULL, NULL};

    params[0] = mk_32(ir, insn);

    ir->add_call_void(ir, "arm64_hlp_dirty_add_simd_vector",
                           mk_64(ir, (uint64_t) arm64_hlp_dirty_add_simd_vector),
                           params);

    return 0;
}

static int dis_cmpeq_simd_vector(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4] = {NULL, NULL, NULL, NULL};

    params[0] = mk_32(ir, insn);

    ir->add_call_void(ir, "arm64_hlp_dirty_cmpeq_simd_vector",
                           mk_64(ir, (uint64_t) arm64_hlp_dirty_cmpeq_simd_vector),
                           params);

    return 0;
}

static int dis_cmpeq_register_simd_vector(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4] = {NULL, NULL, NULL, NULL};

    params[0] = mk_32(ir, insn);

    ir->add_call_void(ir, "arm64_hlp_dirty_cmpeq_register_simd_vector",
                           mk_64(ir, (uint64_t) arm64_hlp_dirty_cmpeq_register_simd_vector),
                           params);

    return 0;
}

static int dis_orr_register_simd_vector(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4] = {NULL, NULL, NULL, NULL};

    params[0] = mk_32(ir, insn);

    ir->add_call_void(ir, "arm64_hlp_dirty_orr_register_simd_vector",
                           mk_64(ir, (uint64_t) arm64_hlp_dirty_orr_register_simd_vector),
                           params);

    return 0;
}

static int dis_and_simd_vector(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4] = {NULL, NULL, NULL, NULL};

    params[0] = mk_32(ir, insn);

    ir->add_call_void(ir, "arm64_hlp_dirty_and_simd_vector",
                           mk_64(ir, (uint64_t) arm64_hlp_dirty_and_simd_vector),
                           params);

    return 0;
}

static int dis_addp_simd_vector(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4] = {NULL, NULL, NULL, NULL};

    params[0] = mk_32(ir, insn);

    ir->add_call_void(ir, "arm64_hlp_dirty_addp_simd_vector",
                           mk_64(ir, (uint64_t) arm64_hlp_dirty_addp_simd_vector),
                           params);

    return 0;
}

static int dis_load_multiple_structure(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir, struct irRegister *address)
{
    int q = INSN(30,30);
    int opcode = INSN(15,12);
    int rt = INSN(4,0);
    int res = 0;

    if (opcode & 2) {
        //ld1
        int i;
        int nb;
        switch(opcode) {
            case 7: nb = 1; break;
            case 10: nb = 2; break;
            case 6: nb = 3; break;
            case 2: nb = 4; break;
            default: assert(0);
        }
        for(i = 0; i < nb; i++) {
            write_v_lsb(ir, (rt + i) % 32, ir->add_load_64(ir, ir->add_add_64(ir, address, mk_64(ir, i * (q?16:8)))));
            if (q)
                write_v_msb(ir, (rt + i) % 32, ir->add_load_64(ir, ir->add_add_64(ir, address, mk_64(ir, i * 16 + 8))));
        }
        res = 8 * (1 + q) * nb;
    } else {
        //ldn
        assert(0);
    }

    return res;
}

static int dis_store_multiple_structure(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir, struct irRegister *address)
{
    assert(0);
    return 0;
}

static int dis_load_store_multiple_structure(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
    return 0;
}

static int dis_load_store_multiple_structure_post_index(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int l = INSN(22,22);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    struct irRegister *address;
    int offset;

    address = read_x(ir, rn, SP_REG);
    if (l)
        offset = dis_load_multiple_structure(context, insn, ir, address);
    else
        offset = dis_store_multiple_structure(context, insn, ir, address);

    //writeback
    if (rm == 31)
        write_x(ir, rn, ir->add_add_64(ir, address, mk_64(ir, offset)), SP_REG);
    else
        write_x(ir, rn, ir->add_add_64(ir, address, read_x(ir, rm, ZERO_REG)), SP_REG);

    return 0;
}

static int dis_load_store_single_structure(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
    return 0;
}

static int dis_load_store_single_structure_post_index(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
    return 0;
}

static int dis_add_substract_with_carry(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int is_sub = INSN(30,30);
    int rd = INSN(4,0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int S = INSN(29,29);
    struct irRegister *carry;
    struct irRegister *op1;
    struct irRegister *op2;
    struct irRegister *op2_inverted;
    struct irRegister *res;

    /* got carry */
    carry = ir->add_and_32(ir,
                           ir->add_shr_32(ir, read_nzcv(ir), mk_8(ir, 29)),
                           mk_32(ir, 1));

    /* compute res */
    if (is_64) {
        op1 = read_x(ir, rn, ZERO_REG);
        op2 = read_x(ir, rm, ZERO_REG);
        if (is_sub)
            op2_inverted = ir->add_xor_64(ir, op2, mk_64(ir, ~0UL));
        else
            op2_inverted = op2;
        res = ir->add_add_64(ir,
                             ir->add_add_64(ir, op1, op2_inverted),
                             ir->add_32U_to_64(ir, carry));
    } else {
        op1 = read_w(ir, rn, ZERO_REG);
        op2 = read_w(ir, rm, ZERO_REG);
        if (is_sub)
            op2_inverted = ir->add_xor_32(ir, op2, mk_32(ir, ~0));
        else
            op2_inverted = op2;
        res = ir->add_add_32(ir,
                             ir->add_add_32(ir, op1, op2_inverted),
                             carry);
    }

    /* set flags */
    if (S) {
        if (is_64)
            write_nzcv(ir, mk_next_nzcv_64(ir, OPS_ADC, op1, op2_inverted));
        else
            write_nzcv(ir, mk_next_nzcv_32(ir, OPS_ADC, op1, op2_inverted));
    }

    /* write res */
    if (is_64)
        write_x(ir, rd, res, ZERO_REG);
    else
        write_w(ir, rd, res, ZERO_REG);

    return 0;
}

static int dis_mvni(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
    return 0;
}

static int dis_movi(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int Q = INSN(30, 30);
    int op = INSN(29, 29);
    int imm8 = (INSN(18,16) << 5) + INSN(9,5);
    int cmode = INSN(15,12);
    int rd = INSN(4, 0);
    struct irRegister *imm64 = mk_64(ir, simd_immediate(op, cmode, imm8));

    if (Q) {
        write_v_msb(ir, rd, imm64);
        write_v_lsb(ir, rd, imm64);
    } else
        write_d(ir, rd, imm64);

    return 0;
}

/* disassemblers */
static int dis_load_store_exclusive(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int o2 = INSN(23,23);
    int o1 = INSN(21,21);
    int o0 = INSN(15,15);
    int l = INSN(22,22);

    if (o0 == 1 && o1 == 0 && o2 == 0 && l == 1) {
        isExit = dis_load_aquire_exclusive(context, insn, ir);
    } else if (o0 == 1 && o1 == 1 && o2 == 0 && l == 1) {
        isExit = dis_load_aquire_exclusive_pair(context, insn, ir);
    } else if (o0 == 0 && o1 == 1 && o2 == 0 && l == 1) {
        isExit = dis_load_exclusive_pair(context, insn, ir);
    } else if (o0 == 1 &&  o1 == 0 && o2 == 1 && l == 1) {
        isExit = dis_load_aquire(context, insn, ir);
    } else if (o0 == 0 &&  o1 == 0 && o2 == 0 && l == 1) {
        isExit = dis_load_exclusive(context, insn, ir);
    } else if (o0 == 0 && o1 == 0 && o2 == 0 && l == 0) {
        isExit = dis_store_exclusive(context, insn, ir);
    } else if (o0 == 1 && o1 == 0 && o2 == 0 && l == 0) {
        isExit = dis_store_release_exclusive(context, insn, ir);
    } else if (o0 == 1 &&  o1 == 0 && o2 == 1 && l == 0) {
        isExit = dis_store_release(context, insn, ir);
    } else if (o0 == 0 &&  o1 == 1 && o2 == 0 && l == 0) {
        isExit = dis_store_exclusive_pair(context, insn, ir);
    } else if (o0 == 1 &&  o1 == 1 && o2 == 0 && l == 0) {
        isExit = dis_store_release_exclusive_pair(context, insn, ir);
    } else
        fatal("pc = 0x%08x / o0=%d / o1=%d / o2=%d / l =%d\n", context->pc, o0, o1, o2, l);

    return isExit;
}

static int dis_load_store_insn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op_29__28 = INSN(29, 28);

    switch(op_29__28) {
        case 0:
            if (INSN(28, 24) == 0x08) {
                isExit = dis_load_store_exclusive(context, insn, ir);
            } else {
                int op_24__23 = INSN(24, 23);

                switch(op_24__23) {
                    case 0:
                        isExit = dis_load_store_multiple_structure(context, insn, ir);
                        break;
                    case 1:
                        isExit = dis_load_store_multiple_structure_post_index(context, insn, ir);
                        break;
                    case 2:
                        isExit = dis_load_store_single_structure(context, insn, ir);
                        break;
                    case 3:
                        isExit = dis_load_store_single_structure_post_index(context, insn, ir);
                        break;
                }
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
                        if (INSN(26, 26))
                            assert(0 && "dis_load_store_no_allocate_pair_offset_simd_vfp");
                        else
                            isExit = dis_load_store_no_allocate_pair_offset(context, insn, ir);
                        break;
                    case 1:
                        if (INSN(26, 26))
                            isExit = dis_load_store_pair_post_indexed_simd_vfp(context, insn, ir);
                        else
                            isExit = dis_load_store_pair_post_indexed(context, insn, ir);
                        break;
                    case 2:
                        if (INSN(26,26))
                            isExit = dis_load_store_pair_offset_simd_vfp(context, insn, ir);
                        else
                            isExit = dis_load_store_pair_offset(context, insn, ir);
                        break;
                    case 3:
                        if (INSN(26, 26))
                            isExit = dis_load_store_pair_pre_indexed_simd_vfp(context, insn, ir);
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
                        if (INSN(26,26))
                            isExit = dis_load_store_register_unscaled_immediate_simd(context, insn, ir);
                        else
                            isExit = dis_load_store_register_unscaled_immediate(context, insn, ir);
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
                            isExit = dis_load_store_register_unprivileged(context, insn, ir);
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
            isExit = dis_extr(context, insn, ir);
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

static int dis_data_processing_1_source(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31,31);
    int isExit = 0;
    int opcode = INSN(15,10);

    switch(opcode) {
        case 0://rbit
            isExit = dis_rbit(context, insn, ir);
            break;
        case 1://rev16
            isExit = dis_rev16(context, insn, ir);
            break;
        case 2://rev
            if (is_64)
                isExit = dis_rev32(context, insn, ir);
            else
                isExit = dis_rev(context, insn, ir);
            break;
        case 3://rev
            isExit = dis_rev(context, insn, ir);
            break;
        case 4://clz
            isExit = dis_clz(context, insn, ir);
            break;
        case 5://cls
            isExit = dis_cls(context, insn, ir);
            break;
        default:
            fatal("opcode = %d(%x)\n", opcode, opcode);
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
        case 3:
            isExit = dis_sdiv(context, insn, ir);
            break;
        case 8:
            isExit = dis_lslv(context, insn, ir);
            break;
        case 9:
            isExit = dis_lsrv(context, insn, ir);
            break;
        case 10:
            isExit = dis_asrv(context, insn, ir);
            break;
        case 11:
            isExit = dis_rorv(context, insn, ir);
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
        case 1:
            isExit = dis_msub(context, insn, ir);
            break;
        case 2:
            isExit = dis_smaddl(context, insn, ir);
            break;
        case 3:
            isExit = dis_smsubl(context, insn, ir);
            break;
        case 4:
            isExit = dis_smulh(context, insn, ir);
            break;
        case 10:
            isExit = dis_umaddl(context, insn, ir);
            break;
        case 11:
            isExit = dis_umsubl(context, insn, ir);
            break;
        case 12:
            isExit = dis_umulh(context, insn, ir);
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
                    isExit = dis_add_substract_with_carry(context, insn, ir);
                    break;
                case 2:
                    if (INSN(11,11))
                        isExit = dis_ccmp_A_ccmn_immediate(context, insn, ir);
                    else
                        isExit = dis_ccmp_A_ccmn_register(context, insn, ir);
                    break;
                case 4:
                    isExit = dis_conditional_select(context, insn, ir);
                    break;
                case 6:
                    if (INSN(30,30))
                        isExit = dis_data_processing_1_source(context, insn, ir);
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
    int crn = INSN(15,12);
    int op2 = INSN(7,5);

    if (l) {
        if (op0 == 2 || op0 == 3)
            isExit = dis_mrs_register(context, insn, ir);
        else
            assert(0);
    } else {
        switch(op0) {
            case 0:
                if (crn == 3) {
                    switch(op2) {
                        case 2:
                            isExit = dis_clrex(context, insn, ir);
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
                            fatal("op2 = %d\n", op2);
                    }
                } else if (crn == 2) {
                    ;//hints are handled like nop
                } else
                    fatal("pc = 0x%016lx / crn = %d\n", context->pc, crn);
                break;
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

static int dis_floating_point_data_processing_1_source(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int opcode = INSN(20,15);

    switch(opcode) {
        case 0:
            isExit = dis_fmov_register(context, insn, ir);
            break;
        default:
            fatal("opcode = %d(0x%x)\n", opcode, opcode);
    }

    return isExit;
}

static int dis_conversion_between_floating_point_and_integer_insn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int sf = INSN(31,31);
    int type = INSN(23,22);
    int rmode = INSN(20,19);
    int opcode = INSN(18,16);

    if (sf == 0 && type == 0 && rmode == 0 && opcode == 7) {
        isExit = dis_fmov(context, insn, ir);
    } else if (sf == 0 && type == 0 && rmode == 0 && opcode == 6) {
         isExit = dis_fmov(context, insn, ir);
    } else if (sf == 1 && type == 1 && rmode == 0 && opcode == 7) {
         isExit = dis_fmov(context, insn, ir);
    } else if (sf == 1 && type == 1 && rmode == 0 && opcode == 6) {
         isExit = dis_fmov(context, insn, ir);
    } else if (sf == 1 && type == 2 && rmode == 1 && opcode == 6) {
         isExit = dis_fmov(context, insn, ir);
    } else
        fatal("pc = 0x%016lx / sf=%d / type=%d / rmode=%d / opcode=%d\n", context->pc, sf, type, rmode, opcode);

    return isExit;
}

static int dis_advanced_simd_copy(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int op = INSN(29,29);
    int imm4 = INSN(14,11);

    if (op) {
        //ins_element
        assert(0);
    } else {
        switch(imm4) {
            case 1:
                isExit = dis_dup_general(context, insn, ir);
                break;
            case 7:
                isExit = dis_umov(context, insn, ir);
                break;
            default:
                fatal("imm4 = %d\n", imm4);
        }
    }

    return isExit;
}

static int dis_advanced_simd_three_same(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int u = INSN(29,29);
    int size = INSN(23,22);
    int opcode = INSN(15,11);

    switch(opcode) {
        case 3:
            if (u)
                assert(0);
            else {
                if (size == 2)
                    dis_orr_register_simd_vector(context, insn, ir);
                else if (size == 0)
                    dis_and_simd_vector(context, insn, ir);
                else
                    fatal("pc = 0x%016lx / size = %d\n", context->pc, size);
            }
            break;
        case 16:
            if (u)
                assert(0);//sub
            else
                isExit = dis_add_simd_vector(context, insn, ir);
            break;
        case 17:
            if (u)
                isExit = dis_cmpeq_register_simd_vector(context, insn, ir);
            else
                assert(0);
            break;
        case 23:
            if (u)
                assert(0);
            else
                isExit = dis_addp_simd_vector(context, insn, ir);
            break;
        default:
            fatal("opcode = %d(0x%x) / u=%d\n", opcode, opcode, u);
    }

    return isExit;
}

static int dis_advanced_simd_modified_immediate(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int cmode = INSN(15, 12);
    int op = INSN(29, 29);

    switch(cmode) {
            case 0: case 2: case 4: case 6:
            case 8: case 10:
            case 12 ... 13:
                if (op)
                    isExit = dis_mvni(context, insn, ir);
                else
                    isExit = dis_movi(context, insn, ir);
                break;
            case 14:
                isExit = dis_movi(context, insn, ir);
                break;
        default:
            fatal("cmode = %d(0x%x)\n", cmode, cmode);
    }

    return isExit;
}

static int dis_advanced_simd_two_reg_misc(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int u = INSN(29,29);
    int size = INSN(23,22);
    int opcode = INSN(16,12);

    switch(opcode) {
        case 9:
            if (u)
                assert(0);
            else
                isExit = dis_cmpeq_simd_vector(context, insn, ir);
            break;
        default:
            fatal("opcode = %d(0x%x)\n", opcode, opcode);
    }

    return isExit;
}

static int dis_advanced_simd(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;

    if (INSN(28,28) == 0) {
        if (INSN(24, 24) == 0) {
            if (INSN(10, 10) == 0) {
                if (INSN(21, 21) == 0) {
                    if (INSN(29,29) == 0) {
                        if(INSN(11,11) == 0) {
                            assert(0 && "decodeTlbTbx");
                        } else {
                            assert(0 && "decodeZipUzpTrn");
                        }
                    } else {
                        assert(0 && "decodeAdvSIMDExt");
                    }
                } else { //INSN(21, 21) == 1
                    if (INSN(11, 11) == 0) {
                        assert(0 && "decodeAdvSIMDThreeDifferent");
                    } else { //INSN(11, 11) == 1
                        if (INSN(20, 20) == 0) {
                            isExit = dis_advanced_simd_two_reg_misc(context, insn, ir);
                        } else { //INSN(20, 20) == 1
                            assert(0 && "decodeAdvSIMDAccrossLanes");
                        }
                    }
                }
            } else { //INSN(10,10) == 1
                if (INSN(21, 21) == 0) {
                    isExit = dis_advanced_simd_copy(context, insn, ir);
                } else {
                    isExit = dis_advanced_simd_three_same(context, insn, ir);
                }
            }
        } else { //INSN(24,24) == 1
            if (INSN(10,10) == 0) {
                //decodeAdvSIMDVectorXIndexedElement
                fatal("pc = 0x%016lx / insn=0x%08x\n", context->pc, insn);
            } else { //INSN(10,10) == 1
                if (INSN(22,19) == 0) {
                    isExit = dis_advanced_simd_modified_immediate(context, insn, ir);
                } else {
                    //decodeAdvSIMDShiftByImmediate
                    fatal("pc = 0x%016lx / insn=0x%08x\n", context->pc, insn);
                }
            }
        }
    } else { //INSN(28,28) == 1
        fatal("pc = 0x%016lx / insn=0x%08x\n", context->pc, insn);
    }

    return isExit;
}

static int dis_data_processing_simd_insn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;

    if (INSN(28,28)) {
        if (INSN(30,30)) {
            isExit = dis_advanced_simd(context, insn, ir);
        } else {
            if (INSN(24,24)) {
                //floating_point_data_processing_3_source
                assert(0);
            } else {
                if (INSN(21,21)) {
                    switch(INSN(11,10)) {
                        case 0:
                            if (INSN(12,12)) {
                                assert(0);//floating_point_immediate
                            } else if (INSN(13,13)) {
                                assert(0);//floating_point_compare
                            } else if (INSN(14,14)) {
                                isExit = dis_floating_point_data_processing_1_source(context, insn, ir);
                            } else {
                                isExit = dis_conversion_between_floating_point_and_integer_insn(context, insn, ir);
                            }
                            break;
                        case 1:
                            assert(0);//floating_point_conditional_compare
                            break;
                        case 2:
                            assert(0);//floating_point_data_processing_2_source
                            break;
                        case 3:
                            assert(0);//floating_point_conditional_select
                            break;
                    }
                } else {
                    //conversion_between_floating_point_and_fixed_point
                    assert(0);
                }
            }
        }
    } else {
        isExit = dis_advanced_simd(context, insn, ir);
    }

    return isExit;
}

static int disassemble_insn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int isBpReached = 0;

    if (insn == 0xd4200000) {
        struct irRegister *params[4] = {NULL, NULL, NULL, NULL};

        isBpReached = 1;
        write_pc(ir, ir->add_mov_const_64(ir, context->pc));
        ir->add_call_void(ir, "arm64_hlp_gdb_handle_breakpoint",
                          mk_64(ir, (uint64_t) arm64_hlp_gdb_handle_breakpoint),
                          params);

        insn = gdb_get_opcode(context->pc);
    }

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
        isExit = dis_data_processing_simd_insn(context, insn, ir);
    } else if (INSN(28, 25) == 15) {
        isExit = dis_data_processing_simd_insn(context, insn, ir);
    } else
        fatal("Unknow insn = 0x%08x\n", insn);

    if (isBpReached) {
        write_pc(ir, ir->add_mov_const_64(ir, context->pc + 4));
        ir->add_exit(ir, ir->add_mov_const_64(ir, context->pc + 4));
    }

    return isBpReached | isExit;
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

