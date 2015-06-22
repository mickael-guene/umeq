#if defined(COMMON_THUMB)
const static int is_thumb = 1;
#else
const static int is_thumb = 0;
#endif

/* utils */
static uint32_t vfpExpandImm32(int imm8)
{
    uint32_t i7 = (imm8 >> 7) & 1;
    uint32_t i6 = (imm8 >> 6) & 1;
    uint32_t i5_0 = imm8 & 0x3f;

    return (i7 << 31) + ((1 - i6) << 30) + (i6 << 29) + (i6 << 28) +
           (i6 << 27) + (i6 << 26) + (i6 << 25) + (i5_0 << 19);
}

static uint64_t vfpExpandImm64(int imm8)
{
    uint64_t i7 = (imm8 >> 7) & 1;
    uint64_t i6 = (imm8 >> 6) & 1;
    uint64_t i5_0 = imm8 & 0x3f;

    return (i7 << 63) + ((1 - i6) << 62) + (i6 << 61) + (i6 << 60) + (i6 << 59) +
           (i6 << 58) + (i6 << 57) + (i6 << 56) + (i6 << 55) + (i6 << 54) +
           (i5_0 << 48);
}

static uint64_t advSimdExpandImm(int op, int cmode, uint32_t _imm8)
{
    uint64_t res;
    uint64_t imm8 = _imm8;

    switch(cmode) {
        case 0 ... 1:
            res = (imm8 << 32) | (imm8 << 0);
            break;
        case 2 ... 3:
            res = (imm8 << 40) | (imm8 << 8);
            break;
        case 4 ... 5:
            res = (imm8 << 48) | (imm8 << 16);
            break;
        case 6 ... 7:
            res = (imm8 << 56) | (imm8 << 24);
            break;
        case 8 ... 9:
            res = (imm8 << 48) | (imm8 << 32) | (imm8 << 16) | imm8;
            break;
        case 10 ... 11:
            res = (imm8 << 56) | (imm8 << 40) | (imm8 << 24) | (imm8 << 8);
            break;
        case 12:
            res = (imm8 << 40) | (0xffUL << 32) | (imm8 << 8) | 0xff;
            break;
        case 13:
            res = (imm8 << 48) | (0xffffUL << 32) | (imm8 << 16) | 0xffff;
            break;
        case 14:
            if (op) {
                int i;
                res = 0;
                for(i = 0; i < 8; i++) {
                    res = res << 8;
                    if (imm8 & 0x80)
                        res = res | 0xff;
                    imm8 = imm8 << 1;
                }
            } else
                res = (imm8 << 56) | (imm8 << 48) | (imm8 << 40) | (imm8 << 32) |
                      (imm8 << 24) | (imm8 << 16) | (imm8 << 8) | (imm8 << 0);
            break;
        case 15:
            {
                uint64_t f32 = vfpExpandImm32(_imm8);

                res = (f32 << 32) | f32;
            }
            break;
        default:
            fatal("cmode = %d\n", cmode);
    }

    return res;
}

static struct irRegister *read_fpscr(struct arm_target *context, struct irInstructionAllocator *ir)
{
    return ir->add_read_context_32(ir, offsetof(struct arm_registers, fpscr));
}

static void write_fpscr(struct arm_target *context, struct irInstructionAllocator *ir, struct irRegister *value)
{
    ir->add_write_context_32(ir, value, offsetof(struct arm_registers, fpscr));
}

/* ir generation */
static int dis_common_vldm(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = mk_32(ir, is_thumb);
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "hlp_arm_vldm",
                        ir->add_mov_const_64(ir, (uint64_t) hlp_arm_vldm),
                        params);

    return 0;
}

static int dis_common_vstm(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = mk_32(ir, is_thumb);
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "hlp_arm_vstm",
                        ir->add_mov_const_64(ir, (uint64_t) hlp_arm_vstm),
                        params);

    return 0;
}

static int dis_common_vldr(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_add = INSN(23, 23);
    int D = INSN(22, 22);
    int rn = INSN(19, 16);
    int vd = INSN(15, 12);
    uint32_t imm32 = INSN(7, 0) << 2;
    int is_64 = INSN(8, 8);
    struct irRegister *address;
    int d;

#if defined(COMMON_THUMB)
    if (rn == 15)
        address = ir->add_add_32(ir, mk_32(ir, ((context->pc + 4) & ~3)),
                                     mk_32(ir, is_add?imm32:-imm32));
    else
        address = ir->add_add_32(ir, read_reg(context, ir, rn),
                                     mk_32(ir, is_add?imm32:-imm32));
#else
    address = ir->add_add_32(ir, read_reg(context, ir, rn),
                                 mk_32(ir, is_add?imm32:-imm32));
#endif
    if (is_64) {
        d = (D << 4) + vd;
        write_reg_d(context, ir, d, ir->add_load_64(ir, mk_address(ir, address)));
    } else {
        d = (vd << 1) + D;
        write_reg_s(context, ir, d, ir->add_load_32(ir, mk_address(ir, address)));
    }

    return 0;
}

static int dis_common_vstr(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_add = INSN(23, 23);
    int D = INSN(22, 22);
    int rn = INSN(19, 16);
    int vd = INSN(15, 12);
    uint32_t imm32 = INSN(7, 0) << 2;
    int is_64 = INSN(8, 8);
    int isExit = 0;
    struct irRegister *address;
    int d;

    /* FIXME: not tested so disable it. handle arm/thumb differences */
    assert(rn != 15);
    address = ir->add_add_32(ir, read_reg(context, ir, rn),
                             mk_32(ir, is_add?imm32:-imm32));
    if (is_64) {
        d = (D << 4) + vd;
        ir->add_store_64(ir, read_reg_d(context, ir, d), mk_address(ir, address));
    } else {
        d = (vd << 1) + D;
        ir->add_store_32(ir, read_reg_s(context, ir, d), mk_address(ir, address));
    }

    return isExit;
}

static int dis_common_vmov_arm_core_and_s_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int vn = (INSN(19, 16) << 1) | INSN(7, 7);
    int rt = INSN(15, 12);
    int is_to_arm_registers = INSN(20, 20);

    assert(rt != 15);
    if (is_to_arm_registers) {
        write_reg(context, ir, rt, read_reg_s(context, ir, vn));
    } else {
        write_reg_s(context, ir, vn, read_reg(context, ir, rt));
    }

    return 0;
}

static int dis_common_vmov_scalar_to_arm(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int vn = (INSN(7, 7) << 4) | INSN(19, 16);
    int rt = INSN(15, 12);
    int opc1 = INSN(22, 21);
    int opc2 = INSN(6, 5);
    int is_unsigned = INSN(23, 23);
    struct irRegister *res;
    int index;

    /* we read directly the register part we need. This is the most efficient but register accesss
       is partly 'hidden' */
    if (opc1 & 2) {
        index = ((opc1 & 1) << 2) + opc2;
        if (is_unsigned) {
            res = ir->add_8U_to_32(ir, ir->add_read_context_8(ir, offsetof(struct arm_registers, e.d[vn]) + index));
        } else {
            res = ir->add_8S_to_32(ir, ir->add_read_context_8(ir, offsetof(struct arm_registers, e.d[vn]) + index));
        }
    } else if (opc2 & 1) {
        index = ((opc1 & 1) << 1) + (opc2 >> 1);
        if (is_unsigned) {
            res = ir->add_16U_to_32(ir, ir->add_read_context_16(ir, offsetof(struct arm_registers, e.d[vn]) + index * 2));
        } else {
            res = ir->add_16S_to_32(ir, ir->add_read_context_16(ir, offsetof(struct arm_registers, e.d[vn]) + index * 2));
        }
    } else {
        index = opc1 & 1;
        res = ir->add_read_context_32(ir, offsetof(struct arm_registers, e.d[vn]) + index * 4);
    }

    write_reg(context, ir, rt, res);

    return 0;
}

static int dis_common_vmov_register(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_vfp = INSN(23, 23);
    int D = INSN(22, 22);
    int vd = INSN(15, 12);
    int M = INSN(5, 5);
    int vm = INSN(3, 0);
    int d;
    int m;

    if (is_vfp) {
        int sz = INSN(8, 8);

        if (sz) {
            d = (D << 4) + vd;
            m = (M << 4) + vm;

            write_reg_d(context, ir, d, read_reg_d(context, ir, m));
        } else {
            d = (vd << 1) + D;
            m = (vm << 1) + M;

            write_reg_s(context, ir, d, read_reg_s(context, ir, m));
        }
    } else {
        assert(0);
    }

    return 0;
}

static int dis_common_vmov_immediate(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int D = INSN(22, 22);
    int vd = INSN(15, 12);
    int is_64 = INSN(8, 8);
    int imm8 = (INSN(19, 16) << 4) + INSN(3, 0);
    int d;

    if (is_64) {
        uint64_t imm64 = vfpExpandImm64(imm8);
        d = (D << 4) + vd;

        write_reg_d(context, ir, d, mk_64(ir, imm64));
    } else {
        uint32_t imm32 = vfpExpandImm32(imm8);
        d = (vd << 1) + D;

        write_reg_s(context, ir, d, mk_32(ir, imm32));
    }

    return 0;
}

static int dis_common_vmov_two_arm_core_and_two_s_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int is_to_arm_registers = INSN(20, 20);
    int rt2 = INSN(19, 16);
    int rt = INSN(15, 12);
    int m = (INSN(3, 0) << 1) + INSN(5, 5);

    if (is_to_arm_registers) {
        write_reg(context, ir, rt, read_reg_s(context, ir, m));
        write_reg(context, ir, rt2, read_reg_s(context, ir, m + 1));
    } else {
        write_reg_s(context, ir, m, read_reg(context, ir, rt));
        write_reg_s(context, ir, m + 1, read_reg(context, ir, rt2));
    }

    return 0;
}

static int dis_common_vmov_two_arm_core_and_d_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rt2 = INSN(19, 16);
    int rt = INSN(15, 12);
    int is_to_arm_registers = INSN(20, 20);
    int m = (INSN(5, 5) << 4) + INSN(3, 0);

    assert(rt != 15);
    assert(rt2 != 15);
    if (is_to_arm_registers) {
        write_reg(context, ir, rt, read_reg_s(context, ir, m * 2));
        write_reg(context, ir, rt2, read_reg_s(context, ir, m * 2 + 1));
    } else {
        write_reg_s(context, ir, m * 2, read_reg(context, ir, rt));
        write_reg_s(context, ir, m * 2 + 1, read_reg(context, ir, rt2));
    }

    return 0;
}

static int dis_common_veor_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int Q = INSN(6, 6);
    int d = (INSN(22, 22) << 4) + INSN(15, 12);
    int n = (INSN(7, 7) << 4) + INSN(19, 16);
    int m = (INSN(5, 5) << 4) + INSN(3, 0);

    write_reg_d(context, ir, d, ir->add_xor_64(ir,
                                               read_reg_d(context, ir, n),
                                               read_reg_d(context, ir, m)));
    if (Q) {
        write_reg_d(context, ir,  d + 1, ir->add_xor_64(ir,
                                                        read_reg_d(context, ir, n + 1),
                                                        read_reg_d(context, ir, m + 1)));
    }

    return 0;
}

static int dis_common_vorr_immediate_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int Q = INSN(6, 6);
    int d = (INSN(22, 22) << 4) + INSN(15, 12);
    int cmode = INSN(11, 8);
    int i = is_thumb?INSN(28, 28):INSN(24, 24);
    uint32_t imm8 = (i << 7) | (INSN(18, 16) << 4) | INSN(3, 0);
    uint64_t imm64 = advSimdExpandImm(1, cmode, imm8);

    write_reg_d(context, ir, d, ir->add_or_64(ir,
                                              read_reg_d(context, ir, d),
                                              mk_64(ir, imm64)));
    if (Q)
        write_reg_d(context, ir, d + 1, ir->add_or_64(ir,
                                                      read_reg_d(context, ir, d + 1),
                                                      mk_64(ir, imm64)));

    return 0;
}

static int dis_common_vbic_immediate_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int Q = INSN(6, 6);
    int d = (INSN(22, 22) << 4) + INSN(15, 12);
    int cmode = INSN(11, 8);
    int i = is_thumb?INSN(28, 28):INSN(24, 24);
    uint32_t imm8 = (i << 7) | (INSN(18, 16) << 4) | INSN(3, 0);
    uint64_t imm64 = advSimdExpandImm(1, cmode, imm8);

    write_reg_d(context, ir, d, ir->add_and_64(ir,
                                               read_reg_d(context, ir, d),
                                               mk_64(ir, ~imm64)));
    if (Q)
        write_reg_d(context, ir, d + 1, ir->add_and_64(ir,
                                                       read_reg_d(context, ir, d + 1),
                                                       mk_64(ir, ~imm64)));

    return 0;
}

static int dis_common_vmov_register_simd_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int Q = INSN(6, 6);
    int d = (INSN(22, 22) << 4) + INSN(15, 12);
    int m = (INSN(5, 5) << 4) + INSN(3, 0);

    write_reg_d(context, ir, d, read_reg_d(context, ir, m));
    if (Q) {
        write_reg_d(context, ir, d + 1, read_reg_d(context, ir, m + 1));
    }

    return 0;
}

static int dis_common_vorr_register_simd_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int Q = INSN(6, 6);
    int d = (INSN(22, 22) << 4) + INSN(15, 12);
    int n = (INSN(7, 7) << 4) + INSN(19, 16);
    int m = (INSN(5, 5) << 4) + INSN(3, 0);

    write_reg_d(context, ir, d, ir->add_or_64(ir,
                                              read_reg_d(context, ir, n),
                                              read_reg_d(context, ir, m)));
    if (Q) {
        write_reg_d(context, ir,  d + 1, ir->add_or_64(ir,
                                                       read_reg_d(context, ir, n + 1),
                                                       read_reg_d(context, ir, m + 1)));
    }

    return 0;
}

static int dis_common_vmvn_register_simd_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int Q = INSN(6, 6);
    int d = (INSN(22, 22) << 4) + INSN(15, 12);
    int m = (INSN(5, 5) << 4) + INSN(3, 0);

    write_reg_d(context, ir, d, ir->add_xor_64(ir, read_reg_d(context, ir, m), mk_64(ir, ~0)));
    if (Q) {
        write_reg_d(context, ir, d + 1, ir->add_xor_64(ir, read_reg_d(context, ir, m + 1), mk_64(ir, ~0)));
    }

    return 0;
}

static int dis_common_vswap_simd_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int Q = INSN(6, 6);
    int d = (INSN(22, 22) << 4) + INSN(15, 12);
    int m = (INSN(5, 5) << 4) + INSN(3, 0);
    struct irRegister *tmp;

    tmp = read_reg_d(context, ir, d);
    write_reg_d(context, ir, d, read_reg_d(context, ir, m));
    write_reg_d(context, ir, m, tmp);
    if (Q) {
        tmp = read_reg_d(context, ir, d + 1);
        write_reg_d(context, ir, d + 1, read_reg_d(context, ir, m + 1));
        write_reg_d(context, ir, m + 1, tmp);
    }

    return 0;
}

static int dis_common_vmov_immediate_simd_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int Q = INSN(6, 6);
    int d = (INSN(22, 22) << 4) + INSN(15, 12);
    int cmode = INSN(11, 8);
    int i = is_thumb?INSN(28, 28):INSN(24, 24);
    uint32_t imm8 = (i << 7) | (INSN(18, 16) << 4) | INSN(3, 0);
    int op = INSN(5, 5);
    uint64_t imm64 = advSimdExpandImm(op, cmode, imm8);

    write_reg_d(context, ir, d, mk_64(ir, imm64));
    if (Q)
        write_reg_d(context, ir, d + 1, mk_64(ir, imm64));

    return 0;
}

static int dis_common_vmvn_immediate_simd_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int Q = INSN(6, 6);
    int d = (INSN(22, 22) << 4) + INSN(15, 12);
    int cmode = INSN(11, 8);
    int i = is_thumb?INSN(28, 28):INSN(24, 24);
    uint32_t imm8 = (i << 7) | (INSN(18, 16) << 4) | INSN(3, 0);
    int op = INSN(5, 5);
    uint64_t imm64 = advSimdExpandImm(op, cmode, imm8);

    write_reg_d(context, ir, d, mk_64(ir, ~imm64));
    if (Q)
        write_reg_d(context, ir, d + 1, mk_64(ir, ~imm64));

    return 0;
}

static int dis_common_vand_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int Q = INSN(6, 6);
    int d = (INSN(22, 22) << 4) + INSN(15, 12);
    int n = (INSN(7, 7) << 4) + INSN(19, 16);
    int m = (INSN(5, 5) << 4) + INSN(3, 0);

    write_reg_d(context, ir, d, ir->add_and_64(ir,
                                               read_reg_d(context, ir, n),
                                               read_reg_d(context, ir, m)));
    if (Q) {
        write_reg_d(context, ir,  d + 1, ir->add_and_64(ir,
                                                        read_reg_d(context, ir, n + 1),
                                                        read_reg_d(context, ir, m + 1)));
    }

    return 0;
}

static int dis_common_vbic_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int Q = INSN(6, 6);
    int d = (INSN(22, 22) << 4) + INSN(15, 12);
    int n = (INSN(7, 7) << 4) + INSN(19, 16);
    int m = (INSN(5, 5) << 4) + INSN(3, 0);

    write_reg_d(context, ir, d, ir->add_and_64(ir,
                                               read_reg_d(context, ir, n),
                                               ir->add_xor_64(ir, read_reg_d(context, ir, m), mk_64(ir, ~0UL))));
    if (Q) {
        write_reg_d(context, ir,  d + 1, ir->add_and_64(ir,
                                                        read_reg_d(context, ir, n + 1),
                                                        ir->add_xor_64(ir, read_reg_d(context, ir, m + 1), mk_64(ir, ~0UL))));
    }

    return 0;
}

static int dis_common_vbif_vbit_vbsl(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int Q = INSN(6, 6);
    int d = (INSN(22, 22) << 4) + INSN(15, 12);
    int n = (INSN(7, 7) << 4) + INSN(19, 16);
    int m = (INSN(5, 5) << 4) + INSN(3, 0);
    int op = INSN(21, 20);
    int op1, op2, op3;

    switch(op) {
        case 1:
            op1 = n;
            op2 = d;
            op3 = m;
            break;
        case 2:
            op1 = n;
            op2 = m;
            op3 = d;
            break;
        case 3:
            op1 = d;
            op2 = m;
            op3 = n;
            break;
        default:
            assert(0);
    }
    write_reg_d(context, ir, d, ir->add_or_64(ir,
                                              ir->add_and_64(ir, read_reg_d(context, ir, op1), read_reg_d(context, ir, op2)),
                                              ir->add_and_64(ir,
                                                             read_reg_d(context, ir, op3),
                                                             ir->add_xor_64(ir, read_reg_d(context, ir, op2), mk_64(ir, ~0UL)))));
    if (Q) {
    write_reg_d(context, ir, d + 1, ir->add_or_64(ir,
                                                  ir->add_and_64(ir, read_reg_d(context, ir, op1 + 1), read_reg_d(context, ir, op2 + 1)),
                                                  ir->add_and_64(ir,
                                                                 read_reg_d(context, ir, op3 + 1),
                                                                 ir->add_xor_64(ir, read_reg_d(context, ir, op2 + 1), mk_64(ir, ~0UL)))));
    }

    return 0;
}

static int dis_common_vorn_simd_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int Q = INSN(6, 6);
    int d = (INSN(22, 22) << 4) + INSN(15, 12);
    int n = (INSN(7, 7) << 4) + INSN(19, 16);
    int m = (INSN(5, 5) << 4) + INSN(3, 0);

    write_reg_d(context, ir, d, ir->add_or_64(ir,
                                               read_reg_d(context, ir, n),
                                               ir->add_xor_64(ir, read_reg_d(context, ir, m), mk_64(ir, ~0))));
    if (Q) {
        write_reg_d(context, ir,  d + 1, ir->add_or_64(ir,
                                                        read_reg_d(context, ir, n + 1),
                                                        ir->add_xor_64(ir, read_reg_d(context, ir, m + 1), mk_64(ir, ~0))));
    }

    return 0;
}

static int dis_common_adv_simd_three_different_length_hlp(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = mk_32(ir, is_thumb);
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "hlp_common_adv_simd_three_different_length",
                        ir->add_mov_const_64(ir, (uint64_t) hlp_common_adv_simd_three_different_length),
                        params);

    return 0;
}

static int dis_common_adv_simd_three_same_length_hlp(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = mk_32(ir, is_thumb);
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "hlp_common_adv_simd_three_same_length",
                        ir->add_mov_const_64(ir, (uint64_t) hlp_common_adv_simd_three_same_length),
                        params);

    return 0;
}

static int dis_common_adv_simd_two_regs_misc_hlp(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "hlp_common_adv_simd_two_regs_misc",
                        ir->add_mov_const_64(ir, (uint64_t) hlp_common_adv_simd_two_regs_misc),
                        params);

    return 0;
}

static int dis_common_vmsr(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rt = INSN(15, 12);

    write_fpscr(context, ir, read_reg(context, ir, rt));

    return 0;
}

static int dis_common_vmrs(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int rt = INSN(15, 12);

    if (rt == 15) {
        struct irRegister *new_cpsr;

        new_cpsr = ir->add_or_32(ir,
                                 ir->add_and_32(ir, read_cpsr(context, ir), mk_32(ir, 0x0fffffff)),
                                 ir->add_and_32(ir, read_fpscr(context, ir), mk_32(ir, 0xf0000000)));
        write_cpsr(context, ir, new_cpsr);
    } else {
        write_reg(context, ir, rt, read_fpscr(context, ir));
    }

    return 0;
}

static int dis_common_adv_simd_vdup_arm_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "hlp_common_adv_simd_vdup_arm",
                        ir->add_mov_const_64(ir, (uint64_t) hlp_common_adv_simd_vdup_arm),
                        params);

    return 0;
}

static int dis_common_adv_simd_vmov_from_arm_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "hlp_common_adv_simd_vmov_from_arm",
                        ir->add_mov_const_64(ir, (uint64_t) hlp_common_adv_simd_vmov_from_arm),
                        params);

    return 0;
}

/* disassembler */
static int dis_common_extension_register_load_store_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int isExit = 0;
    int opcode = INSN(24, 21);
    int is_load = INSN(20, 20);

    if (opcode == 2) {
        fatal("64 bits transfers between arm core and extension registers\n");
    } else if (is_load) {
        if ((opcode & 0x9) == 8) {
            isExit = dis_common_vldr(context, insn, ir);
        } else {
            isExit = dis_common_vldm(context, insn, ir);
        }
    } else {
        if ((opcode & 0x9) == 8) {
            isExit = dis_common_vstr(context, insn, ir);
        } else {
            isExit = dis_common_vstm(context, insn, ir);
        }
    }

    return isExit;
}

static int dis_common_transfert_arm_core_and_extension_registers_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int lc = (INSN(20, 20) << 1) | INSN(8, 8);
    int a = INSN(23, 21);
    int isExit = 0;

    switch(lc) {
        case 0:
            if (a)
                isExit = dis_common_vmsr(context, insn, ir);
            else
                isExit = dis_common_vmov_arm_core_and_s_insn(context, insn, ir);
            break;
        case 1:
            if (a&4)
                isExit = dis_common_adv_simd_vdup_arm_insn(context, insn, ir);
            else
                isExit = dis_common_adv_simd_vmov_from_arm_insn(context, insn, ir);
            break;
        case 2:
            if (a)
                isExit = dis_common_vmrs(context, insn, ir);
            else
                isExit = dis_common_vmov_arm_core_and_s_insn(context, insn, ir);
            break;
        case 3:
            isExit = dis_common_vmov_scalar_to_arm(context, insn, ir);
            break;
        default:
            fatal("lc = %d / a = %d\n", lc, a);
    }

    return isExit;
}

static int mk_common_vfp_data_processing_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "hlp_common_vfp_data_processing_insn",
                           ir->add_mov_const_64(ir, (uint64_t) hlp_common_vfp_data_processing_insn),
                           params);

    return 0;
}

static int dis_common_vfp_data_processing_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int opc1 = INSN(23, 20);
    int opc2 = INSN(19, 16);
    int opc3_1 = INSN(7, 7);
    int opc3_0 = INSN(6, 6);
    int isExit = 0;

    switch(opc1) {
        case 0: case 4:
            isExit = mk_common_vfp_data_processing_insn(context, insn, ir);
            break;
        case 1: case 5:
            isExit = mk_common_vfp_data_processing_insn(context, insn, ir);
            break;
        case 2: case 6:
            isExit = mk_common_vfp_data_processing_insn(context, insn, ir);
            break;
        case 3: case 7:
            isExit = mk_common_vfp_data_processing_insn(context, insn, ir);
            break;
        case 8: case 12:
            isExit = mk_common_vfp_data_processing_insn(context, insn, ir);
            break;
        case 11: case 15:
            if (opc3_0) {
                switch(opc2) {
                    case 0:
                        if (opc3_1)
                            isExit = mk_common_vfp_data_processing_insn(context, insn, ir);
                        else
                            isExit = dis_common_vmov_register(context, insn, ir);
                        break;
                    case 1:
                        isExit = mk_common_vfp_data_processing_insn(context, insn, ir);
                        break;
                    case 4: case 5:
                        isExit = mk_common_vfp_data_processing_insn(context, insn, ir);
                        break;
                    case 7:
                        isExit = mk_common_vfp_data_processing_insn(context, insn, ir);
                        break;
                    case 8: case 12: case 13:
                        isExit = mk_common_vfp_data_processing_insn(context, insn, ir);
                        break;
                    default:
                        fatal("opc2 = %d(0x%x)\n", opc2, opc2);
                }
            } else {
                isExit = dis_common_vmov_immediate(context, insn, ir);
            }
            break;
        default:
            fprintf(stderr, "pc = 0x%08x\n", context->pc);
            fatal("opc1 = %d(0x%x)\n", opc1, opc1);
    }

    return isExit;
}

static int dis_common_adv_simd_three_different_length_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    return dis_common_adv_simd_three_different_length_hlp(context, insn, ir);
}

static int dis_common_adv_simd_two_regs_and_scalar_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = mk_32(ir, is_thumb);
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "hlp_common_adv_simd_two_regs_and_scalar",
                        ir->add_mov_const_64(ir, (uint64_t) hlp_common_adv_simd_two_regs_and_scalar),
                        params);

    return 0;
}

static int dis_common_adv_simd_three_same_length_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int a = INSN(11, 8);
    int b = INSN(4, 4);
    int c = INSN(21, 20);
    int u = is_thumb?INSN(28, 28):INSN(24, 24);
    int isExit = 0;

    switch(a) {
        case 1:
            if (b) {
                switch(c) {
                    case 0:
                        isExit = u?dis_common_veor_insn(context, insn, ir):dis_common_vand_insn(context, insn, ir);
                        break;
                    case 1:
                        isExit = u?dis_common_vbif_vbit_vbsl(context, insn, ir):dis_common_vbic_insn(context, insn, ir);
                        break;
                    case 2:
                        if (u) {
                            isExit = dis_common_vbif_vbit_vbsl(context, insn, ir);
                        } else {
                            if (INSN(19, 16) == INSN(3, 0) && INSN(7, 7) == INSN(5, 5))
                                dis_common_vmov_register_simd_insn(context, insn, ir);
                            else
                                dis_common_vorr_register_simd_insn(context, insn, ir);
                        }
                        break;
                    case 3:
                        isExit = u?dis_common_vbif_vbit_vbsl(context, insn, ir):dis_common_vorn_simd_insn(context, insn, ir);
                        break;
                    default:
                        fatal("c = %d(0x%x)\n", c, c);
                }
            } else {
                /* vrhadd */
                isExit = dis_common_adv_simd_three_same_length_hlp(context, insn, ir);
            }
            break;
        case 0:
        case 2:
        case 3:
        case 4:
        case 5:
        case 6:
        case 7:
        case 8:
        case 9:
        case 10:
        case 11:
        case 13:
        case 14:
        case 15:
            isExit = dis_common_adv_simd_three_same_length_hlp(context, insn, ir);
            break;
        default:
            fatal("a = %d(0x%x)\n", a, a);
    }

    return isExit;
}

static int dis_common_adv_simd_vdup_scalar_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "hlp_common_adv_simd_vdup_scalar",
                        ir->add_mov_const_64(ir, (uint64_t) hlp_common_adv_simd_vdup_scalar),
                        params);

    return 0;
}

static int dis_common_adv_simd_two_regs_misc_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int a = INSN(17, 16);
    int b = INSN(10, 6);

    if (a == 0 && (b&0x1e) == 0x16)
        return dis_common_vmvn_register_simd_insn(context, insn, ir);
    else if (a == 2 && (b&0x1e) == 0x00)
        return dis_common_vswap_simd_insn(context, insn, ir);
    else
        return dis_common_adv_simd_two_regs_misc_hlp(context, insn, ir);
}

static int dis_common_adv_simd_vext_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = NULL;
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "hlp_common_adv_simd_vext",
                        ir->add_mov_const_64(ir, (uint64_t) hlp_common_adv_simd_vext),
                        params);

    return 0;
}

static int dis_common_adv_simd_one_register_and_modified_immediate_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int cmode = INSN(11, 8);
    int op = INSN(5, 5);
    int isExit = 0;

    switch(cmode) {
        case 1: case 3: case 5: case 7: case 9: case 11:
            isExit = op?dis_common_vbic_immediate_insn(context, insn, ir):dis_common_vorr_immediate_insn(context, insn, ir);
            break;
        case 0: case 2: case 4: case 6: case 8: case 10: case 12: case 13:
            isExit = op?dis_common_vmvn_immediate_simd_insn(context, insn, ir):dis_common_vmov_immediate_simd_insn(context, insn, ir);
            break;
        case 14:
            isExit = dis_common_vmov_immediate_simd_insn(context, insn, ir);
            break;
        case 15:
            assert(op == 0);
            isExit = dis_common_vmov_immediate_simd_insn(context, insn, ir);
            break;
        default:
            fatal("cmode = %d / op = %d\n", cmode, op);
    }

    return isExit;
}

static int dis_common_adv_simd_two_regs_and_shift_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    struct irRegister *params[4];

    params[0] = mk_32(ir, insn);
    params[1] = mk_32(ir, is_thumb);
    params[2] = NULL;
    params[3] = NULL;

    ir->add_call_void(ir, "hlp_common_adv_simd_two_regs_and_shift",
                        ir->add_mov_const_64(ir, (uint64_t) hlp_common_adv_simd_two_regs_and_shift),
                        params);

    return 0;
}

static int dis_common_adv_simd_data_preocessing_insn(struct arm_target *context, uint32_t insn, struct irInstructionAllocator *ir)
{
    int a = INSN(23, 19);
    int b = INSN(11, 8);
    int c = INSN(7, 4);
    int U = is_thumb?INSN(28, 28):INSN(24, 24);
    int isExit = 0;

    if ((a & 0x16) == 0x16 && (c & 1) == 0) {
        if (U) {
            if (b == 0xc) {
                isExit = dis_common_adv_simd_vdup_scalar_insn(context, insn, ir);
            } else if ((b & 0xc) == 0x8) {
                //vtbl, vtbx
            } else if ((b & 0x8) == 0) {
                isExit = dis_common_adv_simd_two_regs_misc_insn(context, insn, ir);
            } else
                assert(0);
        } else {
            isExit = dis_common_adv_simd_vext_insn(context, insn, ir);
        }
    } else if (a & 0x10) {
        if (c & 1) {
            if ((a & 0x17) == 0x10 && (c & 0x9) == 1) {
                isExit = dis_common_adv_simd_one_register_and_modified_immediate_insn(context, insn, ir);
            } else {
                isExit = dis_common_adv_simd_two_regs_and_shift_insn(context, insn, ir);
            }
        } else {
            if ((c & 0x5) == 0 && ((a & 0x14) == 0x10 || (a & 0x16) == 0x14)) {
                isExit = dis_common_adv_simd_three_different_length_insn(context, insn, ir);
            } else if ((c & 0x5) == 0x4 && ((a & 0x14) == 0x10 || (a & 0x16) == 0x14)) {
                isExit = dis_common_adv_simd_two_regs_and_scalar_insn(context, insn, ir);
            } else
                assert(0);
        }
    } else {
        isExit = dis_common_adv_simd_three_same_length_insn(context, insn, ir);
    }

    return isExit;
}
