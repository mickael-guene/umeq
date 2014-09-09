#include <unistd.h>
#include <assert.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

/* FIXME: use alloca to allocate space for ptr */
/* FIXME: work only under proot */
int execve_s3264(uint32_t filename_p,uint32_t argv_p,uint32_t envp_p)
{
    int res;
    const char *filename = (const char *) g_2_h(filename_p);
    uint32_t * argv_guest = (uint32_t *) g_2_h(argv_p);
    uint32_t * envp_guest = (uint32_t *) g_2_h(envp_p);
    char *ptr[4096];
    char **argv;
    char **envp;
    int index = 0;

    argv = &ptr[index];
    while(*argv_guest != 0) {
        ptr[index++] = (char *) g_2_h(*argv_guest);
        argv_guest = (uint32_t *) ((long)argv_guest + 4);
    }
    ptr[index++] = NULL;
    envp = &ptr[index];
    while(*envp_guest != 0) {
        ptr[index++] = (char *) g_2_h(*envp_guest);
        envp_guest = (uint32_t *) ((long)envp_guest + 4);
    }
    ptr[index++] = NULL;
    /* sanity check in case we overflow => see fixme */
    if (index >= sizeof(ptr) / sizeof(char *))
        assert(0);

    res = syscall(SYS_execve, filename, argv, envp);

    return res;
}
