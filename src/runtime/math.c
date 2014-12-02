#include <math.h>
#include <stdint.h>

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

int __isnan (double x)
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
