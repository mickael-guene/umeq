#include <math.h>
#include <stdint.h>
#include <fenv.h>

typedef uint32_t u_int32_t;

/* stolen from glibc */
typedef union
{
  double value;
  struct
  {
    u_int32_t lsw;
    u_int32_t msw;
  } parts;
  uint64_t word;
} ieee_double_shape_type;

typedef union
{
  float value;
  u_int32_t word;
} ieee_float_shape_type;

#define EXTRACT_WORDS(ix0,ix1,d)        \
do {                \
  ieee_double_shape_type ew_u;          \
  ew_u.value = (d);           \
  (ix0) = ew_u.parts.msw;         \
  (ix1) = ew_u.parts.lsw;         \
} while (0)

# define GET_FLOAT_WORD(i,d)          \
do {                \
  ieee_float_shape_type gf_u;         \
  gf_u.value = (d);           \
  (i) = gf_u.word;            \
} while (0)

double floor(double d)
{
  double res;
# if defined __AVX__ || defined SSE2AVX
  asm ("vroundsd $1, %1, %0, %0" : "=x" (res) : "xm" (d));
# else
  asm ("roundsd $1, %1, %0" : "=x" (res) : "xm" (d));
# endif
  return res;
}

double sqrt (double d)
{
  double res;
#if defined __AVX__ || defined SSE2AVX
  asm ("vsqrtsd %1, %0, %0" : "=x" (res) : "xm" (d));
#else
  asm ("sqrtsd %1, %0" : "=x" (res) : "xm" (d));
#endif
  return res;
}

float sqrtf (float d)
{
  float res;
#if defined __AVX__ || defined SSE2AVX
  asm ("vsqrtss %1, %0, %0" : "=x" (res) : "xm" (d));
#else
  asm ("sqrtss %1, %0" : "=x" (res) : "xm" (d));
#endif
  return res;
}

int __isnan(double x)
{
  int32_t hx, lx;
  EXTRACT_WORDS (hx, lx, x);
  hx &= 0x7fffffff;
  hx |= (u_int32_t) (lx | (-lx)) >> 31;
  hx = 0x7ff00000 - hx;
  return (int) (((u_int32_t) hx) >> 31);
}

int __isnanf(float x)
{
  int32_t ix;
  GET_FLOAT_WORD(ix,x);
  ix &= 0x7fffffff;
  ix = 0x7f800000 - ix;
  return (int)(((u_int32_t)(ix))>>31);
}

int feraiseexcept (int excepts)
{
  /* Raise exceptions represented by EXPECTS.  But we must raise only
     one signal at a time.  It is important that if the overflow/underflow
     exception and the inexact exception are given at the same time,
     the overflow/underflow exception follows the inexact exception.  */

  /* First: invalid exception.  */
  if ((FE_INVALID & excepts) != 0)
    {
      /* One example of an invalid operation is 0.0 / 0.0.  */
      float f = 0.0;

      __asm__ __volatile__ ("divss %0, %0 " : : "x" (f));
      (void) &f;
    }

  /* Next: division by zero.  */
  if ((FE_DIVBYZERO & excepts) != 0)
    {
      float f = 1.0;
      float g = 0.0;

      __asm__ __volatile__ ("divss %1, %0" : : "x" (f), "x" (g));
      (void) &f;
    }

  /* Next: overflow.  */
  if ((FE_OVERFLOW & excepts) != 0)
    {
      /* XXX: Is it ok to only set the x87 FPU?  */
      /* There is no way to raise only the overflow flag.  Do it the
     hard way.  */
      fenv_t temp;

      /* Bah, we have to clear selected exceptions.  Since there is no
     `fldsw' instruction we have to do it the hard way.  */
      __asm__ __volatile__ ("fnstenv %0" : "=m" (*&temp));

      /* Set the relevant bits.  */
      temp.__status_word |= FE_OVERFLOW;

      /* Put the new data in effect.  */
      __asm__ __volatile__ ("fldenv %0" : : "m" (*&temp));

      /* And raise the exception.  */
      __asm__ __volatile__ ("fwait");
    }

  /* Next: underflow.  */
  if ((FE_UNDERFLOW & excepts) != 0)
    {
      /* XXX: Is it ok to only set the x87 FPU?  */
      /* There is no way to raise only the underflow flag.  Do it the
     hard way.  */
      fenv_t temp;

      /* Bah, we have to clear selected exceptions.  Since there is no
     `fldsw' instruction we have to do it the hard way.  */
      __asm__ __volatile__ ("fnstenv %0" : "=m" (*&temp));

      /* Set the relevant bits.  */
      temp.__status_word |= FE_UNDERFLOW;

      /* Put the new data in effect.  */
      __asm__ __volatile__ ("fldenv %0" : : "m" (*&temp));

      /* And raise the exception.  */
      __asm__ __volatile__ ("fwait");
    }

  /* Last: inexact.  */
  if ((FE_INEXACT & excepts) != 0)
    {
      /* XXX: Is it ok to only set the x87 FPU?  */
      /* There is no way to raise only the inexact flag.  Do it the
     hard way.  */
      fenv_t temp;

      /* Bah, we have to clear selected exceptions.  Since there is no
     `fldsw' instruction we have to do it the hard way.  */
      __asm__ __volatile__ ("fnstenv %0" : "=m" (*&temp));

      /* Set the relevant bits.  */
      temp.__status_word |= FE_INEXACT;

      /* Put the new data in effect.  */
      __asm__ __volatile__ ("fldenv %0" : : "m" (*&temp));

      /* And raise the exception.  */
      __asm__ __volatile__ ("fwait");
    }

  /* Success.  */
  return 0;
}

int feclearexcept (int excepts)
{
  fenv_t temp;
  unsigned int mxcsr;

  /* Mask out unsupported bits/exceptions.  */
  excepts &= FE_ALL_EXCEPT;

  /* Bah, we have to clear selected exceptions.  Since there is no
     `fldsw' instruction we have to do it the hard way.  */
  __asm__ ("fnstenv %0" : "=m" (*&temp));

  /* Clear the relevant bits.  */
  temp.__status_word &= excepts ^ FE_ALL_EXCEPT;

  /* Put the new data in effect.  */
  __asm__ ("fldenv %0" : : "m" (*&temp));

  /* And the same procedure for SSE.  */
  __asm__ ("stmxcsr %0" : "=m" (*&mxcsr));

  /* Clear the relevant bits.  */
  mxcsr &= ~excepts;

  /* And put them into effect.  */
  __asm__ ("ldmxcsr %0" : : "m" (*&mxcsr));

  /* Success.  */
  return 0;
}

int fetestexcept (int excepts)
{
  int temp;
  unsigned int mxscr;

  /* Get current exceptions.  */
  __asm__ ("fnstsw %0\n"
       "stmxcsr %1" : "=m" (*&temp), "=m" (*&mxscr));

  return (temp | mxscr) & excepts & FE_ALL_EXCEPT;
}

int fesetround (int round)
{
  unsigned short int cw;
  int mxcsr;

  if ((round & ~0xc00) != 0)
    /* ROUND is no valid rounding mode.  */
    return 1;

  /* First set the x87 FPU.  */
  asm ("fnstcw %0" : "=m" (*&cw));
  cw &= ~0xc00;
  cw |= round;
  asm ("fldcw %0" : : "m" (*&cw));

  /* And now the MSCSR register for SSE, the precision is at different bit
     positions in the different units, we need to shift it 3 bits.  */
  asm ("stmxcsr %0" : "=m" (*&mxcsr));
  mxcsr &= ~ 0x6000;
  mxcsr |= round << 3;
  asm ("ldmxcsr %0" : : "m" (*&mxcsr));

  return 0;
}

int fegetround (void)
{
  int cw;
  /* We only check the x87 FPU unit.  The SSE unit should be the same
     - and if it's not the same there's no way to signal it.  */

  __asm__ ("fnstcw %0" : "=m" (*&cw));

  return cw & 0xc00;
}
