/* This file is part of Umeq, an equivalent of qemu user mode emulation with improved robustness.
 *
 * Copyright (C) 2015 STMicroelectronics
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA.
 */

#include <stdlib.h>

#include <stddef.h>
#include <stdint.h>

#include "softfloat.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM_HELPERS_STOLEN_FROM_QEMU__
#define __ARM_HELPERS_STOLEN_FROM_QEMU__ 1

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
