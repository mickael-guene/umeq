#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <unistd.h>
#include <assert.h>

#include "syscall64_64_private.h"

/* FIXME: use alloca to allocate space for ptr */
/* FIXME: work only under proot */
long execve_s6464(uint64_t filename_p, uint64_t argv_p, uint64_t envp_p)
{
    long res;
    const char *filename = (const char *) g_2_h(filename_p);
    uint64_t * argv_guest = (uint64_t *) g_2_h(argv_p);
    uint64_t * envp_guest = (uint64_t *) g_2_h(envp_p);
    char *ptr[4096];
    char **argv;
    char **envp;
    int index = 0;

    /* FIXME: do we really need to support this ? */
    /* Manual say 'On Linux, argv and envp can be specified as NULL' */
    assert(argv_p != 0);
    assert(envp_p != 0);

    argv = &ptr[index];
    while(*argv_guest != 0) {
        ptr[index++] = (char *) g_2_h(*argv_guest);
        argv_guest = (uint64_t *) ((long)argv_guest + 8);
    }
    ptr[index++] = NULL;
    envp = &ptr[index];
    while(*envp_guest != 0) {
        ptr[index++] = (char *) g_2_h(*envp_guest);
        envp_guest = (uint64_t *) ((long)envp_guest + 8);
    }
    ptr[index++] = NULL;
    /* sanity check in case we overflow => see fixme */
    if (index >= sizeof(ptr) / sizeof(char *))
        assert(0);

    res = syscall(SYS_execve, filename, argv, envp);

    return res;
}