#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <sys/mman.h>
#include <signal.h>

#include "arm_private.h"
#include "arm_helpers.h"
#include "runtime.h"
#include "gdb.h"

#define INSN(msb, lsb) ((insn >> (lsb)) & ((1 << ((msb) - (lsb) + 1))-1))
#define INSN1(msb, lsb) INSN(msb+16, lsb+16)
#define INSN2(msb, lsb) INSN(msb, lsb)
//#define DUMP_STACK 1

void arm_hlp_dump(uint64_t regs)
{
    struct arm_target *context = container_of((void *) regs, struct arm_target, regs);
    int i;

    printf("==============================================================================\n\n");
    for(i = 0; i < 16; i++) {
        printf("r[%2d] = 0x%08x  ", i, context->regs.r[i]);
        if (i % 4 == 3)
            printf("\n");
    }
    printf("cpsr  = 0x%08x\n", context->regs.cpsr);
#ifdef DUMP_STACK
    for(i = 0 ;i < 16; i++) {
        if (context->regs.r[13] + i * 4 < context->sp_init)
            printf("0x%08x [sp + 0x%02x] = 0x%08x\n", context->regs.r[13] + i * 4, i * 4, *((uint32_t *)(uint64_t)(context->regs.r[13] + i * 4)));
    }
#endif
}

void arm_hlp_dump_and_assert(uint64_t regs)
{
    arm_hlp_dump(regs);
    assert(0);
}

static pid_t gettid()
{
    return syscall(SYS_gettid);
}

void arm_gdb_breakpoint_instruction(uint64_t regs)
{
    tkill(gettid(), SIGILL);
}

void arm_hlp_gdb_handle_breakpoint(uint64_t regs)
{
    struct arm_target *context = container_of((void *) regs, struct arm_target, regs);

    gdb_handle_breakpoint(&context->gdb);
}

void arm_hlp_vdso_cmpxchg(uint64_t _regs)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t oldval = regs->r[0];
    uint32_t newval = regs->r[1];
    uint32_t *address = (uint32_t *) (uint64_t)regs->r[2];

    if (__sync_bool_compare_and_swap(address, oldval, newval)) {
        regs->r[0] = 0;
        regs->cpsr |= 0x20000000;
    } else {
        regs->r[0] = ~0;
        regs->cpsr &= ~0x20000000;
    }
}

uint32_t arm_hlp_compute_flags_pred(uint64_t context, uint32_t cond, uint32_t cpsr)
{
    uint32_t pred = 0;

    switch(cond >> 1) {
        case 0://EQ + NE
            pred = (cpsr & 0x40000000)?0:1;
            break;
        case 1://HS + LO
            pred = (cpsr & 0x20000000)?0:1;
            break;
       case 2://MI + PL
            pred = (cpsr & 0x80000000)?0:1;
            break;
       case 3://VS + VC
            pred = (cpsr & 0x10000000)?0:1;
            break;
       case 4://HI + LS
            pred = ((cpsr & 0x20000000) && !(cpsr & 0x40000000))?0:1;
            break;
       case 5://GE + LT
            {
                uint32_t n = (cpsr >> 31) & 1;
                uint32_t v = (cpsr >> 28) & 1;
                pred = (n == v)?0:1;
            }
            break;
       case 6://GT + LE
            {
                uint32_t n = (cpsr >> 31) & 1;
                uint32_t z = (cpsr >> 30) & 1;
                uint32_t v = (cpsr >> 28) & 1;
                pred = ((z == 0) && (n == v))?0:1;
            }
            break;
        default:
            assert(0);
    }
    //invert cond
    if (cond&1)
        pred = 1 - pred;

    return pred;
}

uint32_t arm_hlp_compute_next_flags(uint64_t context, uint32_t opcode_and_shifter_carry_out, uint32_t rn, uint32_t op, uint32_t oldcpsr)
{
    int n, z, c, v;
    uint32_t op1 = rn;
    uint32_t op2 = op;
    uint32_t calc;
    uint32_t shifter_carry_out;
    uint32_t opcode;
    uint32_t c_in = (oldcpsr >> 29) & 1;

    opcode = opcode_and_shifter_carry_out & 0xf;
    shifter_carry_out = opcode_and_shifter_carry_out & 0x20000000;
    c = oldcpsr & 0x20000000;
    v = oldcpsr & 0x10000000;
    switch(opcode) {
        case 0://and
        case 8://tst
            calc = rn & op;
            c = shifter_carry_out;
            break;
        case 1://eor
            calc = rn ^ op;
            c = shifter_carry_out;
            break;
        case 2://sub
        case 10://cmp
            calc = rn - op;
            c = (op1 >= op2)?0x20000000:0;
            v = (((op1 ^ op2) & (op1 ^ calc)) >> 3) & 0x10000000;
            break;
        case 3://rsb
            calc = op - rn;
            c = (op2 >= op1)?0x20000000:0;
            v = (((op2 ^ op1) & (op2 ^ calc)) >> 3) & 0x10000000;
            break;
        case 5://adc
            calc = rn + op + c_in;
            if (c_in)
                c = (calc <= op1)?0x20000000:0;
            else
                c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 3) & 0x10000000;
            break;
        case 6://sbc
            calc = rn + ~op + c_in;
            if (c_in)
                c = (calc <= op1)?0x20000000:0;
            else
                c = (calc < op1)?0x20000000:0;
            v = (((op1 ^ op2) & (op1 ^ calc)) >> 3) & 0x10000000;
            break;
        case 7://rsc
            calc = op + ~rn + c_in;
            if (c_in)
                c = (calc <= op2)?0x20000000:0;
            else
                c = (calc < op2)?0x20000000:0;
            v = (((op2 ^ op1) & (op2 ^ calc)) >> 3) & 0x10000000;
            break;
        case 9://teq
            calc = rn ^ op;
            c = shifter_carry_out;
            break;
        case 4://add
        case 11://cmn
            calc = rn + op;
            c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 3) & 0x10000000;
            break;
        case 12://orr
            calc = rn | op;
            c = shifter_carry_out;
            break;
        case 13://mov
            calc = op;
            c = shifter_carry_out;
            break;
        case 14://bic
            calc = rn & (~op);
            c = shifter_carry_out;
            break;
        case 15://mvn
            calc = ~op;
            c = shifter_carry_out;
            break;
        default:
            printf("arm_hlp_compute_next_flags : opcode = %d\n\n", opcode);
            assert(0);
    }
    n = (calc & 0x80000000);
    z = (calc == 0)?0x40000000:0;

    return n|z|c|v|(oldcpsr&0x0fffffff);
}

uint32_t thumb_t2_hlp_compute_sco(uint64_t context, uint32_t insn, uint32_t rm, uint32_t op, uint32_t oldcpsr)
{
    int shift_mode = INSN(5, 4);
    int sco = oldcpsr & 0x20000000;

    op = op & 0xff;
    switch(shift_mode) {
        case 0:
            if (op == 0) {
                ;//nothing to do
            } else {
                sco = ((rm << (op - 1)) >> 2) & 0x20000000;
            }
            break;
        case 1:
            if (op == 0) //op is convert to 32
                sco = ((rm >> (31)) << 29) & 0x20000000;
            else
                sco = ((rm >> (op - 1)) << 29) & 0x20000000;
            break;
        case 2:
            if (op == 0)
                sco = ((rm >> (31)) << 29) & 0x20000000;
            else
                sco = ((rm >> (op - 1)) << 29) & 0x20000000;
            break;
        case 3:
            if (op == 0) {
                sco = (rm << 29) & 0x20000000;
            } else
                sco = ((rm >> (op - 1)) << 29) & 0x20000000;
            break;
        default:
            assert(0);
    }

    return sco;
}

uint32_t arm_hlp_compute_sco(uint64_t context, uint32_t insn, uint32_t rm, uint32_t op, uint32_t oldcpsr)
{
    int shift_mode = INSN(6, 5);
    int sco = oldcpsr & 0x20000000;

    op = op & 0xff;
    switch(shift_mode) {
        case 0:
            if (op > 32) {
                sco = 0;
            } else if (op) {
                sco = ((rm << (op - 1)) >> 2) & 0x20000000;
            }
            break;
        case 1:
            if (INSN(4, 4) == 0 && op == 0)
                op += 32;
            if (op > 32) {
                sco = 0;
            } else if (op) {
                sco = ((rm >> (op - 1)) << 29) & 0x20000000;
            }
            break;
        case 2:
            if (INSN(4, 4) == 0  && op == 0)
                op += 32;
            if (op >= 32) {
                sco = (rm & 0x80000000) >> 2;
            } else if (op) {
                int32_t rm_s = rm;

                sco = ((rm_s >> (op - 1)) << 29) & 0x20000000;
            }
            break;
        case 3:
            if (INSN(4, 4) == 0  && op == 0) {
                //rrx
                sco = (rm & 1) << 29;
            } else if (op) {
                op = op & 0x1f;
                if (op) {
                    sco = ((rm >> (op - 1)) << 29) & 0x20000000;
                } else {
                    sco = (rm >> 2) & 0x20000000;
                }
            }
            break;
        default:
            assert(0);
    }

    return sco;
}

uint32_t thumb_hlp_compute_next_flags_data_processing(uint64_t context, uint32_t opcode, uint32_t rn, uint32_t op, uint32_t oldcpsr)
{
    int n, z, c, v;
    uint32_t op1 = rn;
    uint32_t op2 = op;
    uint32_t calc;
    uint32_t c_in = (oldcpsr >> 29) & 1;

    c = oldcpsr & 0x20000000;
    v = oldcpsr & 0x10000000;
    switch(opcode) {
        case 0://and
            calc = rn & op;
            break;
        case 1://eor
            calc = rn ^ op;
            break;
        case 2://lsl
            {
                op2 = op2 & 0xff;
                if (op2 == 0) {
                    calc = op1;
                } else if (op2 < 32) {
                    calc = op1 << op2;
                    c = ((op1 << (op2 - 1)) >> 2) & 0x20000000;
                } else if (op2 == 32) {
                    calc = 0;
                    c = ((op1 << (op2 - 1)) >> 2) & 0x20000000;
                } else {
                    calc = 0;
                    c = 0;
                }
            }
            break;
        case 3://lsr
            {
                op2 = op2 & 0xff;
                if (op2 == 0) {
                    calc = op1;
                } else if (op2 < 32) {
                    calc = op1 >> op2;
                    c = ((op1 >> (op2 - 1)) << 29) & 0x20000000;
                } else if (op2 == 32) {
                    calc = 0;
                    c = ((op1 >> (op2 - 1)) << 29) & 0x20000000;
                } else {
                    calc = 0;
                    c = 0;
                }
            }
            break;
        case 4://asr
            {
                int32_t ops = (int32_t) op1;

                op2 = op2 & 0xff;
                if (op2 == 0) {
                    calc = op1;
                } else if (op2 < 32) {
                    calc = ops >> op2;
                    c = ((ops >> (op2 - 1)) << 29) & 0x20000000;
                } else {
                    calc = ops >> 31;
                    c = ((ops >> (32 - 1)) << 29) & 0x20000000;
                }
            }
            break;
        case 5://adc
            calc = rn + op + c_in;
            if (c_in)
                c = (calc <= op1)?0x20000000:0;
            else
                c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 3) & 0x10000000;
            break;
        case 6://sbc
            calc = rn + ~op + c_in;
            if (c_in)
                c = (calc <= op1)?0x20000000:0;
            else
                c = (calc < op1)?0x20000000:0;
            v = (((op1 ^ op2) & (op1 ^ calc)) >> 3) & 0x10000000;
            break;
        case 7://ror
            op2 = op2 & 0xff;
            if (op2 == 0) {
                calc = op1;
            } else if ((op2 & 0x1f) == 0) {
                calc = op1;
                c = (op1 >> 2) & 0x20000000;
            } else {
                op2 = op2 & 0x1f;
                calc = (op1 >> op2) | (op1 << (32 - op2));
                c = ((op1 >> (op2 - 1)) << 29) & 0x20000000;
            }
            break;
        case 8://tst
            calc = rn & op;
            break;
        case 9://rsb
            calc = -op2;
            c = (0 >= op2)?0x20000000:0;
            v = (((0 ^ op2) & (0 ^ calc)) >> 3) & 0x10000000;
            break;
        case 10://cmp
            calc = rn - op;
            c = (op1 >= op2)?0x20000000:0;
            v = (((op1 ^ op2) & (op1 ^ calc)) >> 3) & 0x10000000;
            break;
        case 11://cmn
            calc = rn + op;
            c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 3) & 0x10000000;
            break;
        case 12://orr
            calc = rn | op;
            break;
        case 13://mul
            calc = op1 * op2;
            break;
        case 14://bic
            calc = rn & (~op);
            break;
        case 15://mvn
            calc = ~op;
            break;
        default:
            fatal("thumb_hlp_compute_next_flags_data_processing : unsupported opcode %d\n", opcode);
    }
    n = (calc & 0x80000000);
    z = (calc == 0)?0x40000000:0;

    return n|z|c|v|(oldcpsr&0x0fffffff);
}

uint32_t thumb_hlp_compute_next_flags(uint64_t context, uint32_t opcode_and_shifter_carry_out, uint32_t rn, uint32_t op, uint32_t oldcpsr)
{
    int n, z, c, v;
    uint32_t op1 = rn;
    uint32_t op2 = op;
    uint32_t calc;
    uint32_t shifter_carry_out;
    uint32_t opcode;

    opcode = opcode_and_shifter_carry_out & 0x1f;
    shifter_carry_out = opcode_and_shifter_carry_out & 0x20000000;
    c = oldcpsr & 0x20000000;
    v = oldcpsr & 0x10000000;
    switch(opcode) {
        case 12://add register
        case 14://add 3 bits immediate
        case 24 ... 27://add 8 bits immediate
            calc = rn + op;
            c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 3) & 0x10000000;
            break;
        case 0 ... 3://lsl immediate
        case 4 ... 7://lsr immediate
        case 8 ... 11://asr immediate
        case 16 ... 19://move immediate
            calc = op;
            c = shifter_carry_out;
            break;
        case 13://sub register
        case 15://sub 3 bits immediate
        case 20 ... 23://cmp immediate
        case 28 ... 31://sub 8 immediate
            calc = rn - op;
            c = (op1 >= op2)?0x20000000:0;
            v = (((op1 ^ op2) & (op1 ^ calc)) >> 3) & 0x10000000;
            break;
        default:
            fatal("thumb_hlp_compute_next_flags : unsupported opcode %d(0x%x)\n", opcode, opcode);
    }
    n = (calc & 0x80000000);
    z = (calc == 0)?0x40000000:0;

    return n|z|c|v|(oldcpsr&0x0fffffff);
}

uint32_t thumb_hlp_t2_modified_compute_next_flags(uint64_t context, uint32_t opcode_and_shifter_carry_out, uint32_t rn, uint32_t op, uint32_t oldcpsr)
{
    int n, z, c, v;
    uint32_t op1 = rn;
    uint32_t op2 = op;
    uint32_t calc;
    uint32_t shifter_carry_out;
    uint32_t opcode;
    uint32_t c_in = (oldcpsr >> 29) & 1;

    opcode = opcode_and_shifter_carry_out & 0xf;
    shifter_carry_out = opcode_and_shifter_carry_out & 0x20000000;
    c = oldcpsr & 0x20000000;
    v = oldcpsr & 0x10000000;
    switch(opcode) {
        case 0://and / test
            calc = rn & op;
            c = shifter_carry_out;
            break;
        case 1://bic
            calc = rn & (~op);
            c = shifter_carry_out;
            break;
        case 2://orr / mv
            calc = rn | op;
            c = shifter_carry_out;
            break;
        case 3://orn / mvn
            calc = rn | (~op);
            c = shifter_carry_out;
            break;
        case 4://eor / teq
            calc = rn ^ op;
            c = shifter_carry_out;
            break;
        case 8://add / cmn
            calc = rn + op;
            c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 3) & 0x10000000;
            break;
        case 10://adc
            calc = rn + op + c_in;
            if (c_in)
                c = (calc <= op1)?0x20000000:0;
            else
                c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 3) & 0x10000000;
            break;
        case 11:
            calc = rn + ~op + c_in;
            if (c_in)
                c = (calc <= op1)?0x20000000:0;
            else
                c = (calc < op1)?0x20000000:0;
            v = (((op1 ^ op2) & (op1 ^ calc)) >> 3) & 0x10000000;
            break;
        case 13://sub / cmp
            calc = rn - op;
            c = (op1 >= op2)?0x20000000:0;
            v = (((op1 ^ op2) & (op1 ^ calc)) >> 3) & 0x10000000;
            break;
        case 14://rsb
            calc = op - rn;
            c = (op2 >= op1)?0x20000000:0;
            v = (((op2 ^ op1) & (op2 ^ calc)) >> 3) & 0x10000000;
            break;
        default:
            printf("thumb_hlp_t2_modified_compute_next_flags : opcode = %d\n\n", opcode);
            assert(0);
    }
    n = (calc & 0x80000000);
    z = (calc == 0)?0x40000000:0;

    return n|z|c|v|(oldcpsr&0x0fffffff);
}

uint32_t arm_hlp_clz(uint64_t context, uint32_t rm)
{
    uint32_t res = 0;
    int i;

    for(i = 31; i >= 0; i--) {
        if ((rm >> i) & 1)
            break;
        res++;
    }

    return res;
}

uint32_t arm_hlp_multiply_unsigned_lsb(uint64_t context, uint32_t op1, uint32_t op2)
{
    return op1 * op2;
}

uint32_t arm_hlp_multiply_flag_update(uint64_t context, uint32_t res, uint32_t old_cpsr)
{
    uint32_t cpsr = old_cpsr & 0x3fffffff;

    if (res == 0)
        cpsr |= 0x40000000;
    if (res >> 31)
        cpsr |= 0x80000000;

    return cpsr;
}

/* FIXME: ldrex / strex implementation below is not sematically correct. It's subject
          to the ABBA problem which is not the case of ldrex/strex hardware implementation
 */
uint32_t arm_hlp_ldrex(uint64_t regs, uint32_t address, uint32_t size_access)
{
    struct arm_target *context = container_of((void *) regs, struct arm_target, regs);

    switch(size_access) {
        case 4:
            context->exclusive_value = (uint64_t) *((uint32_t *)(uint64_t) address);
            break;
        default:
            fatal("size_access %d unsupported\n", size_access);
    }

    return (uint32_t) context->exclusive_value;
}

uint32_t arm_hlp_strex(uint64_t regs, uint32_t address, uint32_t size_access, uint32_t value)
{
    struct arm_target *context = container_of((void *) regs, struct arm_target, regs);
    uint32_t res = 0;

    switch(size_access) {
        case 4:
            if (__sync_bool_compare_and_swap((uint32_t *)(uint64_t) address, (uint32_t)context->exclusive_value, value))
                res = 0;
            else
                res = 1;
            break;
        default:
            fatal("size_access %d unsupported\n", size_access);
    }

    return res;
}

void arm_hlp_memory_barrier(uint64_t regs)
{
    __sync_synchronize();
}

static uint32_t unsignedSat8(int32_t i)
{
    if (i > 255)
        return 255;
    else if (i < 0)
        return 0;
    else
        return i;
}

static void uadd8(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int rn = INSN1(3, 0);
    int rd = INSN2(11, 8);
    int rm = INSN2(3, 0);
    uint32_t sum[4];
    int i;

    //clear ge
    regs->cpsr &= 0xfff0ffff;
    for(i = 0; i < 4; i++) {
        sum[i] = ((regs->r[rn] >> (i * 8)) & 0xff) + ((regs->r[rm] >> (i * 8)) & 0xff);
        regs->cpsr |= (sum[i] >= 0x100)?(0x10000 << i):0;
    }
    regs->r[rd] = ((sum[3] & 0xff) << 24) + ((sum[2] & 0xff) << 16) + ((sum[1] & 0xff) << 8) + (sum[0] & 0xff);
}

static void uqsub8(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t diff[4];
    int i;

    for(i = 0; i < 4; i++) {
        diff[i] = ((regs->r[rn] >> (i * 8)) & 0xff) - ((regs->r[rm] >> (i * 8)) & 0xff);
    }
    regs->r[rd] = (unsignedSat8(diff[3]) << 24) | (unsignedSat8(diff[2]) << 16) | (unsignedSat8(diff[1]) << 8) | unsignedSat8(diff[0]);
}

static void uqsub8_a1(uint64_t _regs, uint32_t insn)
{
    uqsub8(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

void thumb_hlp_t2_unsigned_parallel(uint64_t regs, uint32_t insn)
{
    int op1 = INSN1(6, 4);
    int op2 = INSN2(5, 4);

    if (op2 == 0) {
        switch(op1) {
            case 0:
                uadd8(regs, insn);
                break;
            default:
                fatal("op1 = %d(0x%x)\n", op1, op1);
        }
    } else {
        assert(0);
    }
}

void arm_hlp_unsigned_parallel(uint64_t regs, uint32_t insn)
{
    int op1 = INSN(21, 20);
    int op2 = INSN(7, 5);

    if (op1 == 1) {
        assert(0);
    } else if (op1 == 2) {
        switch(op2) {
            case 7:
                uqsub8_a1(regs, insn);
                break;
            default:
                fatal("op2 = %d(0x%x)\n", op2, op2);
        }
    } else if (op1 == 3) {
        assert(0);
    } else
        assert(0);
}

uint32_t arm_hlp_sel(uint64_t context, uint32_t cpsr, uint32_t rn, uint32_t rm)
{
    uint32_t res = 0;
    int i;

    for(i = 0; i < 4; i++) {
        if (cpsr & (0x10000 << i))
            res |= rn & (0xff << (i * 8));
        else
            res |= rm & (0xff << (i * 8));
    }

    return res;
}

uint32_t arm_hlp_multiply_unsigned_msb(uint64_t context, uint32_t op1, uint32_t op2)
{
    uint64_t res = (uint64_t)op1 * (uint64_t)op2;

    return res >> 32;
}

uint32_t arm_hlp_multiply_signed_lsb(uint64_t context, int32_t op1, int32_t op2)
{
    int64_t res = (int64_t)op1 * (int64_t)op2;

    return (uint32_t) res;
}

uint32_t arm_hlp_multiply_signed_msb(uint64_t context, int32_t op1, int32_t op2)
{
    int64_t res = (int64_t)op1 * (int64_t)op2;

    return (uint32_t) (res >> 32);
}

uint32_t arm_hlp_multiply_accumulate_unsigned_lsb(uint64_t context, uint32_t op1, uint32_t op2, uint32_t rdhi, uint32_t rdlo)
{
    return op1 * op2 + rdlo;
}

uint32_t arm_hlp_multiply_accumulate_signed_lsb(uint64_t context, int32_t op1, int32_t op2, uint32_t rdhi, uint32_t rdlo)
{
    uint64_t acc = (((uint64_t)rdhi) << 32) + rdlo;
    int64_t res = (int64_t)op1 * (int64_t)op2 + (int64_t)acc;

    return (uint32_t) res;
}

uint32_t arm_hlp_multiply_accumulate_unsigned_msb(uint64_t context, uint32_t op1, uint32_t op2, uint32_t rdhi, uint32_t rdlo)
{
    uint64_t acc = (((uint64_t)rdhi) << 32) + rdlo;
    uint64_t res = (uint64_t)op1 * (uint64_t)op2 + acc;

    return res >> 32;
}

uint32_t arm_hlp_multiply_accumulate_signed_msb(uint64_t context, int32_t op1, int32_t op2, uint32_t rdhi, uint32_t rdlo)
{
    uint64_t acc = (((uint64_t)rdhi) << 32) + rdlo;
    int64_t res = (int64_t)op1 * (int64_t)op2 + (int64_t)acc;

    return (uint32_t) (res >> 32);
}

void hlp_dirty_vpush(uint64_t _regs, uint32_t insn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int isDouble = INSN(8, 8);
    int d = (INSN(22, 22) << 4) | INSN(15, 12);
    int rn = 13;
    int regss = INSN(7, 0);
    uint32_t imm32 = INSN(7, 0) << 2;
    uint32_t *address = (uint32_t *) g_2_h(regs->r[rn] - imm32);
    int r;

    assert(isDouble == 1 && ((regss & 1) == 0));
    for(r = 0; r < regss; r++) {
        *address++ = regs->e.s[d + r];
    }

    regs->r[rn] = regs->r[rn] - imm32;
}
