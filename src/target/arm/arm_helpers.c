#include <stdio.h>
#include <assert.h>
#include <stddef.h>

#include "arm_private.h"
#include "arm_helpers.h"
#include "runtime.h"

#define INSN(msb, lsb) ((insn >> (lsb)) & ((1 << ((msb) - (lsb) + 1))-1))
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

    return n|z|c|v;
}

uint32_t arm_hlp_compute_sco(uint64_t context, uint32_t insn, uint32_t rm, uint32_t op, uint32_t oldcpsr)
{
    int shift_mode = INSN(6, 5);
    int sco = oldcpsr & 0x20000000;

    op = op & 0xff;
    switch(shift_mode) {
        case 0:
            if (op) {
                sco = ((rm << (op - 1)) >> 2) & 0x20000000;
            }
            break;
        case 1:
            if (INSN(4, 4) == 0 && op == 0)
                op += 32;
            if (op) {
                sco = ((rm >> (op - 1)) << 29) & 0x20000000;
            }
            break;
        case 2:
            if (INSN(4, 4) == 0  && op == 0)
                op += 32;
            if (op) {
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

uint32_t thumb_hlp_compute_next_flags(uint64_t context, uint32_t opcode, uint32_t rn, uint32_t op, uint32_t oldcpsr)
{
    int n, z, c, v;
    uint32_t op1 = rn;
    uint32_t op2 = op;
    uint32_t calc;

    c = oldcpsr & 0x20000000;
    v = oldcpsr & 0x10000000;
    switch(opcode) {
        case 4://add
            calc = rn + op;
            c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 3) & 0x10000000;
            break;
        case 2://sub
        case 10://cmp
            calc = rn - op;
            c = (op1 >= op2)?0x20000000:0;
            v = (((op1 ^ op2) & (op1 ^ calc)) >> 3) & 0x10000000;
            break;
        case 13://mov
            calc = op;
            break;
        default:
            fatal("thumb_hlp_compute_next_flags : unsupported opcode %d\n", opcode);
    }
    n = (calc & 0x80000000);
    z = (calc == 0)?0x40000000:0;

    return n|z|c|v;
}
