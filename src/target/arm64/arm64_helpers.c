#include <stdio.h>
#include <assert.h>

#include "arm64_private.h"
#include "arm64_helpers.h"
#include "runtime.h"

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
            break;
        default:
            fprintf(stderr, "context = 0x%016lx\n", context);
            fprintf(stderr, "opcode = 0x%08x\n", opcode);
            fprintf(stderr, "op1 = 0x%016lx\n", op1);
            fprintf(stderr, "op2 = 0x%016lx\n", op2);
            fprintf(stderr, "oldcpsr = 0x%018x\n", oldnzcv);
            fatal("ops = %d\n", ops);
    }

    n = (calc & 0x80000000);
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
        default:
            assert(0);
    }
    //invert cond
    if (cond&1)
        pred = 1 - pred;

    return pred;
}

/* FIXME: code stolen from my valgrind port. But was already stolen ? to be rework */
uint64_t arm64_hlp_compute_bitfield(uint64_t context, uint32_t insn, uint64_t rn, uint64_t rd)
{
  uint64_t res;
  int is64 = INSN(31,31);
  int reg_size = is64 ? 64 : 32;
  int64_t reg_mask = is64 ? ~0 : 0xffffffff;
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
  int64_t src = rn;
  // Rotate source bitfield into place.
  int64_t result = ((int64_t)(src) >> R) | (src << (reg_size - R));
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

uint64_t arm64_hlp_umul_lsb_64(uint64_t context, uint64_t op1, uint64_t op2)
{
    return op1 * op2;
}

uint32_t arm64_hlp_umul_lsb_32(uint64_t context, uint32_t op1, uint32_t op2)
{
    return op1 * op2;
}
