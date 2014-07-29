#include <stdio.h>
#include <assert.h>
#include <stddef.h>
#include <sys/mman.h>
#include <elf.h>
#include <string.h>

#include "arm.h"
#include "runtime.h"

void *auxv_start = NULL;
void *auxv_end = NULL;

static void map_vdso_version()
{
    void *res;

    res = mmap_guest((void *)0xffff0000, 4*1024, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
    if (res == (void *)0xffff0000) {
        //write kuser_helper_version
        *((uint32_t *)0xffff0ffc) = 5;
        //remove write access
        mprotect((void *)0xffff0000, 4*1024, PROT_READ);
    } else
        assert(0);
}

static int strcmp_env(char *s1, char *s2)
{
    while ( (*s1++ == *s2++) && (*s1 != '=') );
    return (*((unsigned char *)--s1) < *((unsigned char *)--s2)) ? -1 : (*(unsigned char *)s1 != *(unsigned char *)s2);
}

static int is_environment_variable_copy(char *current, void **additionnal_env, void **unset_env)
{
    while(*additionnal_env) {
        if (strcmp_env(current, *additionnal_env) == 0)
            return 0;
        additionnal_env++;
    }
    while(*unset_env) {
        if (strcmp_env(current, *unset_env) == 0)
            return 0;
        unset_env++;
    }

    return 1;
}

static void *allocate_stack()
{
    if (mmap_guest((void *) 0x70000000, 4 * 1024 * 1024,
        PROT_EXEC | PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN | MAP_FIXED,
        -1, 0) == (void *)0x70000000) {
        return (void *) (0x70000000 + 4 * 1024 * 1024);
    } else
        assert(0);
}

static void compute_emulated_stack_space(int argc, char **argv, struct load_auxv_info *auxv_info, void **additionnal_env, void **unset_env, void *target_argv0,
                                         int *string_area_size, int *pointer_area_size)
{
    void **pos = (void **) &argv[0];
    Elf64_auxv_t *auxv;

    *pointer_area_size = 0;
    *string_area_size = 0;
    /* setup argument number */
    *pointer_area_size += sizeof(void *);
    /* special handling of first argument */
    if (target_argv0) {
        *pointer_area_size += sizeof(void *);
        *string_area_size += strlen(target_argv0) + 1;
         pos++;
    }
    /* setup argument */
    while(*pos != NULL) {
        *pointer_area_size += sizeof(void *);
        *string_area_size += strlen(*pos) + 1;
        pos++;
    }
    *pointer_area_size += sizeof(void *);
    pos++;
    /* setup env */
    while(*pos != NULL) {
        if (is_environment_variable_copy(*pos, additionnal_env, unset_env)) {
            *pointer_area_size += sizeof(void *);
            *string_area_size += strlen(*pos) + 1;
        }
        pos++;
    }
    while(*additionnal_env != NULL) {
        *pointer_area_size += sizeof(void *);
        *string_area_size += strlen(*additionnal_env) + 1;
        additionnal_env++;
    }
    *pointer_area_size += sizeof(void *);
    pos++;
    /* auxv stuff */
    auxv = (Elf64_auxv_t *) pos;
    while(auxv->a_type != AT_NULL) {
        switch(auxv->a_type) {
            case AT_RANDOM:
            case AT_PAGESZ:
            case AT_UID:
            case AT_EUID:
            case AT_GID:
            case AT_EGID:
            case AT_CLKTCK:
                *pointer_area_size += sizeof(*auxv);
                break;
            default:
                break;
        }
        auxv++;
    }
    // add loading info
    *pointer_area_size += sizeof(*auxv);
    *pointer_area_size += sizeof(*auxv);
    *pointer_area_size += sizeof(*auxv);
    *pointer_area_size += sizeof(*auxv);
    // end of auxv
    *pointer_area_size += sizeof(*auxv);

    //round to page size
    *pointer_area_size = (*pointer_area_size + 4096) & ~0xfff;
    *string_area_size = (*string_area_size + 4096) & ~0xfff;
}

static void *populate_emulated_stack(void *stack, int argc, char **argv, struct load_auxv_info *auxv_info, void **additionnal_env, void **unset_env, void *target_argv0)
{
    uint32_t *ptr_area;
    char *str_area;
    Elf32_auxv_t *auxv_target;
    void **pos = (void **) &argv[0];
    Elf64_auxv_t *auxv;
    int string_area_size;
    int pointer_area_size;

    compute_emulated_stack_space(argc, argv, auxv_info, additionnal_env, unset_env, target_argv0, &string_area_size, &pointer_area_size);
    ptr_area = (uint32_t *) ((uint64_t)stack - string_area_size - pointer_area_size);
    str_area = (char *) ((uint64_t)stack - string_area_size);
    /* setup argument number */
    *ptr_area++ = argc;
    /* special handling of first argument */
    if (target_argv0) {
        *ptr_area++ = (uint32_t)(uint64_t) str_area;
        strcpy(str_area, target_argv0);
        str_area += strlen(target_argv0) + 1;
        pos++;
    }
    /* setup argument */
    while(*pos != NULL) {
        *ptr_area++ = (uint32_t)(uint64_t) str_area;
        strcpy(str_area, *pos);
        str_area += strlen(*pos) + 1;
        pos++;
    }
    *ptr_area++ = 0;
    pos++;
    /* setup env */
    while(*pos != NULL) {
        if (is_environment_variable_copy(*pos, additionnal_env, unset_env)) {
            *ptr_area++ = (uint32_t)(uint64_t) str_area;
            strcpy(str_area, *pos);
            str_area += strlen(*pos) + 1;
        }
        pos++;
    }
    while(*additionnal_env != NULL) {
        *ptr_area++ = (uint32_t)(uint64_t) str_area;
        strcpy(str_area, *additionnal_env);
        str_area += strlen(*additionnal_env) + 1;
        additionnal_env++;
    }
    *ptr_area++ = 0;
    pos++;
    /* auxv stuff */
    auxv = (Elf64_auxv_t *) pos;
    auxv_target = (Elf32_auxv_t *) ptr_area;
    auxv_start = (void *) auxv_target;
    while(auxv->a_type != AT_NULL) {
        auxv_target->a_type = auxv->a_type;
        switch(auxv->a_type) {
            case AT_RANDOM:
                auxv_target->a_un.a_val = (uint32_t) ((uint64_t)stack - string_area_size - pointer_area_size);
                auxv_target++;
                break;
            // below list auxv we simply copy
            case AT_PAGESZ:
            case AT_UID:
            case AT_EUID:
            case AT_GID:
            case AT_EGID:
            case AT_CLKTCK:
                auxv_target->a_un.a_val = auxv->a_un.a_val;
                auxv_target++;
                break;
            default:
                break;
        }
        auxv++;
    }
    // add loading info
    auxv_target->a_type = AT_PHDR;
    auxv_target->a_un.a_val = (unsigned int)(long) auxv_info->load_AT_PHDR;
    auxv_target++;
    auxv_target->a_type = AT_PHENT;
    auxv_target->a_un.a_val = auxv_info->load_AT_PHENT;
    auxv_target++;
    auxv_target->a_type = AT_PHNUM;
    auxv_target->a_un.a_val = auxv_info->load_AT_PHNUM;
    auxv_target++;
    auxv_target->a_type = AT_ENTRY;
    auxv_target->a_un.a_val = (unsigned int)(long) auxv_info->load_AT_ENTRY;
    auxv_target++;
    // end of auxv
    auxv_target->a_type = AT_NULL;
    auxv_target++;
    auxv_end = (void *) auxv_target;

    return (void *)((uint64_t)stack - string_area_size - pointer_area_size);
}

/* public api */
void *arm_load_program(const char *file, struct load_auxv_info *auxv_info)
{
    return load32(file, auxv_info);
}

void * arm_allocate_and_populate_stack(int argc, char **argv, struct load_auxv_info *auxv_info, void **additionnal_env, void **unset_env, void *target_argv0)
{
    void *emulated_stack = allocate_stack();

    map_vdso_version();
    return populate_emulated_stack(emulated_stack, argc, argv, auxv_info, additionnal_env, unset_env, target_argv0);;
}