#include <sys/mman.h>
#include <elf.h>
#include <string.h>

#include "arm64.h"
#include "runtime.h"
#include "arm64_private.h"

void *auxv64_start = NULL;
void *auxv64_end = NULL;

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

static void compute_emulated_stack_space(int argc, char **argv, struct load_auxv_info_64 *auxv_info, void **additionnal_env,
                                         void **unset_env, void *target_argv0, int *string_area_size, int *pointer_area_size)
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

static guest64_ptr populate_emulated_stack(guest64_ptr stack, int argc, char **argv, struct load_auxv_info_64 *auxv_info,
                                         void **additionnal_env, void **unset_env, void *target_argv0)
{
    uint64_t *ptr_area;
    guest64_ptr str_area;
    Elf64_auxv_t *auxv_target;
    void **pos = (void **) &argv[0];
    Elf64_auxv_t *auxv;
    int string_area_size;
    int pointer_area_size;

    compute_emulated_stack_space(argc, argv, auxv_info, additionnal_env, unset_env, target_argv0, &string_area_size, &pointer_area_size);
    ptr_area = (uint64_t *) g_2_h_64(stack - string_area_size - pointer_area_size);
    str_area = stack - string_area_size;
    /* setup argument number */
    *ptr_area++ = argc;
    /* special handling of first argument */
    if (target_argv0) {
        *ptr_area++ = str_area;
        strcpy(g_2_h_64(str_area), target_argv0);
        str_area += strlen(target_argv0) + 1;
        pos++;
    }
    /* setup argument */
    while(*pos != NULL) {
        *ptr_area++ = (uint64_t) str_area;
        strcpy(g_2_h_64(str_area), *pos);
        str_area += strlen(*pos) + 1;
        pos++;
    }
    *ptr_area++ = 0;
    pos++;
    /* setup env */
    while(*pos != NULL) {
        if (is_environment_variable_copy(*pos, additionnal_env, unset_env)) {
            *ptr_area++ = (uint64_t) str_area;
            strcpy(g_2_h_64(str_area), *pos);
            str_area += strlen(*pos) + 1;
        }
        pos++;
    }
    while(*additionnal_env != NULL) {
        *ptr_area++ = (uint32_t) str_area;
        strcpy(g_2_h_64(str_area), *additionnal_env);
        str_area += strlen(*additionnal_env) + 1;
        additionnal_env++;
    }
    *ptr_area++ = 0;
    pos++;
    /* auxv stuff */
    auxv = (Elf64_auxv_t *) pos;
    auxv_target = (Elf64_auxv_t *) ptr_area;
    auxv64_start = (void *) auxv_target;
    while(auxv->a_type != AT_NULL) {
        auxv_target->a_type = auxv->a_type;
        switch(auxv->a_type) {
            case AT_RANDOM:
                auxv_target->a_un.a_val = ((uint64_t)stack - string_area_size - pointer_area_size);
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
    auxv_target->a_un.a_val = auxv_info->load_AT_PHDR;
    auxv_target++;
    auxv_target->a_type = AT_PHENT;
    auxv_target->a_un.a_val = auxv_info->load_AT_PHENT;
    auxv_target++;
    auxv_target->a_type = AT_PHNUM;
    auxv_target->a_un.a_val = auxv_info->load_AT_PHNUM;
    auxv_target++;
    auxv_target->a_type = AT_ENTRY;
    auxv_target->a_un.a_val = auxv_info->load_AT_ENTRY;
    auxv_target++;
    // end of auxv
    auxv_target->a_type = AT_NULL;
    auxv_target++;
    auxv64_end = (void *) auxv_target;

    return stack - string_area_size - pointer_area_size;
}

static guest64_ptr arm64_load_program(const char *file, struct load_auxv_info_64 *auxv_info)
{
    guest64_ptr res;

    res = load64(file, auxv_info);

    return res;
}

static guest64_ptr allocate_stack()
{
    if (mmap64_guest(0x70000000, 4 * 1024 * 1024,
        PROT_EXEC | PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS | MAP_GROWSDOWN | MAP_FIXED,
        -1, 0) == 0x70000000) {
        return (0x70000000 + 4 * 1024 * 1024);
    } else
        assert(0);
}

static guest64_ptr arm64_allocate_and_populate_stack(int argc, char **argv, struct load_auxv_info_64 *auxv_info, void **additionnal_env, void **unset_env, void *target_argv0)
{
    guest64_ptr emulated_stack = allocate_stack();

    return populate_emulated_stack(emulated_stack, argc, argv, auxv_info, additionnal_env, unset_env, target_argv0);;
}

/* public api */
void arm64_load_image(int argc, char **argv, void **additionnal_env, void **unset_env, void *target_argv0,
                    uint64_t *entry, uint64_t *stack)
{
    struct load_auxv_info_64 auxv_info;

    *entry = arm64_load_program(argv[0], &auxv_info);
    if (*entry) {
        arm64_setup_brk();
        *stack = arm64_allocate_and_populate_stack(argc, argv, &auxv_info, additionnal_env, unset_env, target_argv0);
    }
}
