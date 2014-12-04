#include <stdlib.h>

#include <stddef.h>
#include <stdint.h>

#include "softfloat.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM64_HELPERS_STOLEN_FROM_QEMU__
#define __ARM64_HELPERS_STOLEN_FROM_QEMU__ 1

#define HELPER(a)   qemu_stolen_##a

extern float32 HELPER(recpe_f32)(float32 input, void *fpstp);
extern float64 HELPER(recpe_f64)(float64 input, void *fpstp);
extern float32 HELPER(frecpx_f32)(float32 a, void *fpstp);
extern float64 HELPER(frecpx_f64)(float64 a, void *fpstp);
extern float32 HELPER(rsqrte_f32)(float32 input, void *fpstp);
extern float64 HELPER(rsqrte_f64)(float64 input, void *fpstp);

#endif

#ifdef __cplusplus
}
#endif
