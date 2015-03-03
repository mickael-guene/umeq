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

#define INSN(msb, lsb) ((insn >> (lsb)) & ((1 << ((msb) - (lsb) + 1))-1))
#define INSN1(msb, lsb) INSN(msb+16, lsb+16)
#define INSN2(msb, lsb) INSN(msb, lsb)
//#define DUMP_STACK 1

static int tkill(int pid, int sig)
{
    return syscall(SYS_tkill, (long) pid, (long) sig);
}

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

void arm_hlp_vdso_cmpxchg(uint64_t _regs)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t oldval = regs->r[0];
    uint32_t newval = regs->r[1];
    uint32_t *address = (uint32_t *) g_2_h(regs->r[2]);

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
            context->exclusive_value = (uint64_t) *((uint32_t *)g_2_h(address));
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
            if (__sync_bool_compare_and_swap((uint32_t *) g_2_h(address), (uint32_t)context->exclusive_value, value))
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

static int64_t unsignedSatQ(int64_t i, int n, int *isSat)
{
    int64_t sat_pos = (1UL << n) - 1;
    int64_t sat_neg = 0;
    int64_t res;
    *isSat = 0;

    if (i > sat_pos) {
        res = sat_pos;
        *isSat = 1;
    } else if (i < sat_neg) {
        res  = sat_neg;
        *isSat = 1;
    } else
        res = i;

    return res;
}

static int64_t unsignedSat(int64_t i, int n)
{
    int isSatDummy;

    return unsignedSatQ(i, n, &isSatDummy);
}

static int32_t unsignedSat8(int32_t i)
{
    return unsignedSat(i, 8);
}

static int32_t unsignedSat16(int32_t i)
{
    return unsignedSat(i, 16);
}

static uint32_t signedSat16(int32_t i)
{
    if (i > 32767)
        return 0x7fff;
    else if (i < -32768)
        return 0x8000;
    else
        return i & 0xffff;
}

static uint32_t signedSat8(int32_t i)
{
    if (i > 127)
        return 0x7f;
    else if (i < -128)
        return 0x80;
    else
        return i & 0xff;
}

static uint32_t signedSat32Q(int64_t i, int *isSat)
{
    uint32_t res;
    *isSat = 0;

    if (i > 2147483647L) {
        res = 0x7fffffff;
        *isSat = 1;
    } else if (i < -2147483648L) {
        res = 0x80000000;
        *isSat = 1;
    } else
        res = (uint32_t) i;

    return res;
}

static int64_t signedSatQ(int64_t i, int n, int *isSat)
{
    int64_t sat_pos = (1UL << (n - 1)) - 1;
    int64_t sat_neg = -(1UL << (n - 1));
    int64_t res;
    *isSat = 0;

    if (i > sat_pos) {
        res = sat_pos;
        *isSat = 1;
    } else if (i < sat_neg) {
        res  = sat_neg;
        *isSat = 1;
    } else
        res = i;

    return res;
}

static void uadd16_usub16(uint64_t _regs, int is_sub, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t res[2];
    int i;

    //clear ge
    regs->cpsr &= 0xfff0ffff;
    for(i = 0; i < 2; i++) {
        if (is_sub) {
            res[i] = ((regs->r[rn] >> (i * 16)) & 0xffff) - ((regs->r[rm] >> (i * 16)) & 0xffff);
            regs->cpsr |= (res[i] >= 0)?(0x30000 << (i * 2)):0;
        } else {
            res[i] = ((regs->r[rn] >> (i * 16)) & 0xffff) + ((regs->r[rm] >> (i * 16)) & 0xffff);
            regs->cpsr |= (res[i] >= 0x10000)?(0x30000 << (i * 2)):0;
        }
    }
    regs->r[rd] = ((res[1] & 0xffff) << 16) + (res[0] & 0xffff);
}

static void uadd16_usub16_a1(uint64_t _regs, uint32_t insn)
{
    uadd16_usub16(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void uadd16_usub16_t32(uint64_t _regs, uint32_t insn)
{
    uadd16_usub16(_regs, INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void uasx_usax(uint64_t _regs, int is_diff_first, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t op1, op2;

    //clear ge
    regs->cpsr &= 0xfff0ffff;
    if (is_diff_first) {
        op1 = ((regs->r[rn] >> (0 * 16)) & 0xffff) - ((regs->r[rm] >> (1 * 16)) & 0xffff);
        op2 = ((regs->r[rn] >> (1 * 16)) & 0xffff) + ((regs->r[rm] >> (0 * 16)) & 0xffff);
    } else {
        op1 = ((regs->r[rn] >> (0 * 16)) & 0xffff) + ((regs->r[rm] >> (1 * 16)) & 0xffff);
        op2 = ((regs->r[rn] >> (1 * 16)) & 0xffff) - ((regs->r[rm] >> (0 * 16)) & 0xffff);
    }
    if (is_diff_first) {
        if (op1 >= 0)
            regs->cpsr |= 3 << 16;
        if (op2 >= 0x10000)
            regs->cpsr |= 3 << 18;
    } else {
        if (op1 >= 0x10000)
            regs->cpsr |= 3 << 16;
        if (op2 >= 0)
            regs->cpsr |= 3 << 18;
    }

    regs->r[rd] = (((uint16_t)op2) << 16) | ((uint16_t)op1);
}

static void uasx_usax_a1(uint64_t _regs, uint32_t insn)
{
    uasx_usax(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void uasx_usax_t32(uint64_t _regs, uint32_t insn)
{
    uasx_usax(_regs, 1 - INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void uadd8_usub8(uint64_t _regs, int rd, int rn, int rm, int is_sub)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t res[4];
    int i;

    //clear ge
    regs->cpsr &= 0xfff0ffff;
    for(i = 0; i < 4; i++) {
        if (is_sub) {
            res[i] = ((regs->r[rn] >> (i * 8)) & 0xff) - ((regs->r[rm] >> (i * 8)) & 0xff);
            regs->cpsr |= (res[i] >= 0)?(0x10000 << i):0;
        } else {
            res[i] = ((regs->r[rn] >> (i * 8)) & 0xff) + ((regs->r[rm] >> (i * 8)) & 0xff);
            regs->cpsr |= (res[i] >= 0x100)?(0x10000 << i):0;
        }
    }
    regs->r[rd] = ((res[3] & 0xff) << 24) + ((res[2] & 0xff) << 16) + ((res[1] & 0xff) << 8) + (res[0] & 0xff);
}

static void uadd8_usub8_a1(uint64_t _regs, uint32_t insn)
{
    uadd8_usub8(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0), INSN(5, 5));
}

static void uadd8_usub8_t32(uint64_t _regs, uint32_t insn)
{
    uadd8_usub8(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0), INSN1(6, 6));
}

static void uqadd16_uqsub16(uint64_t _regs, int is_sub, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t res[2];
    int i;

    for(i = 0; i < 2; i++) {
        if (is_sub)
            res[i] = ((regs->r[rn] >> (i * 16)) & 0xffff) - ((regs->r[rm] >> (i * 16)) & 0xffff);
        else
            res[i] = ((regs->r[rn] >> (i * 16)) & 0xffff) + ((regs->r[rm] >> (i * 16)) & 0xffff);
    }

    regs->r[rd] =  (unsignedSat16(res[1]) << 16) | unsignedSat16(res[0]);
}

static void uqadd16_uqsub16_a1(uint64_t _regs, uint32_t insn)
{
    uqadd16_uqsub16(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void uqadd16_uqsub16_t32(uint64_t _regs, uint32_t insn)
{
    uqadd16_uqsub16(_regs, INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void uqasx_uqsax(uint64_t _regs, int is_diff_first, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t res[2];

    if (is_diff_first) {
        res[0] = ((regs->r[rn] >> (0 * 16)) & 0xffff) - ((regs->r[rm] >> (1 * 16)) & 0xffff);
        res[1] = ((regs->r[rn] >> (1 * 16)) & 0xffff) + ((regs->r[rm] >> (0 * 16)) & 0xffff);
    } else {
        res[0] = ((regs->r[rn] >> (0 * 16)) & 0xffff) + ((regs->r[rm] >> (1 * 16)) & 0xffff);
        res[1] = ((regs->r[rn] >> (1 * 16)) & 0xffff) - ((regs->r[rm] >> (0 * 16)) & 0xffff); 
    }

    regs->r[rd] =  (unsignedSat16(res[1]) << 16) | unsignedSat16(res[0]);
}

static void uqasx_uqsax_a1(uint64_t _regs, uint32_t insn)
{
    uqasx_uqsax(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void uqasx_uqsax_t32(uint64_t _regs, uint32_t insn)
{
    uqasx_uqsax(_regs, 1 - INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void uqadd8(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t res[4];
    int i;

    for(i = 0; i < 4; i++) {
        res[i] = ((regs->r[rn] >> (i * 8)) & 0xff) + ((regs->r[rm] >> (i * 8)) & 0xff);
    }

    regs->r[rd] =  (unsignedSat8(res[3]) << 24) | (unsignedSat8(res[2]) << 16) | (unsignedSat8(res[1]) << 8) | unsignedSat8(res[0]);
}

static void uqadd8_a1(uint64_t _regs, uint32_t insn)
{
    uqadd8(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void uqadd8_t32(uint64_t _regs, uint32_t insn)
{
    uqadd8(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
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

static void uqsub8_t32(uint64_t _regs, uint32_t insn)
{
    uqsub8(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void uhadd16_uhsub16(uint64_t _regs, int is_sub, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t sum[2];
    int i;

    for(i = 0; i < 2; i++) {
        if (is_sub)
            sum[i] = ((regs->r[rn] >> (i * 16)) & 0xffff) - ((regs->r[rm] >> (i * 16)) & 0xffff);
        else
            sum[i] = ((regs->r[rn] >> (i * 16)) & 0xffff) + ((regs->r[rm] >> (i * 16)) & 0xffff);
        sum[i] >>= 1;
    }

    regs->r[rd] = ((uint16_t)(sum[1]) << 16) | (uint16_t)(sum[0]);
}

static void uhadd16_uhsub16_a1(uint64_t _regs, uint32_t insn)
{
    uhadd16_uhsub16(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void uhadd16_uhsub16_t32(uint64_t _regs, uint32_t insn)
{
    uhadd16_uhsub16(_regs, INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void uhasx_uhsax(uint64_t _regs, int is_diff_first, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t res[2];

    if (is_diff_first) {
        res[0] = ((regs->r[rn] >> (0 * 16)) & 0xffff) - ((regs->r[rm] >> (1 * 16)) & 0xffff);
        res[1] = ((regs->r[rn] >> (1 * 16)) & 0xffff) + ((regs->r[rm] >> (0 * 16)) & 0xffff);
    } else {
        res[0] = ((regs->r[rn] >> (0 * 16)) & 0xffff) + ((regs->r[rm] >> (1 * 16)) & 0xffff);
        res[1] = ((regs->r[rn] >> (1 * 16)) & 0xffff) - ((regs->r[rm] >> (0 * 16)) & 0xffff);
    }
    res[0] >>= 1;
    res[1] >>= 1;

    regs->r[rd] = ((uint16_t)(res[1]) << 16) | (uint16_t)(res[0]);
}

static void uhasx_uhsax_a1(uint64_t _regs, uint32_t insn)
{
    uhasx_uhsax(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void uhasx_uhsax_t32(uint64_t _regs, uint32_t insn)
{
    uhasx_uhsax(_regs, 1 - INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void uhsub8_uhadd8(uint64_t _regs, int is_sub, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t sum[4];
    int i;

    for(i = 0; i < 4; i++) {
        if (is_sub)
            sum[i] = ((regs->r[rn] >> (i * 8)) & 0xff) - ((regs->r[rm] >> (i * 8)) & 0xff);
        else
            sum[i] = ((regs->r[rn] >> (i * 8)) & 0xff) + ((regs->r[rm] >> (i * 8)) & 0xff);
        sum[i] >>= 1;
    }

    regs->r[rd] = ((uint8_t)(sum[3]) << 24) | ((uint8_t)(sum[2]) << 16) | ((uint8_t)(sum[1]) << 8) | (uint8_t)(sum[0]);
}

static void uhsub8_uhadd8_a1(uint64_t _regs, uint32_t insn)
{
    uhsub8_uhadd8(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void uhsub8_uhadd8_t32(uint64_t _regs, uint32_t insn)
{
    uhsub8_uhadd8(_regs, INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void sadd16_ssub16(uint64_t _regs, int is_sub, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t res[2];
    int i;

    //clear ge
    regs->cpsr &= 0xfff0ffff;
    for(i = 0; i < 2; i++) {
        if (is_sub)
            res[i] = (int32_t)(int16_t)((regs->r[rn] >> (i * 16)) & 0xffff) - (int32_t)(int16_t)((regs->r[rm] >> (i * 16)) & 0xffff);
        else
            res[i] = (int32_t)(int16_t)((regs->r[rn] >> (i * 16)) & 0xffff) + (int32_t)(int16_t)((regs->r[rm] >> (i * 16)) & 0xffff);
        if (res[i] >= 0)
            regs->cpsr |= 3 << (16 + i * 2);
    }

    regs->r[rd] = (((uint16_t)res[1]) << 16) | ((uint16_t)res[0]);
}

static void sadd16_ssub16_a1(uint64_t _regs, uint32_t insn)
{
    sadd16_ssub16(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void sadd16_ssub16_t32(uint64_t _regs, uint32_t insn)
{
    sadd16_ssub16(_regs, INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void sasx_ssax(uint64_t _regs, int is_diff_first, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t op1, op2;

    //clear ge
    regs->cpsr &= 0xfff0ffff;
    if (is_diff_first) {
        op1 = (int32_t)(int16_t)((regs->r[rn] >> (0 * 16)) & 0xffff) - (int32_t)(int16_t)((regs->r[rm] >> (1 * 16)) & 0xffff);
        op2  = (int32_t)(int16_t)((regs->r[rn] >> (1 * 16)) & 0xffff) + (int32_t)(int16_t)((regs->r[rm] >> (0 * 16)) & 0xffff);
    } else {
        op1 = (int32_t)(int16_t)((regs->r[rn] >> (0 * 16)) & 0xffff) + (int32_t)(int16_t)((regs->r[rm] >> (1 * 16)) & 0xffff);
        op2  = (int32_t)(int16_t)((regs->r[rn] >> (1 * 16)) & 0xffff) - (int32_t)(int16_t)((regs->r[rm] >> (0 * 16)) & 0xffff);
    }
    if (op1 >= 0)
        regs->cpsr |= 3 << 16;
    if (op2 >= 0)
        regs->cpsr |= 3 << 18;

    regs->r[rd] = (((uint16_t)op2) << 16) | ((uint16_t)op1);
}

static void sasx_ssax_a1(uint64_t _regs, uint32_t insn)
{
    sasx_ssax(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void sasx_ssax_t32(uint64_t _regs, uint32_t insn)
{
    sasx_ssax(_regs, 1 - INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void sadd8_ssub8(uint64_t _regs, int is_sub, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t res[4];
    int i;

    //clear ge
    regs->cpsr &= 0xfff0ffff;
    for(i = 0; i < 4; i++) {
        if (is_sub)
            res[i] = (int32_t)(int8_t)((regs->r[rn] >> (i * 8)) & 0xff) - (int32_t)(int8_t)((regs->r[rm] >> (i * 8)) & 0xff);
        else
            res[i] = (int32_t)(int8_t)((regs->r[rn] >> (i * 8)) & 0xff) + (int32_t)(int8_t)((regs->r[rm] >> (i * 8)) & 0xff);
        if (res[i] >= 0)
            regs->cpsr |= 1 << (16 + i);
    }

    regs->r[rd] = (((uint8_t)res[3]) << 24) |(((uint8_t)res[2]) << 16) | (((uint8_t)res[1]) << 8) | ((uint8_t)res[0]);
}

static void sadd8_ssub8_a1(uint64_t _regs, uint32_t insn)
{
    sadd8_ssub8(_regs, INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void sadd8_ssub8_t32(uint64_t _regs, uint32_t insn)
{
    sadd8_ssub8(_regs, INSN1(6, 6), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void qadd16(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t sum[2];
    int i;

    for(i = 0; i < 2; i++) {
        sum[i] = (int32_t)(int16_t)((regs->r[rn] >> (i * 16)) & 0xffff) + (int32_t)(int16_t)((regs->r[rm] >> (i * 16)) & 0xffff);
    }
    regs->r[rd] = (signedSat16(sum[1]) << 16) | signedSat16(sum[0]);
}

static void qadd16_a1(uint64_t _regs, uint32_t insn)
{
    qadd16(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qadd16_t32(uint64_t _regs, uint32_t insn)
{
    qadd16(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void qasx(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t res[2];


    res[0] = (int32_t)(int16_t)((regs->r[rn] >> (0 * 16)) & 0xffff) - (int32_t)(int16_t)((regs->r[rm] >> (1 * 16)) & 0xffff);
    res[1] = (int32_t)(int16_t)((regs->r[rn] >> (1 * 16)) & 0xffff) + (int32_t)(int16_t)((regs->r[rm] >> (0 * 16)) & 0xffff);

    regs->r[rd] = (signedSat16(res[1]) << 16) | signedSat16(res[0]);
}

static void qasx_a1(uint64_t _regs, uint32_t insn)
{
    qasx(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qasx_t32(uint64_t _regs, uint32_t insn)
{
    qasx(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void qsax(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t res[2];


    res[0] = (int32_t)(int16_t)((regs->r[rn] >> (0 * 16)) & 0xffff) + (int32_t)(int16_t)((regs->r[rm] >> (1 * 16)) & 0xffff);
    res[1] = (int32_t)(int16_t)((regs->r[rn] >> (1 * 16)) & 0xffff) - (int32_t)(int16_t)((regs->r[rm] >> (0 * 16)) & 0xffff);

    regs->r[rd] = (signedSat16(res[1]) << 16) | signedSat16(res[0]);
}

static void qsax_a1(uint64_t _regs, uint32_t insn)
{
    qsax(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qsax_t32(uint64_t _regs, uint32_t insn)
{
    qsax(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void qsub16(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t sum[2];
    int i;

    for(i = 0; i < 2; i++) {
        sum[i] = (int32_t)(int16_t)((regs->r[rn] >> (i * 16)) & 0xffff) - (int32_t)(int16_t)((regs->r[rm] >> (i * 16)) & 0xffff);
    }
    regs->r[rd] = (signedSat16(sum[1]) << 16) | signedSat16(sum[0]);
}

static void qsub16_a1(uint64_t _regs, uint32_t insn)
{
    qsub16(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qsub16_t32(uint64_t _regs, uint32_t insn)
{
    qsub16(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void qadd8(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t sum[4];
    int i;

    for(i = 0; i < 4; i++) {
        sum[i] = (int32_t)(int8_t)((regs->r[rn] >> (i * 8)) & 0xff) + (int32_t)(int8_t)((regs->r[rm] >> (i * 8)) & 0xff);
    }
    regs->r[rd] = (signedSat8(sum[3]) << 24) | (signedSat8(sum[2]) << 16) | (signedSat8(sum[1]) << 8) | signedSat8(sum[0]);
}

static void qadd8_a1(uint64_t _regs, uint32_t insn)
{
    qadd8(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qadd8_t32(uint64_t _regs, uint32_t insn)
{
    qadd8(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void qsub8(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t sum[4];
    int i;

    for(i = 0; i < 4; i++) {
        sum[i] = (int32_t)(int8_t)((regs->r[rn] >> (i * 8)) & 0xff) - (int32_t)(int8_t)((regs->r[rm] >> (i * 8)) & 0xff);
    }
    regs->r[rd] = (signedSat8(sum[3]) << 24) | (signedSat8(sum[2]) << 16) | (signedSat8(sum[1]) << 8) | signedSat8(sum[0]);
}

static void qsub8_a1(uint64_t _regs, uint32_t insn)
{
    qsub8(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qsub8_t32(uint64_t _regs, uint32_t insn)
{
    qsub8(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void shadd16(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t sum[2];
    int i;

    for(i = 0; i < 2; i++) {
        sum[i] = (int32_t)(int16_t)((regs->r[rn] >> (i * 16)) & 0xffff) + (int32_t)(int16_t)((regs->r[rm] >> (i * 16)) & 0xffff);
        sum[i] >>= 1;
    }

    regs->r[rd] = ((uint16_t)(sum[1]) << 16) | (uint16_t)(sum[0]);
}

static void shadd16_a1(uint64_t _regs, uint32_t insn)
{
    shadd16(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void shadd16_t32(uint64_t _regs, uint32_t insn)
{
    shadd16(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void shasx(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t diff, sum;

    diff = (int32_t)(int16_t)((regs->r[rn] >> (0 * 16)) & 0xffff) - (int32_t)(int16_t)((regs->r[rm] >> (1 * 16)) & 0xffff);
    sum  = (int32_t)(int16_t)((regs->r[rn] >> (1 * 16)) & 0xffff) + (int32_t)(int16_t)((regs->r[rm] >> (0 * 16)) & 0xffff);
    diff >>= 1;
    sum >>= 1;

    regs->r[rd] = ((uint16_t)(sum) << 16) | (uint16_t)(diff);
}

static void shasx_a1(uint64_t _regs, uint32_t insn)
{
    shasx(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void shasx_t32(uint64_t _regs, uint32_t insn)
{
    shasx(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void shsax(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t diff, sum;

    sum = (int32_t)(int16_t)((regs->r[rn] >> (0 * 16)) & 0xffff) + (int32_t)(int16_t)((regs->r[rm] >> (1 * 16)) & 0xffff);
    diff  = (int32_t)(int16_t)((regs->r[rn] >> (1 * 16)) & 0xffff) - (int32_t)(int16_t)((regs->r[rm] >> (0 * 16)) & 0xffff);
    diff >>= 1;
    sum >>= 1;

    regs->r[rd] = ((uint16_t)(diff) << 16) | (uint16_t)(sum);
}

static void shsax_a1(uint64_t _regs, uint32_t insn)
{
    shsax(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void shsax_t32(uint64_t _regs, uint32_t insn)
{
    shsax(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void shsub16(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t diff[2];
    int i;

    for(i = 0; i < 2; i++) {
        diff[i] = (int32_t)(int16_t)((regs->r[rn] >> (i * 16)) & 0xffff) - (int32_t)(int16_t)((regs->r[rm] >> (i * 16)) & 0xffff);
        diff[i] >>= 1;
    }

    regs->r[rd] = ((uint16_t)(diff[1]) << 16) | (uint16_t)(diff[0]);
}

static void shsub16_a1(uint64_t _regs, uint32_t insn)
{
    shsub16(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void shsub16_t32(uint64_t _regs, uint32_t insn)
{
    shsub16(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void shadd8(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t sum[4];
    int i;

    for(i = 0; i < 4; i++) {
        sum[i] = (int32_t)(int8_t)((regs->r[rn] >> (i * 8)) & 0xff) + (int32_t)(int8_t)((regs->r[rm] >> (i * 8)) & 0xff);
        sum[i] >>= 1;
    }

    regs->r[rd] = ((uint8_t)(sum[3]) << 24) | ((uint8_t)(sum[2]) << 16) | ((uint8_t)(sum[1]) << 8) | (uint8_t)(sum[0]);
}

static void shadd8_a1(uint64_t _regs, uint32_t insn)
{
    shadd8(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void shadd8_t32(uint64_t _regs, uint32_t insn)
{
    shadd8(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void shsub8(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int32_t diff[4];
    int i;

    for(i = 0; i < 4; i++) {
        diff[i] = (int32_t)(int8_t)((regs->r[rn] >> (i * 8)) & 0xff) - (int32_t)(int8_t)((regs->r[rm] >> (i * 8)) & 0xff);
        diff[i] >>= 1;
    }

    regs->r[rd] = ((uint8_t)(diff[3]) << 24) | ((uint8_t)(diff[2]) << 16) | ((uint8_t)(diff[1]) << 8) | (uint8_t)(diff[0]);
}

static void shsub8_a1(uint64_t _regs, uint32_t insn)
{
    shsub8(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void shsub8_t32(uint64_t _regs, uint32_t insn)
{
    shsub8(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

void thumb_hlp_t2_unsigned_parallel(uint64_t regs, uint32_t insn)
{
    int op1 = INSN1(6, 4);
    int op2 = INSN2(5, 4);

    if (op2 == 0) {
        switch(op1) {
            case 0: case 4:
                uadd8_usub8_t32(regs, insn);
                break;
            case 1: case 5:
                uadd16_usub16_t32(regs, insn);
                break;
            case 2: case 6:
                uasx_usax_t32(regs, insn);
                break;
            default:
                fatal("op1 = %d(0x%x)\n", op1, op1);
        }
    } else if (op2 == 1) {
        switch(op1) {
            case 0:
                uqadd8_t32(regs, insn);
                break;
            case 1: case 5:
                uqadd16_uqsub16_t32(regs, insn);
                break;
            case 2: case 6:
                uqasx_uqsax_t32(regs, insn);
                break;
            case 4:
                uqsub8_t32(regs, insn);
                break;
            default:
                fatal("op1 = %d(0x%x)\n", op1, op1);
        }
    } else if (op2 == 2) {
        switch(op1) {
            case 0: case 4:
                uhsub8_uhadd8_t32(regs, insn);
                break;
            case 1: case 5:
                uhadd16_uhsub16_t32(regs, insn);
                break;
            case 2: case 6:
                uhasx_uhsax_t32(regs, insn);
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

    switch(op1) {
        case 1:
            switch(op2) {
                case 0:
                case 3:
                    uadd16_usub16_a1(regs, insn);
                    break;
                case 1:
                case 2:
                    uasx_usax_a1(regs, insn);
                    break;
                case 4:
                case 7:
                    uadd8_usub8_a1(regs, insn);
                    break;
                default:
                    fatal("op1 = %d / op2 = %d(0x%x)\n", op1, op2, op2);
            }
            break;
        case 2:
            switch(op2) {
                case 0:
                    uqadd16_uqsub16_a1(regs, insn);
                    break;
                case 1:
                case 2:
                    uqasx_uqsax_a1(regs, insn);
                    break;
                case 3:
                    uqadd16_uqsub16_a1(regs, insn);
                    break;
                case 4:
                    uqadd8_a1(regs, insn);
                    break;
                case 7:
                    uqsub8_a1(regs, insn);
                    break;
                default:
                    fatal("op1 = %d / op2 = %d(0x%x)\n", op1, op2, op2);
            }
            break;
        case 3:
            switch(op2) {
                case 0:
                case 3:
                    uhadd16_uhsub16_a1(regs, insn);
                    break;
                case 1:
                case 2:
                    uhasx_uhsax_a1(regs, insn);
                    break;
                case 4:
                case 7:
                    uhsub8_uhadd8_a1(regs, insn);
                    break;
                default:
                    fatal("op1 = %d / op2 = %d(0x%x)\n", op1, op2, op2);
            }
            break;
        default:
            fatal("op1 = %d / op2 = %d(0x%x)\n", op1, op2, op2);
    }
}

void arm_hlp_signed_parallel(uint64_t regs, uint32_t insn)
{
    int op1 = INSN(21, 20);
    int op2 = INSN(7, 5);

    switch(op1) {
        case 1:
            switch(op2) {
                case 0:
                case 3:
                    sadd16_ssub16_a1(regs, insn);
                    break;
                case 1:
                    sasx_ssax_a1(regs, insn);
                    break;
                case 2:
                    sasx_ssax_a1(regs, insn);
                    break;
                case 4:
                case 7:
                    sadd8_ssub8_a1(regs, insn);
                    break;
                default:
                    fatal("op1 = %d / op2 = %d(0x%x)\n", op1, op2, op2);
            }
            break;
        case 2:
            //saturating
            switch(op2) {
                case 0:
                    qadd16_a1(regs, insn);
                    break;
                case 1:
                    qasx_a1(regs, insn);
                    break;
                case 2:
                    qsax_a1(regs, insn);
                    break;
                case 3:
                    qsub16_a1(regs, insn);
                    break;
                case 4:
                    qadd8_a1(regs, insn);
                    break;
                case 7:
                    qsub8_a1(regs, insn);
                    break;
                default:
                    fatal("op1 = %d / op2 = %d(0x%x)\n", op1, op2, op2);
            }
            break;
        case 3:
            switch(op2) {
                case 0:
                    shadd16_a1(regs, insn);
                    break;
                case 1:
                    shasx_a1(regs, insn);
                    break;
                case 2:
                    shsax_a1(regs, insn);
                    break;
                case 3:
                    shsub16_a1(regs, insn);
                    break;
                case 4:
                    shadd8_a1(regs, insn);
                    break;
                case 7:
                    shsub8_a1(regs, insn);
                    break;
                default:
                    fatal("op1 = %d / op2 = %d(0x%x)\n", op1, op2, op2);
            }
            break;
        default:
            fatal("op1 = %d(0x%x)\n", op1, op1);
    }
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

static void qadd(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t unsat = (int64_t)(int32_t) regs->r[rm] + (int64_t)(int32_t) regs->r[rn];
    uint32_t res;
    int sat;

    res = signedSat32Q(unsat, &sat);
    if (sat)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = res;
}

static void qadd_a1(uint64_t _regs, uint32_t insn)
{
    qadd(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qadd_t32(uint64_t _regs, uint32_t insn)
{
    qadd(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void qsub(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t unsat = (int64_t)(int32_t) regs->r[rm] - (int64_t)(int32_t) regs->r[rn];
    uint32_t res;
    int sat;

    res = signedSat32Q(unsat, &sat);
    if (sat)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = res;
}

static void qsub_a1(uint64_t _regs, uint32_t insn)
{
    qsub(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qsub_t32(uint64_t _regs, uint32_t insn)
{
    qsub(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void qdadd(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t doubled;
    uint32_t res;
    int sat1, sat2;

    doubled = signedSat32Q(2 * (int64_t)(int32_t) regs->r[rn], &sat1);
    res = signedSat32Q((int64_t)(int32_t) regs->r[rm] + (int64_t)(int32_t) doubled, &sat2);
    if (sat1 || sat2)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = res;
}

static void qdadd_a1(uint64_t _regs, uint32_t insn)
{
    qdadd(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qdadd_t32(uint64_t _regs, uint32_t insn)
{
    qdadd(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void qdsub(uint64_t _regs, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t doubled;
    uint32_t res;
    int sat1, sat2;

    doubled = signedSat32Q(2 * (int64_t)(int32_t) regs->r[rn], &sat1);
    res = signedSat32Q((int64_t)(int32_t) regs->r[rm] - (int64_t)(int32_t) doubled, &sat2);
    if (sat1 || sat2)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = res;
}

static void qdsub_a1(uint64_t _regs, uint32_t insn)
{
    qdsub(_regs, INSN(15, 12), INSN(19, 16), INSN(3, 0));
}

static void qdsub_t32(uint64_t _regs, uint32_t insn)
{
    qdsub(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void smlaxy(uint64_t _regs, int n, int m, int rd, int rn, int rm, int ra)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t operand1, operand2;
    int64_t res;

    operand1 = (int16_t)(n?regs->r[rn]>>16:regs->r[rn]);
    operand2 = (int16_t)(m?regs->r[rm]>>16:regs->r[rm]);

    res = operand1 * operand2 + (int32_t)regs->r[ra];
    if (res != (int32_t) res)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = res;
}

static void smlaxy_a1(uint64_t _regs, uint32_t insn)
{
    smlaxy(_regs, INSN(5, 5), INSN(6, 6), INSN(19, 16), INSN(3, 0), INSN(11, 8), INSN(15, 12));
}

static void smlaxy_t32(uint64_t _regs, uint32_t insn)
{
    smlaxy(_regs, INSN2(5, 5), INSN2(4, 4), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0), INSN2(15, 12));
}

static void smulwy(uint64_t _regs, int m, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t operand2;
    int64_t res;

    operand2 = (int16_t)(m?regs->r[rm]>>16:regs->r[rm]);

    res = (int32_t)regs->r[rn] * operand2;

    regs->r[rd] = res >> 16;
}

static void smulwy_a1(uint64_t _regs, uint32_t insn)
{
    smulwy(_regs, INSN(6, 6), INSN(19, 16), INSN(3, 0), INSN(11, 8));
}

static void smulwy_t32(uint64_t _regs, uint32_t insn)
{
    smulwy(_regs, INSN2(4, 4), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void smlawy(uint64_t _regs, int m, int rd, int rn, int rm, int ra)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t operand2;
    int64_t res;

    operand2 = (int16_t)(m?regs->r[rm]>>16:regs->r[rm]);

    res = (int32_t)regs->r[rn] * operand2 + (((int64_t)(int32_t)regs->r[ra]) << 16);
    if ((res >> 16) != (int32_t)(res >> 16))
        regs->cpsr |= 1 << 27;

    regs->r[rd] = res >> 16;
}

static void smlawy_a1(uint64_t _regs, uint32_t insn)
{
    smlawy(_regs, INSN(6, 6), INSN(19, 16), INSN(3, 0), INSN(11, 8), INSN(15, 12));
}

static void smlawy_t32(uint64_t _regs, uint32_t insn)
{
    smlawy(_regs, INSN2(4, 4), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0), INSN2(15, 12));
}

static void smlalxy(uint64_t _regs, int n, int m, int rdlo, int rdhi, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t operand1, operand2;
    int64_t acc = ((uint64_t) regs->r[rdhi] << 32) + regs->r[rdlo];
    int64_t res;

    operand1 = (int16_t)(n?regs->r[rn]>>16:regs->r[rn]);
    operand2 = (int16_t)(m?regs->r[rm]>>16:regs->r[rm]);

    res = operand1 * operand2 + acc;

    regs->r[rdhi] = res >> 32;
    regs->r[rdlo] = res;
}

static void smlalxy_a1(uint64_t _regs, uint32_t insn)
{
    smlalxy(_regs, INSN(5, 5), INSN(6, 6), INSN(15, 12), INSN(19, 16), INSN(3, 0), INSN(11, 8));
}

static void smlalxy_t32(uint64_t _regs, uint32_t insn)
{
    smlalxy(_regs, INSN2(5, 5), INSN2(4, 4), INSN2(15, 12), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void smulxy(uint64_t _regs, int n, int m, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t operand1, operand2;
    int64_t res;

    operand1 = (int16_t)(n?regs->r[rn]>>16:regs->r[rn]);
    operand2 = (int16_t)(m?regs->r[rm]>>16:regs->r[rm]);

    res = operand1 * operand2;

    regs->r[rd] = res;
}

static void smulxy_a1(uint64_t _regs, uint32_t insn)
{
    smulxy(_regs, INSN(5, 5), INSN(6, 6), INSN(19, 16), INSN(3, 0), INSN(11, 8));
}

static void smulxy_t32(uint64_t _regs, uint32_t insn)
{
    smulxy(_regs, INSN2(5, 5), INSN2(4, 4), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void smuad_smusd(uint64_t _regs, int is_sub, int m, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t operand2 = regs->r[rm];
    int64_t product1, product2;
    int64_t res;

    if (m)
        operand2 = (operand2 >> 16) | (operand2 << 16);
    product1 = (int16_t)regs->r[rn] * (int16_t)operand2;
    product2 = (int16_t)(regs->r[rn] >> 16) * (int16_t)(operand2 >> 16);
    if (is_sub)
        res = product1 - product2;
    else
        res = product1 + product2;
    if (res != (int32_t) res)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = res;
}

static void smuad_smusd_a1(uint64_t _regs, uint32_t insn)
{
    smuad_smusd(_regs, INSN(6, 6), INSN(5, 5), INSN(19, 16), INSN(3, 0), INSN(11, 8));
}

static void smuad_smusd_t32(uint64_t _regs, uint32_t insn)
{
    smuad_smusd(_regs, INSN1(6, 6), INSN2(4, 4), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void smlad_smlsd(uint64_t _regs, int is_sub, int m, int rd, int rn, int rm, int ra)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t operand2 = regs->r[rm];
    int64_t product1, product2;
    int64_t res;

    if (m)
        operand2 = (operand2 >> 16) | (operand2 << 16);
    product1 = (int16_t)regs->r[rn] * (int16_t)operand2;
    product2 = (int16_t)(regs->r[rn] >> 16) * (int16_t)(operand2 >> 16);

    if (is_sub)
        res = product1 - product2 + (int32_t)regs->r[ra];
    else
        res = product1 + product2 + (int32_t)regs->r[ra];
    if (res != (int32_t) res)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = res;
}

static void smlad_smlsd_a1(uint64_t _regs, uint32_t insn)
{
    smlad_smlsd(_regs, INSN(6,6), INSN(5, 5), INSN(19, 16), INSN(3, 0), INSN(11, 8), INSN(15, 12));
}

static void smlad_smlsd_t32(uint64_t _regs, uint32_t insn)
{
    smlad_smlsd(_regs, INSN1(6,6), INSN2(4, 4), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0), INSN2(15, 12));
}

static void smlald_smlsld(uint64_t _regs, int is_sub, int m, int rdlo, int rdhi, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t operand2 = regs->r[rm];
    int64_t product1, product2;
    int64_t acc = ((uint64_t) regs->r[rdhi] << 32) + regs->r[rdlo];
    int64_t res;

    if (m)
        operand2 = (operand2 >> 16) | (operand2 << 16);
    product1 = (int16_t)regs->r[rn] * (int16_t)operand2;
    product2 = (int16_t)(regs->r[rn] >> 16) * (int16_t)(operand2 >> 16);

    if (is_sub)
        res = product1 - product2 + acc;
    else
        res = product1 + product2 + acc;
    if (res != (int32_t) res)
        regs->cpsr |= 1 << 27;

    regs->r[rdhi] = res >> 32;
    regs->r[rdlo] = res;
}

static void smlald_smlsld_a1(uint64_t _regs, uint32_t insn)
{
    smlald_smlsld(_regs, INSN(6, 6), INSN(5, 5), INSN(15, 12), INSN(19, 16), INSN(3, 0), INSN(11, 8));
}

static void smlald_smlsld_t32(uint64_t _regs, uint32_t insn)
{
    smlald_smlsld(_regs, INSN1(4, 4), INSN2(4, 4), INSN2(15, 12), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void smmul(uint64_t _regs, int r, int rd, int rn, int rm)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t product1 = (int32_t)regs->r[rn];
    int64_t product2 = (int32_t)regs->r[rm];
    int64_t res;

    res = product1 * product2;
    if (r)
        res += 0x80000000;

    regs->r[rd] = res >> 32;
}

static void smmul_a1(uint64_t _regs, uint32_t insn)
{
    smmul(_regs, INSN(5, 5), INSN(19, 16), INSN(3, 0), INSN(11, 8));
}

static void smmul_t32(uint64_t _regs, uint32_t insn)
{
    smmul(_regs, INSN2(4, 4), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0));
}

static void smmla_smmls(uint64_t _regs, int is_sub, int r, int rd, int rn , int rm, int ra)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t acc = (int64_t)regs->r[ra] << 32;
    int64_t product1 = (int32_t)regs->r[rn];
    int64_t product2 = (int32_t)regs->r[rm];
    int64_t res;

    if (is_sub)
        res = acc - product1 * product2;
    else
        res = acc + product1 * product2;
    if (r)
        res += 0x80000000;

    regs->r[rd] = res >> 32;
}

static void smmla_smmls_a1(uint64_t _regs, uint32_t insn)
{
    smmla_smmls(_regs, INSN(6, 6), INSN(5, 5), INSN(19, 16), INSN(3, 0), INSN(11, 8), INSN(15, 12));
}

static void smmla_smmls_t32(uint64_t _regs, uint32_t insn)
{
    smmla_smmls(_regs, INSN1(5, 5), INSN2(4, 4), INSN2(11, 8), INSN1(3, 0), INSN2(3, 0), INSN2(15, 12));
}

static void dis_ssat(uint64_t _regs, int saturate_to, int shift_mode, int shift_value, int rd, int rn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t operand;
    int64_t result;
    int isSat;

    switch(shift_mode) {
        case 0:
            operand = (int64_t)(int32_t)(regs->r[rn] << shift_value);
            break;
        case 2:
            operand = (int64_t)(int32_t)regs->r[rn] >> (shift_value?shift_value:32);
            break;
        default:
            fatal("Invalid shift mode %d\n", shift_mode);
    }

    result = signedSatQ(operand, saturate_to, &isSat);
    if (isSat)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = result;
}

static void dis_ssat_a1(uint64_t _regs, uint32_t insn)
{
    dis_ssat(_regs, INSN(20, 16) + 1, INSN(6, 5), INSN(11, 7), INSN(15, 12), INSN(3, 0));
}

static void dis_ssat_t32(uint64_t _regs, uint32_t insn)
{
    dis_ssat(_regs, INSN2(4, 0) + 1, INSN1(5, 4), (INSN2(14, 12) << 2) | INSN2(7, 6), INSN2(11, 8), INSN1(3, 0));
}

static void dis_usat(uint64_t _regs, int saturate_to, int shift_mode, int shift_value, int rd, int rn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t operand;
    int64_t result;
    int isSat;

    switch(shift_mode) {
        case 0:
            operand = (int64_t)(int32_t)(regs->r[rn] << shift_value);
            break;
        case 2:
            operand = (int64_t)(int32_t)regs->r[rn] >> (shift_value?shift_value:32);
            break;
        default:
            fatal("Invalid shift mode %d\n", shift_mode);
    }

    result = unsignedSatQ(operand, saturate_to, &isSat);
    if (isSat)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = result;
}

static void dis_usat_a1(uint64_t _regs, uint32_t insn)
{
    dis_usat(_regs, INSN(20, 16), INSN(6, 5), INSN(11, 7), INSN(15, 12), INSN(3, 0));
}

static void dis_usat_t32(uint64_t _regs, uint32_t insn)
{
    dis_usat(_regs, INSN2(4, 0), INSN1(5, 4), (INSN2(14, 12) << 2) | INSN2(7, 6), INSN2(11, 8), INSN1(3, 0));
}

static void dis_ssat16(uint64_t _regs, int saturate_to, int rd, int rn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t result1, result2;
    int isSat1, isSat2;

    result1 = signedSatQ((int64_t)(int16_t)regs->r[rn], saturate_to, &isSat1);
    result2 = signedSatQ((int64_t)(int16_t)(regs->r[rn] >> 16), saturate_to, &isSat2);
    if (isSat1 || isSat2)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = (result2 << 16) | (result1 & 0xffff);
}

static void dis_ssat16_a1(uint64_t _regs, uint32_t insn)
{
    dis_ssat16(_regs, INSN(19, 16) + 1, INSN(15, 12), INSN(3, 0));
}

static void dis_ssat16_t32(uint64_t _regs, uint32_t insn)
{
    dis_ssat16(_regs, INSN2(3, 0) + 1, INSN2(11, 8), INSN1(3, 0));
}

static void dis_usat16(uint64_t _regs, int saturate_to, int rd, int rn)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    int64_t result1, result2;
    int isSat1, isSat2;

    result1 = unsignedSatQ((int64_t)(int16_t)regs->r[rn], saturate_to, &isSat1);
    result2 = unsignedSatQ((int64_t)(int16_t)(regs->r[rn] >> 16), saturate_to, &isSat2);
    if (isSat1 || isSat2)
        regs->cpsr |= 1 << 27;

    regs->r[rd] = (result2 << 16) | (result1 & 0xffff);
}

static void dis_usat16_a1(uint64_t _regs, uint32_t insn)
{
    dis_usat16(_regs, INSN(19, 16), INSN(15, 12), INSN(3, 0));
}

static void dis_usat16_t32(uint64_t _regs, uint32_t insn)
{
    dis_usat16(_regs, INSN2(3, 0), INSN2(11, 8), INSN1(3, 0));
}

static void usad8_usada8(uint64_t _regs, int rd, int rn, int rm, int ra)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint32_t result = 0;
    int i;

    for(i = 0; i < 4; i++) {
        result += abs(((regs->r[rn] >> (i * 8)) & 0xff) - ((regs->r[rm] >> (i * 8)) & 0xff));
    }
    if (ra != 15)
        result += regs->r[ra];

    regs->r[rd] = result;
}

static void usad8_usada8_t32(uint64_t _regs, uint32_t insn)
{
    usad8_usada8(_regs, INSN2(11, 8), INSN1(3, 0), INSN2(3, 0), INSN2(15, 12));
}

void arm_hlp_dirty_saturating(uint64_t regs, uint32_t insn)
{
    int op = INSN(22, 21);

    switch(op) {
        case 0:
            qadd_a1(regs, insn);
            break;
        case 1:
            qsub_a1(regs, insn);
            break;
        case 2:
            qdadd_a1(regs, insn);
            break;
        case 3:
            qdsub_a1(regs, insn);
            break;
        default:
            fatal("op = %d(0x%x)\n", op, op);
    }
}

void arm_hlp_halfword_mult_and_mult_acc(uint64_t regs, uint32_t insn)
{
    int op1 = INSN(22, 21);
    int op = INSN(5,5);

    switch(op1) {
        case 0:
            smlaxy_a1(regs, insn);
            break;
        case 1:
            if (op)
                smulwy_a1(regs, insn);
            else
                smlawy_a1(regs, insn);
            break;
        case 2:
            smlalxy_a1(regs, insn);
            break;
        case 3:
            smulxy_a1(regs, insn);
            break;
        default:
            fatal("op1 = %d(0x%x)\n", op1, op1);
    }
}

void arm_hlp_signed_multiplies(uint64_t regs, uint32_t insn)
{
    int op1 = INSN(22, 20);
    int a = INSN(15, 12);

    switch(op1) {
        case 0:
            if (a == 15)
                smuad_smusd_a1(regs, insn);
            else
                smlad_smlsd_a1(regs, insn);
            break;
        case 4:
            smlald_smlsld_a1(regs, insn);
            break;
        case 5:
            if (a == 15)
                smmul_a1(regs, insn);
            else
                smmla_smmls_a1(regs, insn);
            break;
        default:
            fatal("op1 = %d(0x%x)\n", op1, op1);
    }
}

void arm_hlp_saturation(uint64_t regs, uint32_t insn)
{
    int op1 = INSN(22, 20);
    int op2 = INSN(7, 5);

    switch(op1) {
        case 2:
            if ((op2 & 1) == 0) {
                dis_ssat_a1(regs, insn);
            } else if (op2 == 1) {
                dis_ssat16_a1(regs, insn);
            } else
                assert(0);
            break;
        case 3:
            if ((op2 & 1) == 0) {
                dis_ssat_a1(regs, insn);
            } else
                assert(0);
            break;
        case 6:
            if ((op2 & 1) == 0) {
                dis_usat_a1(regs, insn);
            } else if (op2 == 1) {
                dis_usat16_a1(regs, insn);
            } else
                assert(0);
            break;
        case 7:
            if ((op2 & 1) == 0) {
                dis_usat_a1(regs, insn);
            } else
                assert(0);
            break;
        default:
            fatal("op1 = %d(0x%x)\n", op1, op1);
    }
}

void t32_hlp_saturation(uint64_t regs, uint32_t insn)
{
    int op = INSN1(8, 4);

    switch(op) {
        case 16:
            dis_ssat_t32(regs, insn);
            break;
        case 18:
            if (INSN2(14, 12) || INSN2(7, 6))
                dis_ssat_t32(regs, insn);
            else
                dis_ssat16_t32(regs, insn);
            break;
        case 24:
            dis_usat_t32(regs, insn);
            break;
        case 26:
            if (INSN2(14, 12) || INSN2(7, 6))
                dis_usat_t32(regs, insn);
            else
                dis_usat16_t32(regs, insn);
            break;
        default:
            fatal("op = %d(0x%x)\n", op, op);
    }
}

void arm_hlp_umaal(uint64_t _regs, uint32_t rdhi_nb, uint32_t rdlo_nb, uint32_t rn_nb, uint32_t rm_nb)
{
    struct arm_registers *regs = (struct arm_registers *) _regs;
    uint64_t rdhi = regs->r[rdhi_nb];
    uint64_t rdlo = regs->r[rdlo_nb];
    uint64_t rn = regs->r[rn_nb];
    uint64_t rm = regs->r[rm_nb];
    uint64_t result = rn * rm + rdhi + rdlo;

    regs->r[rdhi_nb] = result >> 32;
    regs->r[rdlo_nb] = result;
}

void arm_hlp_sum_absolute_difference(uint64_t _regs, uint32_t insn)
{
    usad8_usada8(_regs, INSN(19, 16), INSN(3, 0), INSN(11, 8), INSN(15, 12));
}

void thumb_hlp_t2_misc_saturating(uint64_t regs, uint32_t insn)
{
    int op2 = INSN2(5, 4);

    switch(op2) {
        case 0:
            qadd_t32(regs, insn);
            break;
        case 1:
            qdadd_t32(regs, insn);
            break;
        case 2:
            qsub_t32(regs, insn);
            break;
        case 3:
            qdsub_t32(regs, insn);
            break;
        default:
            fatal("op2 = %d(0x%x)\n", op2, op2);
    }
}

void thumb_hlp_t2_signed_parallel(uint64_t regs, uint32_t insn)
{
    int op1 = INSN1(6, 4);
    int op2 = INSN2(5, 4);

    switch(op2) {
        case 0:
            switch(op1) {
                case 0: case 4:
                    sadd8_ssub8_t32(regs, insn);
                    break;
                case 1: case 5:
                    sadd16_ssub16_t32(regs, insn);
                    break;
                case 2:
                    sasx_ssax_t32(regs, insn);
                    break;
                case 6:
                    sasx_ssax_t32(regs, insn);
                    break;
                default:
                    fatal("op1 = %d(0x%x)\n", op1, op1);
            }
            break;
        case 1:
            switch(op1) {
                case 0:
                    qadd8_t32(regs, insn);
                    break;
                case 1:
                    qadd16_t32(regs, insn);
                    break;
                case 2:
                    qasx_t32(regs, insn);
                    break;
                case 4:
                    qsub8_t32(regs, insn);
                    break;
                case 5:
                    qsub16_t32(regs, insn);
                    break;
                case 6:
                    qsax_t32(regs, insn);
                    break;
                default:
                    fatal("op1 = %d(0x%x)\n", op1, op1);
            }
            break;
        case 2:
            switch(op1) {
                case 0:
                    shadd8_t32(regs, insn);
                    break;
                case 1:
                    shadd16_t32(regs, insn);
                    break;
                case 2:
                    shasx_t32(regs, insn);
                    break;
                case 4:
                    shsub8_t32(regs, insn);
                    break;
                case 5:
                    shsub16_t32(regs, insn);
                    break;
                case 6:
                    shsax_t32(regs, insn);
                    break;
                default:
                    fatal("op1 = %d(0x%x)\n", op1, op1);
            }
            break;
        default:
            fatal("op2 = %d(0x%x)\n", op2, op2);
    }
}

void thumb_hlp_t2_mult_A_mult_acc_A_abs_diff(uint64_t regs, uint32_t insn)
{
    int op1 = INSN1(6, 4);
    //int op2 = INSN2(5, 4);
    int ra = INSN2(15, 12);

    switch(op1) {
        case 1:
            if (ra == 15)
                smulxy_t32(regs, insn);
            else
                smlaxy_t32(regs, insn);
            break;
        case 2:
            if (ra == 15)
                smuad_smusd_t32(regs, insn);
            else
                smlad_smlsd_t32(regs, insn);
            break;
        case 3:
            if (ra == 15)
                smulwy_t32(regs, insn);
            else
                smlawy_t32(regs, insn);
            break;
        case 4:
            if (ra == 15)
                smuad_smusd_t32(regs, insn);
            else
                smlad_smlsd_t32(regs, insn);
            break;
        case 5:
            if (ra == 15)
                smmul_t32(regs, insn);
            else
                smmla_smmls_t32(regs, insn);
            break;
        case 6:
            if (ra == 15)
                assert(0);
            else
                smmla_smmls_t32(regs, insn);
            break;
        case 7:
            usad8_usada8_t32(regs, insn);
            break;
        default:
           fatal("op1 = %d(0x%x)\n", op1, op1);
    }
}

void thumb_hlp_t2_long_mult_A_long_mult_acc_A_div(uint64_t regs, uint32_t insn)
{
    int op1 = INSN1(6, 4);
    int op2 = INSN2(7, 4);

    switch(op1) {
        case 4:
            if ((op2 & 0xc) == 0x8)
                smlalxy_t32(regs, insn);
            else if ((op2 & 0xe) == 0xc)
                smlald_smlsld_t32(regs, insn);
            else
                assert(0);
            break;
        case 5:
            smlald_smlsld_t32(regs, insn);
            break;
        case 6:
            if (op2)
                arm_hlp_umaal(regs, INSN2(11, 8), INSN2(15, 12), INSN1(3, 0), INSN2(3, 0));
            else
                assert(0);
            break;
        default:
           fatal("op1 = %d(0x%x)\n", op1, op1);
    }
}
