#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>

#include "arm64_private.h"
#include "arm64_helpers.h"
#include "runtime.h"
#include "cache.h"

//#define DUMP_STACK 1
#define INSN(msb, lsb) ((insn >> (lsb)) & ((1 << ((msb) - (lsb) + 1))-1))

void arm64_hlp_dump(uint64_t regs)
{
    struct arm64_target *context = container_of((void *) regs, struct arm64_target, regs);
    int i;

    printf("==============================================================================\n\n");
    for(i = 0; i < 32; i++) {
        printf("r[%2d] = 0x%016lx  ", i, context->regs.r[i]);
        if (i % 4 == 3)
            printf("\n");
    }
    printf("nzcv  = 0x%08x\n", context->regs.nzcv);
    printf("pc    = 0x%016lx\n", context->regs.pc);
#ifdef DUMP_STACK
    for(i = 0 ;i < 16; i++) {
        if (context->regs.r[31] + i * 8 < context->sp_init)
            printf("0x%016lx [sp + 0x%02x] = 0x%016lx\n", context->regs.r[31] + i * 8, i * 8, *((uint64_t *)(context->regs.r[31] + i * 8)));
    }
#endif
}

static int tkill(int pid, int sig)
{
    return syscall(SYS_tkill, (long) pid, (long) sig);
}

static pid_t gettid()
{
    return syscall(SYS_gettid);
}

void arm64_gdb_breakpoint_instruction(uint64_t regs)
{
    tkill(gettid(), SIGILL);
    cleanCaches(0,~0);
}

void arm64_gdb_stepin_instruction(uint64_t regs)
{
    tkill(gettid(), SIGTRAP);
    cleanCaches(0,~0);
}

void arm64_clean_caches(uint64_t regs)
{
    cleanCaches(0,~0);
}

uint32_t arm64_hlp_compute_next_nzcv_32(uint64_t context, uint32_t opcode, uint32_t op1, uint32_t op2, uint32_t oldnzcv)
{
    int n, z, c, v;
    enum ops ops = opcode;
    uint32_t calc = 0;
    uint32_t c_in = (oldnzcv >> 29) & 1;

    /*fprintf(stderr, "context = 0x%016lx\n", context);
    fprintf(stderr, "opcode = 0x%08x\n", opcode);
    fprintf(stderr, "op1 = 0x%08x\n", op1);
    fprintf(stderr, "op2 = 0x%08x\n", op2);
    fprintf(stderr, "oldcpsr = 0x%018x\n", oldnzcv);*/

    c = oldnzcv & 0x20000000;
    v = oldnzcv & 0x10000000;
    switch(ops) {
        case OPS_ADD:
            calc = op1 + op2;
            c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 3) & 0x10000000;
            break;
        case OPS_SUB:
            calc = op1 - op2;
            c = (op1 >= op2)?0x20000000:0;
            v = (((op1 ^ op2) & (op1 ^ calc)) >> 3) & 0x10000000;
            break;
        case OPS_LOGICAL:
            calc = op1;
            c = 0;
            v = 0;
            break;
        case OPS_ADC:
            calc = op1 + op2 + c_in;
            if (c_in)
                c = (calc <= op1)?0x20000000:0;
            else
                c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 3) & 0x10000000;
            break;
        default:
            fatal("ops = %d\n", ops);
    }

    n = (calc & 0x80000000);
    z = (calc == 0)?0x40000000:0;

    return n|z|c|v|(oldnzcv&0x0fffffff);
}

uint32_t arm64_hlp_compute_next_nzcv_64(uint64_t context, uint32_t opcode, uint64_t op1, uint64_t op2, uint32_t oldnzcv)
{
    int n, z, c, v;
    enum ops ops = opcode;
    uint64_t calc = 0;
    uint32_t c_in = (oldnzcv >> 29) & 1;

    c = oldnzcv & 0x20000000;
    v = oldnzcv & 0x10000000;
    switch(ops) {
        case OPS_ADD:
            calc = op1 + op2;
            c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 35) & 0x10000000;
            break;
        case OPS_SUB:
            calc = op1 - op2;
            c = (op1 >= op2)?0x20000000:0;
            v = (((op1 ^ op2) & (op1 ^ calc)) >> 35) & 0x10000000;
            break;
        case OPS_LOGICAL:
            calc = op1;
            c = 0;
            v = 0;
            break;
        case OPS_ADC:
            calc = op1 + op2 + c_in;
            if (c_in)
                c = (calc <= op1)?0x20000000:0;
            else
                c = (calc < op1)?0x20000000:0;
            v = (((calc ^ op1) & (calc ^ op2)) >> 35) & 0x10000000;
            break;
        default:
            fprintf(stderr, "context = 0x%016lx\n", context);
            fprintf(stderr, "opcode = 0x%08x\n", opcode);
            fprintf(stderr, "op1 = 0x%016lx\n", op1);
            fprintf(stderr, "op2 = 0x%016lx\n", op2);
            fprintf(stderr, "oldcpsr = 0x%018x\n", oldnzcv);
            fatal("ops = %d\n", ops);
    }

    n = (calc & 0x8000000000000000UL) >> 32;
    z = (calc == 0)?0x40000000:0;

    return n|z|c|v|(oldnzcv&0x0fffffff);
}

/* return 1 if cond code is true */
/* note that this is the invert of arm code which return 0 when cond code is true ... */
uint32_t arm64_hlp_compute_flags_pred(uint64_t context, uint32_t cond, uint32_t nzcv)
{
    uint32_t pred = 0;

    switch(cond >> 1) {
        case 0://EQ + NE
            pred = (nzcv & 0x40000000)?1:0;
            break;
        case 1://HS + LO
            pred = (nzcv & 0x20000000)?1:0;
            break;
       case 2://MI + PL
            pred = (nzcv & 0x80000000)?1:0;
            break;
       case 3://VS + VC
            pred = (nzcv & 0x10000000)?1:0;
            break;
       case 4://HI + LS
            pred = ((nzcv & 0x20000000) && !(nzcv & 0x40000000))?1:0;
            break;
       case 5://GE + LT
            {
                uint32_t n = (nzcv >> 31) & 1;
                uint32_t v = (nzcv >> 28) & 1;
                pred = (n == v)?1:0;
            }
            break;
       case 6://GT + LE
            {
                uint32_t n = (nzcv >> 31) & 1;
                uint32_t z = (nzcv >> 30) & 1;
                uint32_t v = (nzcv >> 28) & 1;
                pred = ((z == 0) && (n == v))?1:0;
            }
            break;
        case 7://ALWAYS true
            pred = 1;
            break;
        default:
            assert(0);
    }
    //invert cond
    if (cond&1 && cond != 15)
        pred = 1 - pred;

    return pred;
}

/* FIXME: code stolen from my valgrind port. But was already stolen ? to be rework */
uint64_t arm64_hlp_compute_bitfield(uint64_t context, uint32_t insn, uint64_t rn, uint64_t rd)
{
  uint64_t res;
  int is64 = INSN(31,31);
  int reg_size = is64 ? 64 : 32;
  int64_t reg_mask = is64 ? ~0UL : 0xffffffff;
  int64_t R = INSN(21,16);
  int64_t S = INSN(15,10);
  int64_t diff = S - R;
  int64_t mask;
  // inzero indicates if the extracted bitfield is inserted into the
  // destination register value or in zero.
  // If extend is true, extend the sign of the extracted bitfield.
  int inzero = 0;
  int extend = 0;

  if (diff >= 0) {
    mask = diff < reg_size - 1 ? (1L << (diff + 1)) - 1
                               : reg_mask;
  } else {
    mask = ((1L << (S + 1)) - 1);
    mask = ((uint64_t)(mask) >> R) | (mask << (reg_size - R));
    diff += reg_size;
  }

  switch (insn & 0xFF800000) {
    case 0x33000000: //BFM_x
    case 0xB3000000: //BFM_w:
      break;
    case 0x13000000: //SBFM_x:
    case 0x93000000: //SBFM_w:
      inzero = 1;
      extend = 1;
      break;
    case 0x53000000: //UBFM_x:
    case 0xD3000000: //UBFM_w:
      inzero = 1;
      break;
    default:
      assert(0);
  }

  int64_t dst = inzero ? 0 : rd;
  int64_t src = is64?rn:(rn & 0xffffffff);
  // Rotate source bitfield into place.
  int64_t result = ((uint64_t)(src) >> R) | (src << (reg_size - R));
  // Determine the sign extension.
  int64_t topbits = ((1L << (reg_size - diff - 1)) - 1) << (diff + 1);
  int64_t signbits = extend && ((src >> S) & 1) ? topbits : 0;

  // Merge sign extension, dest/zero and bitfield.
  res = signbits | (result & mask) | (dst & ~mask);

  return res;
}

uint64_t arm64_hlp_udiv_64(uint64_t context, uint64_t op1, uint64_t op2)
{
    if (op2)
        return op1/op2;
    return 0;
}

uint32_t arm64_hlp_udiv_32(uint64_t context, uint32_t op1, uint32_t op2)
{
    if (op2)
        return op1/op2;
    return 0;
}

int64_t arm64_hlp_sdiv_64(uint64_t context, int64_t op1, int64_t op2)
{
    if (op2)
        return op1/op2;
    return 0;
}

int32_t arm64_hlp_sdiv_32(uint64_t context, int32_t op1, int32_t op2)
{
    if (op2)
        return op1/op2;
    return 0;
}

uint64_t arm64_hlp_umul_lsb_64(uint64_t context, uint64_t op1, uint64_t op2)
{
    return op1 * op2;
}

uint32_t arm64_hlp_umul_lsb_32(uint64_t context, uint32_t op1, uint32_t op2)
{
    return op1 * op2;
}

uint64_t arm64_hlp_umul_msb_64(uint64_t context, uint64_t op1, uint64_t op2)
{
    __uint128_t res = (__uint128_t)op1 * (__uint128_t)op2;

    return (uint64_t) (res >> 64);
}

int64_t arm64_hlp_smul_msb_64(uint64_t context, int64_t op1, int64_t op2)
{
    __int128_t res = (__int128_t)op1 * (__int128_t)op2;

    return (int64_t) (res >> 64);
}

int64_t arm64_hlp_smul_lsb_64(uint64_t context, int64_t op1, int64_t op2)
{
    return op1 * op2;
}

/* FIXME: ldrex / strex implementation below is not sematically correct. It's subject
          to the ABBA problem which is not the case of ldrex/strex hardware implementation
 */
uint64_t arm64_hlp_ldxr(uint64_t regs, uint64_t address, uint32_t size_access)
{
    struct arm64_target *context = container_of((void *) regs, struct arm64_target, regs);

    switch(size_access) {
        case 3://64 bits
            context->exclusive_value = (uint64_t) *((uint64_t *)g_2_h(address));
            break;
        case 2://32 bits
            context->exclusive_value = (uint32_t) *((uint32_t *)g_2_h(address));
            break;
        case 1://16 bits
            context->exclusive_value = (uint16_t) *((uint16_t *)g_2_h(address));
            break;
        case 0://8 bits
            context->exclusive_value = (uint8_t) *((uint8_t *)g_2_h(address));
            break;
        default:
            fatal("size_access %d unsupported\n", size_access);
    }

    return context->exclusive_value;
}

uint64_t arm64_hlp_ldaxr(uint64_t regs, uint64_t address, uint32_t size_access)
{
    uint64_t res;

    res = arm64_hlp_ldxr(regs, address, size_access);
    __sync_synchronize();

    return res;
}

void arm64_hlp_ldxp_dirty(uint64_t _regs, uint32_t insn)
{
    struct arm64_target *context = container_of((void *) _regs, struct arm64_target, regs);
    struct arm64_registers *regs = (struct arm64_registers *) _regs;

    int size = INSN(31, 30);
    int rn = INSN(9, 5);
    int rt = INSN(4, 0);
    int rt2 = INSN(14, 10);
    uint64_t address = regs->r[rn];

    if (size == 3) {
        context->exclusive_value = (__uint128_t) *((__uint128_t *) g_2_h(address));
        if (rt != 31)
            regs->r[rt] = (uint64_t) context->exclusive_value;
        if (rt2 != 31)
            regs->r[rt2] = (uint64_t) (context->exclusive_value >> 64);
    } else if (size == 2) {
        context->exclusive_value = (uint64_t) *((uint64_t *) g_2_h(address));
        if (rt != 31)
            regs->r[rt] = (uint32_t) context->exclusive_value;
        if (rt2 != 31)
            regs->r[rt2] = (uint32_t) (context->exclusive_value >> 32);
    } else
        assert(0);
}

void arm64_hlp_ldaxp_dirty(uint64_t _regs, uint32_t insn)
{
    arm64_hlp_ldxp_dirty(_regs, insn);
    __sync_synchronize();
}

void arm64_hlp_stxp_dirty(uint64_t _regs, uint32_t insn)
{
    struct arm64_target *context = container_of((void *) _regs, struct arm64_target, regs);
    struct arm64_registers *regs = (struct arm64_registers *) _regs;

    int size_access = INSN(31, 30);
    int rn = INSN(9, 5);
    int rt = INSN(4, 0);
    int rt2 = INSN(14, 10);
    int rs = INSN(20, 16);
    uint64_t address = regs->r[rn];
    uint64_t res = 0;

    switch(size_access) {
        case 3:
            {
#if 0
                //FIXME: need to implement __sync_bool_compare_and_swap_16
                __uint128_t value = ((__uint128_t)regs->r[rt2] << 64) | regs->r[rt];
                if (__sync_bool_compare_and_swap((__uint128_t *) g_2_h(address), (__uint128_t)context->exclusive_value, (__uint128_t)value))
                    res = 0;
                else
                    res = 1;
#else
                /* FIXME: not atomic ..... */
                if (__sync_bool_compare_and_swap((uint64_t *) g_2_h(address), (uint64_t)context->exclusive_value, regs->r[rt])) {
                    if (__sync_bool_compare_and_swap((uint64_t *) g_2_h(address + 8), (uint64_t)(context->exclusive_value >> 64), regs->r[rt2])) {
                        res = 0;
                    } else {
                        /* restore lsb ... */
                        res = 1;
                        context->exclusive_value = ((context->exclusive_value >> 64) << 64) | regs->r[rt];
                    }
                } else
                    res = 1;
#endif
            }
            break;
        case 2:
            {
                uint64_t value = (regs->r[rt2] << 32) | (regs->r[rt] & 0xffffffff);

                if (__sync_bool_compare_and_swap((uint64_t *) g_2_h(address), (uint64_t)context->exclusive_value, (uint64_t)value))
                    res = 0;
                else
                    res = 1;
                break;
            }
        default:
            fatal("size_access %d unsupported\n", size_access);
    }
    regs->r[rs] = res;
}

void arm64_hlp_stlxp_dirty(uint64_t _regs, uint32_t insn)
{
    __sync_synchronize();
    arm64_hlp_stxp_dirty(_regs, insn);
}

uint32_t arm64_hlp_stxr(uint64_t regs, uint64_t address, uint32_t size_access, uint64_t value)
{
    struct arm64_target *context = container_of((void *) regs, struct arm64_target, regs);
    uint32_t res = 0;

    switch(size_access) {
        case 3:
            if (__sync_bool_compare_and_swap((uint64_t *) g_2_h(address), (uint64_t)context->exclusive_value, (uint64_t)value))
                res = 0;
            else
                res = 1;
            break;
        case 2:
            if (__sync_bool_compare_and_swap((uint32_t *) g_2_h(address), (uint32_t)context->exclusive_value, (uint32_t)value))
                res = 0;
            else
                res = 1;
            break;
        case 1:
            if (__sync_bool_compare_and_swap((uint16_t *) g_2_h(address), (uint16_t)context->exclusive_value, (uint16_t)value))
                res = 0;
            else
                res = 1;
            break;
        case 0:
            if (__sync_bool_compare_and_swap((uint8_t *) g_2_h(address), (uint8_t)context->exclusive_value, (uint8_t)value))
                res = 0;
            else
                res = 1;
            break;
        default:
            fatal("size_access %d unsupported\n", size_access);
    }

    return res;
}

uint32_t arm64_hlp_stlxr(uint64_t regs, uint64_t address, uint32_t size_access, uint64_t value)
{
    __sync_synchronize();
    return arm64_hlp_stxr(regs, address, size_access, value);
}

/* FIXME: not correct => should use compare_and_swap to swap atomically value */
void arm64_hlp_clrex(uint64_t regs)
{
    struct arm64_target *context = container_of((void *) regs, struct arm64_target, regs);

    context->exclusive_value = ~~context->exclusive_value;
}

uint64_t arm64_hlp_clz(uint64_t context, uint64_t rn, uint32_t start_index)
{
    uint64_t res = 0;
    int i;

    for(i = start_index; i >= 0; i--) {
        if ((rn >> i) & 1)
            break;
        res++;
    }

    return res;
}

uint64_t arm64_hlp_cls(uint64_t context, uint64_t rn, uint32_t start_index)
{
    uint64_t res = 0;
    int i;
    int sign = (rn >> start_index) & 1;

    for(i = start_index - 1; i >= 0; i--) {
        if (((rn >> i) & 1) != sign)
            break;
        res++;
    }

    return res;
}

void arm64_hlp_memory_barrier(uint64_t regs)
{
    __sync_synchronize();
}

#include <arm64_crc32_tables.c>

static uint64_t crc32(uint64_t crc, uint8_t *buf, unsigned int len)
{
    while(len--)
        crc = crc32_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);

    return crc;
}

static uint64_t crc32c(uint64_t crc, uint8_t *buf, unsigned int len)
{
    while(len--)
        crc = crc32c_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);

    return crc;
}

void arm64_hlp_dirty_crc32(uint64_t _regs, uint32_t insn)
{
    const uint64_t mask[] = {0xff, 0xffff, 0xffffffff, ~0UL};
    struct arm64_registers *regs = (struct arm64_registers *) _regs;
    int sz = INSN(11,10);
    int rd = INSN(4, 0);
    int rn = INSN(9,5);
    int rm = INSN(20,16);
    int is_crc32c = INSN(12,12);
    uint64_t acc = (regs->r[rn] & 0xffffffff);
    uint64_t val= regs->r[rm] & mask[sz];

    if (is_crc32c)
        regs->r[rd] = crc32c(acc, (uint8_t *) &val, 1 << sz);
    else
        regs->r[rd] = crc32(acc, (uint8_t *) &val, 1 << sz);
}
