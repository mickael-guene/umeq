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

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <ucontext.h>
#include "be_i386.h"

#define container_of(ptr, type, member) ({          \
    const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
    (type *)( (char *)__mptr - offsetof(type,member) );})

struct inter {
    struct backend backend;
};

/* backend api */
static struct backend_execute_result execute_be_i386(struct backend *backend, char *buffer, uint64_t context)
{
	assert(0 && "Implement me\n");
}

static void request_signal_alternate_exit(struct backend *backend, void *_ucp, uint64_t result)
{
    assert(0 && "Implement me\n");
}

static void patch(struct backend *backend, void *link_patch_area, void *cache_area)
{
    assert(0 && "Implement me\n");
}

static int jit(struct backend *backend, struct irInstruction *irArray, int irInsnNb, char *buffer, int bufferSize)
{
    struct inter *inter = container_of(backend, struct inter, backend);
    int res;

    assert(0 && "Implement me\n");

    return res;
}

static int find_insn(struct backend *backend, struct irInstruction *irArray, int irInsnNb, char *buffer, int bufferSize, int offset)
{
    struct inter *inter = container_of(backend, struct inter, backend);
    int res;

    assert(0 && "Implement me\n");

    return res;
}

static void reset(struct backend *backend)
{
    struct inter *inter = container_of(backend, struct inter, backend);

}

/* api */
struct backend *createI386Backend(void *memory, int size)
{
    struct inter *inter;

    assert(BE_I386_MIN_CONTEXT_SIZE >= sizeof(*inter));
    inter = (struct inter *) memory;
    if (inter) {
        inter->backend.jit = jit;
        inter->backend.execute = execute_be_i386;
        inter->backend.request_signal_alternate_exit = request_signal_alternate_exit;
        inter->backend.find_insn = find_insn;
        inter->backend.patch = patch;
        inter->backend.reset = reset;

        reset(&inter->backend);
    }

    return &inter->backend;
}

void deleteI386Backend(struct backend *backend)
{
    ;
}
