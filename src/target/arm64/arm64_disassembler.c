#include <assert.h>

#include "target64.h"
#include "arm64_private.h"
#include "runtime.h"
#include "arm64_helpers.h"

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
        return mk_64(ir, 0);
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

/* op code generation */
static int dis_load_store_pair_pre_indexed(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
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
    write_x(ir, rn, address1, SP_REG);

    return 0;
}

static int dis_add_sub_immediate_insn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_64 = INSN(31, 31);
    int is_sub = INSN(30, 30);
    int S = INSN(29, 29);
    uint32_t shift = INSN(23, 22);
    int imm12 = INSN(21, 10);
    int rn = INSN(9, 5);
    int rd = INSN(4, 0);
    struct irRegister *res;

    assert(S == 0);
    assert(shift != 2 && shift != 3);
    if (shift == 1)
        imm12 = imm12 << 12;

    /* do the ops */
    if (is_sub) {
        if (is_64)
            res = ir->add_sub_64(ir, read_x(ir, rn, SP_REG), mk_64(ir, imm12));
        else
            res = ir->add_sub_32(ir, read_w(ir, rn, SP_REG), mk_32(ir, imm12));
    } else {
        if (is_64)
            res = ir->add_add_64(ir, read_x(ir, rn, SP_REG), mk_64(ir, imm12));
        else
            res = ir->add_add_32(ir, read_w(ir, rn, SP_REG), mk_32(ir, imm12));
    }

    /* write reg */
    if (is_64)
        write_x(ir, rd, res, S?ZERO_REG:SP_REG);
    else
        write_w(ir, rd, res, S?ZERO_REG:SP_REG);

    return 0;
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
    assert(0);
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
    assert(0);
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
        case 1://orr
            res_not_inverted = ir->add_or_64(ir, read_x(ir, rn, ZERO_REG), op2);
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

    write_pc(ir, ir->add_mov_const_64(ir, context->pc + 4));
    ir->add_exit(ir, ir->add_mov_const_64(ir, context->pc + 4));

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

/* disassemblers */
static int dis_load_store_register_unsigned_offset(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int size = INSN(31,30);
    int opc = INSN(23,22);
    int imm12 = INSN(21, 10);
    int rn = INSN(9, 5);
    int rt = INSN(4, 0);
    struct irRegister *address;
    struct irRegister *res;

    /* prfm is translated into nop */
    if (size == 3 && opc == 2)
        return 0;

    /* compute address */
    address = ir->add_add_64(ir, read_x(ir, rn, SP_REG), mk_64(ir, imm12 << size));

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

    return 0;
}

static int dis_load_store_register_unsigned_offset_simd(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    assert(0);
}

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
            fatal("load_literal\n");
            break;
        case 2:
            {
                int op_24__23 = INSN(24, 23);

                switch(op_24__23) {
                    case 0:
                        fatal("load_store_no_allocate_pair\n");
                        break;
                    case 1:
                        fatal("load_store_register_pair_post_indexed\n");
                        break;
                    case 2:
                        fatal("load_store_register_pair_offset\n");
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
                        fatal("load_store_register_immedaite_post_indexed\n");
                        break;
                    case 2:
                        if (INSN(21,21))
                            fatal("load_store_register_register_offset\n");
                        else
                            fatal("load_store_register_unprivileged\n");
                        break;
                    case 3:
                        fatal("load_store_register_immediate_pre_indexed\n");
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
            fatal("logical_immediate\n");
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
            fatal("bitfield\n");
            break;
        case 7:
            fatal("extract\n");
            break;
    }

    return isExit;
}

static int dis_data_processing_register_insn(struct arm64_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;

    if (INSN(28,28)) {
        if (INSN(24,24)) {
            fatal("data_processing_3_source\n");
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
                    fatal("cond_select\n");
                    break;
                case 6:
                    if (INSN(30,30))
                        fatal("data_processing_1_source\n");
                    else
                        fatal("data_processing_2_source\n");
                    break;
                default:
                    assert(0);
            }
        }
    } else {
        if (INSN(24,24)) {
            if (INSN(21,21))
                fatal("add_sub_extented_register\n");
            else
                fatal("add_sub_shifted_register\n");
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
        case 2:
            isExit = dis_ret(context, insn, ir);
            break;
        default:
            fatal("opc = %d\n", opc);
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
                fatal("test_and_branch_immediate\n");
            else
                fatal("compare_and_branch_immidiate\n");
            break;
        case 2:
            if (INSN(31,31)) {
                if (INSN(25,25)) {
                    isExit = dis_unconditionnal_branch_register(context, insn, ir);
                } else {
                    if (INSN(24,24))
                        fatal("system\n");
                    else
                        isExit = dis_exception_insn(context, insn, ir);
                }
            } else {
                fatal("conditionnal_branch_immediate\n");
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

