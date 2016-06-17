#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "breakpoints.h"
#include "cache.h"
#ifdef __TARGET32
#include "target32.h"
#endif
#ifdef __TARGET64
#include "target64.h"
#endif

#if 0
#define ARM_BRK             0xe7f001f1
#define T2_BRK              0x00a0f0f7
#define T1_BRK              0xde01

#define MAX_BP              16

typedef struct _breakpoint {
    struct _breakpoint *pNext;
    unsigned int opcode;
    unsigned int addr;
    unsigned int len;
    unsigned int type;
    unsigned int isInUse;
} breakpoint;

breakpoint bps[MAX_BP];

breakpoint *bpList = NULL;

static breakpoint *getFreeBreakpoint()
{
    int i;
    breakpoint *res = NULL;

    for(i = 0; i < MAX_BP; i++) {
        if (!bps[i].isInUse) {
            res = &bps[i];
            res->isInUse = 1;
            break;
        }
    }

    return res;
}

static void releaseBreakpoint(breakpoint *bp)
{
    assert(bp->isInUse == 1);

    bp->isInUse = 0;
}

void gdb_insert_breakpoint(unsigned int addr, int len, int type)
{
    //breakpoint *newBreakpoint = (breakpoint *) malloc(sizeof(breakpoint));
    breakpoint *newBreakpoint = getFreeBreakpoint();

    //fprintf(stderr, "insert BRK : addr = 0x%08x / len = %d / type = %d\n", addr, len, type);

    assert(newBreakpoint != NULL);
    newBreakpoint->addr = addr;
    newBreakpoint->len = len;
    newBreakpoint->type = type;

    switch(len) {
        case 2:
            newBreakpoint->opcode = *((uint16_t *) g_2_h((guest_ptr) addr));
            *((uint16_t *) g_2_h((guest_ptr) addr)) = T1_BRK;
            break;
        case 3:
            newBreakpoint->opcode = *((uint32_t *) g_2_h((guest_ptr) addr));
            *((uint32_t *) g_2_h((guest_ptr) addr)) = T2_BRK;
            break;
        case 4:
            newBreakpoint->opcode = *((uint32_t *) g_2_h((guest_ptr) addr));
            *((uint32_t *) g_2_h((guest_ptr) addr)) = ARM_BRK;
            break;
        default:
            assert(0 && "gdb_insert_breakpoint: invalid len");
    }

    newBreakpoint->pNext = bpList;
    bpList = newBreakpoint;
    cleanCaches(0, ~0);
}

void gdb_remove_breakpoint(unsigned int addr, int len, int type)
{
    breakpoint *pCurrent = bpList;
    breakpoint *pPrev = NULL;

    //fprintf(stderr, "enter gdb_remove_breakpoint\n");
    //find matching breakpoint
    while(pCurrent) {
        if (pCurrent->addr == addr && pCurrent->len == len && pCurrent->type == type)
            break;
        pPrev = pCurrent;
        pCurrent = pCurrent->pNext;
    }
    //remove it
    if (pCurrent) {
        breakpoint *pNext = pCurrent->pNext;
        if (pCurrent->len == 2)
            *((uint16_t *) g_2_h((guest_ptr) pCurrent->addr)) = pCurrent->opcode;
        else
            *((uint32_t *) g_2_h((guest_ptr) pCurrent->addr)) = pCurrent->opcode;
        //free(pCurrent);
        releaseBreakpoint(pCurrent);
        if (pPrev)
            pPrev->pNext = pNext;
        else
            bpList = (void *) pNext;
    }
    //fprintf(stderr, "exit gdb_remove_breakpoint\n");

    cleanCaches(0, ~0);
}

void gdb_remove_all()
{
    breakpoint *pBreakpoint = bpList;

    while(pBreakpoint) {
        breakpoint *pNext = pBreakpoint->pNext;
        if (pBreakpoint->len == 2)
            *((uint16_t *) g_2_h((guest_ptr) pBreakpoint->addr)) = pBreakpoint->opcode;
        else
            *((uint32_t *) g_2_h((guest_ptr) pBreakpoint->addr)) = pBreakpoint->opcode;
        //free(pBreakpoint);
        releaseBreakpoint(pBreakpoint);
        pBreakpoint = pNext;
    }

    bpList = NULL;
}

void gdb_reinstall_opcodes()
{
    breakpoint *pCurrent = bpList;

    //find matching breakpoint
    while(pCurrent) {
        if (pCurrent->len == 2)
            *((uint16_t *) g_2_h((guest_ptr) pCurrent->addr)) = pCurrent->opcode;
        else
            *((uint32_t *) g_2_h((guest_ptr) pCurrent->addr)) = pCurrent->opcode;
        pCurrent = pCurrent->pNext;
    }
}

void gdb_uninstall_opcodes()
{
    breakpoint *pCurrent = bpList;

    //find matching breakpoint
    while(pCurrent) {
        if (pCurrent->len == 2)
            *((uint16_t *) g_2_h((guest_ptr) pCurrent->addr)) = T1_BRK;
        else if (pCurrent->len == 3)
            *((uint32_t *) g_2_h((guest_ptr) pCurrent->addr)) = T2_BRK;
        else
            *((uint32_t *) g_2_h((guest_ptr) pCurrent->addr)) = ARM_BRK;
        pCurrent = pCurrent->pNext;
    }
}

unsigned int gdb_get_opcode(unsigned int addr)
{
    breakpoint *pCurrent = bpList;
    unsigned int res = ARM_BRK;

    //find matching breakpoint
    while(pCurrent) {
        if (pCurrent->addr == addr && pCurrent->type == 0) {
            if (pCurrent->len == 3)
                res = (pCurrent->opcode << 16) | (pCurrent->opcode >> 16);
            else
                res = pCurrent->opcode;
            break;
        }
        pCurrent = pCurrent->pNext;
    }

    return res;
}
#else
#define ARM64_BRK           0xd4200000
#define MAX_BP              16

typedef struct _breakpoint {
    struct _breakpoint *pNext;
    unsigned int opcode;
    unsigned long addr;
    unsigned int len;
    unsigned int type;
    unsigned int isInUse;
} breakpoint;

breakpoint bps[MAX_BP];

breakpoint *bpList = NULL;

static breakpoint *getFreeBreakpoint()
{
    int i;
    breakpoint *res = NULL;

    for(i = 0; i < MAX_BP; i++) {
        if (!bps[i].isInUse) {
            res = &bps[i];
            res->isInUse = 1;
            break;
        }
    }

    return res;
}

static void releaseBreakpoint(breakpoint *bp)
{
    assert(bp->isInUse == 1);

    bp->isInUse = 0;
}

void gdb_insert_breakpoint(unsigned long addr, int len, int type)
{
    //breakpoint *newBreakpoint = (breakpoint *) malloc(sizeof(breakpoint));
    breakpoint *newBreakpoint = getFreeBreakpoint();

    //fprintf(stderr, "insert BRK : addr = 0x%08x / len = %d / type = %d\n", addr, len, type);

    assert(newBreakpoint != NULL);
    newBreakpoint->addr = addr;
    newBreakpoint->len = len;
    newBreakpoint->type = type;

    switch(len) {
        case 4:
            newBreakpoint->opcode = *((uint32_t *)(uint64_t) addr);
            *((uint32_t *)(uint64_t)addr) = ARM64_BRK;
            break;
        default:
            fprintf(stderr, "len = %d\n", len);
            assert(0 && "gdb_insert_breakpoint: invalid len");
    }

    newBreakpoint->pNext = bpList;
    bpList = newBreakpoint;
    cleanCaches(0, ~0);
}

void gdb_remove_breakpoint(unsigned long addr, int len, int type)
{
    breakpoint *pCurrent = bpList;
    breakpoint *pPrev = NULL;

    //fprintf(stderr, "enter gdb_remove_breakpoint\n");
    //find matching breakpoint
    while(pCurrent) {
        if (pCurrent->addr == addr && pCurrent->len == len && pCurrent->type == type)
            break;
        pPrev = pCurrent;
        pCurrent = pCurrent->pNext;
    }
    //remove it
    if (pCurrent) {
        breakpoint *pNext = pCurrent->pNext;
        *((uint32_t *)(uint64_t)pCurrent->addr) = pCurrent->opcode;
        //free(pCurrent);
        releaseBreakpoint(pCurrent);
        if (pPrev)
            pPrev->pNext = pNext;
        else
            bpList = (void *) pNext;
    }
    //fprintf(stderr, "exit gdb_remove_breakpoint\n");

    cleanCaches(0, ~0);
}

void gdb_remove_all()
{
    breakpoint *pBreakpoint = bpList;

    while(pBreakpoint) {
        breakpoint *pNext = pBreakpoint->pNext;
        *((uint32_t *)(uint64_t)pBreakpoint->addr) = pBreakpoint->opcode;
        //free(pBreakpoint);
        releaseBreakpoint(pBreakpoint);
        pBreakpoint = pNext;
    }

    bpList = NULL;
}

void gdb_reinstall_opcodes()
{
    breakpoint *pCurrent = bpList;

    //find matching breakpoint
    while(pCurrent) {
        *((uint32_t *)(uint64_t)pCurrent->addr) = pCurrent->opcode;
        pCurrent = pCurrent->pNext;
    }
}

void gdb_uninstall_opcodes()
{
    breakpoint *pCurrent = bpList;

    //find matching breakpoint
    while(pCurrent) {
        *((uint32_t *)(uint64_t)pCurrent->addr) = ARM64_BRK;
        pCurrent = pCurrent->pNext;
    }
}

unsigned int gdb_get_opcode(unsigned long addr)
{
    breakpoint *pCurrent = bpList;
    unsigned int res = 0/*ARM_BRK*/;

    //find matching breakpoint
    while(pCurrent) {
        if (pCurrent->addr == addr && pCurrent->type == 0) {
            res = pCurrent->opcode;
            break;
        }
        pCurrent = pCurrent->pNext;
    }

    return res;
}

#endif
