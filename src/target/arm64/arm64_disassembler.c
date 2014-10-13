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

static struct irRegister *read_reg(struct irInstructionAllocator *ir, int index, int is_zero)
{
    if (index == 31 && is_zero)
        return mk_64(ir, 0);
    else
        return ir->add_read_context_64(ir, offsetof(struct arm64_registers, r[index]));
}

static void write_reg(struct irInstructionAllocator *ir, int index, struct irRegister *value, int is_zero)
{
    if (index == 31 && is_zero)
        ;//do nothing
    else
        ir->add_write_context_64(ir, value, offsetof(struct arm64_registers, r[index]));
}

static struct irRegister *read_reg_32(struct irInstructionAllocator *ir, int index, int is_zero)
{
    if (index == 31 && is_zero)
        return mk_64(ir, 0);
    else
        return ir->add_read_context_32(ir, offsetof(struct arm64_registers, r[index]));
}

static void write_reg_32(struct irInstructionAllocator *ir, int index, struct irRegister *value, int is_zero)
{
    if (index == 31 && is_zero)
        ;//do nothing
    else
        ir->add_write_context_64(ir, ir->add_32U_to_64(ir, value), offsetof(struct arm64_registers, r[index]));
}

static void write_reg_32_sign_extend(struct irInstructionAllocator *ir, int index, struct irRegister *value, int is_zero)
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
    address1 = ir->add_add_64(ir, read_reg(ir, rn, SP_REG), mk_64(ir, offset));
    address2 = ir->add_add_64(ir, address1, mk_64(ir, is_64?8:4));

    /* read write reg */
    if (is_load) {
        if (is_64) {
            write_reg(ir, rt, ir->add_load_64(ir, address1), ZERO_REG);
            write_reg(ir, rt2, ir->add_load_64(ir, address2), ZERO_REG);
        } else {
            if (is_signed) {
                write_reg_32_sign_extend(ir, rt, ir->add_load_32(ir, address1), ZERO_REG);
                write_reg_32_sign_extend(ir, rt2, ir->add_load_32(ir, address2), ZERO_REG);
            } else {
                write_reg_32(ir, rt, ir->add_load_32(ir, address1), ZERO_REG);
                write_reg_32(ir, rt2, ir->add_load_32(ir, address2), ZERO_REG);
            }
        }
    } else {
        if (is_64) {
            ir->add_store_64(ir, read_reg(ir, rt, ZERO_REG), address1);
            ir->add_store_64(ir, read_reg(ir, rt2, ZERO_REG), address2);
        } else {
            ir->add_store_32(ir, read_reg_32(ir, rt, ZERO_REG), address1);
            ir->add_store_32(ir, read_reg_32(ir, rt2, ZERO_REG), address2);
        }
    }

    /* write back */
    write_reg(ir, rn, address1, SP_REG);

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
            fatal("load_store_register\n");
            break;
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
        //data_processing_immediate
        fatal("data_processing_immediate\n");
    } else if (INSN(28, 26) == 5) {
        //branch_A_exception_A_system
        fatal("branch_A_exception_A_system\n");
    } else if (INSN(27,27) == 1 && INSN(25, 25) == 0) {
        //load_store
        isExit = dis_load_store_insn(context, insn, ir);
    } else if (INSN(27, 25) == 5) {
        //data_processing_register
        fatal("data_processing_register\n");
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
        dump_state(context, ir);
        if (isExit)
            break;
    }
    if (!isExit) {
        //fixme
        write_pc(ir, ir->add_mov_const_64(ir, context->pc + 4));
        ir->add_exit(ir, ir->add_mov_const_64(ir, context->pc + 4));
    }
}

