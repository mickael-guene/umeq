#include <sys/types.h>
#include <unistd.h>

#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

int llseek_s3264(uint32_t fd_p, uint32_t offset_high_p, uint32_t offset_low_p, uint32_t result_p, uint32_t whence_p)
{
    int res;
    int fd = (int) fd_p;
    uint32_t offset_high = offset_high_p;
    uint32_t offset_low = offset_low_p;
    int64_t *result = (int64_t *) g_2_h(result_p);
    unsigned int whence = (unsigned int) whence_p;
    off_t offset = ((long)offset_high << 32) | offset_low;

    res = syscall(SYS_lseek, fd, offset, whence);
    *result = res;

    return res<0?res:0;
}
