#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <unistd.h>
#include <sys/syscall.h>   /* For SYS_xxx definitions */
#include <string.h>
#include <errno.h>

#include "runtime.h"
#include "syscall32_64_types.h"
#include "syscall32_64_private.h"

struct linux_dirent_x86 {
	uint64_t	d_ino;
	uint64_t	d_off;
	uint16_t	d_reclen;
	char		d_name[1];
};

int getdents64_s3264(uint32_t fd_p, uint32_t dirp_p, uint32_t count_p)
{
	unsigned int fd = fd_p;
	struct linux_dirent64_32 *dirp = (struct linux_dirent64_32 *) g_2_h(dirp_p);
	unsigned int count = count_p;
	char buffer[4096];
	struct linux_dirent_64 *dirp_64 = (struct linux_dirent_64 *) buffer;
	int res_32 = 0;
	int res;

	res = syscall(SYS_getdents, fd, buffer, sizeof(buffer));

	res_32 = res<0?res:0;
	if (res_32 >= 0) {
        if (dirp_64->d_reclen - 8 >= count)
            res_32 = -EINVAL;
        else {
	        while(res > 0 && count > dirp_64->d_reclen + 1) {
		        dirp->d_ino = dirp_64->d_ino;
		        dirp->d_off = dirp_64->d_off;
		        dirp->d_type = *(((char *)dirp_64) + dirp_64->d_reclen - 1);
		        dirp->d_reclen = dirp_64->d_reclen + 1;
		        // we copy according to dirp_64->d_reclen size since there is d_type hidden field
		        memcpy(dirp->d_name, dirp_64->d_name, dirp_64->d_reclen - 8 - 8 - 2);
		        res -= dirp_64->d_reclen;
                dirp_64 = (struct linux_dirent_64 *) ((long)dirp_64 + dirp_64->d_reclen);
                res_32 += dirp->d_reclen;
                count -= dirp->d_reclen;
                dirp = (struct linux_dirent64_32 *) ((long)dirp + dirp->d_reclen);
	        }
        }
    }

	return res_32;
}

int getdents_s3264(uint32_t fd_p, uint32_t dirp_p, uint32_t count_p)
{
	unsigned int fd = (unsigned int) fd_p;
	struct linux_dirent_32  *dirp = (struct linux_dirent_32 *) g_2_h(dirp_p);
	unsigned int count = (unsigned int) count_p;
	char buffer[4096];
	struct linux_dirent_64 *dirp_64 = (struct linux_dirent_64 *) buffer;
	int res_32 = 0;
	int res;

	res = syscall(SYS_getdents, fd, buffer, sizeof(buffer));

    res_32 = res<0?res:0;
    if (res_32 >= 0) {
        if (dirp_64->d_reclen - 8 >= count)
            res_32 = -EINVAL;
        else {
	        while(res > 0 && count > dirp_64->d_reclen - 8) {
		        dirp->d_ino = dirp_64->d_ino;
		        dirp->d_off = dirp_64->d_off;
		        dirp->d_reclen = dirp_64->d_reclen - 8;
		        // we copy according to dirp_x86->d_reclen size since there is d_type hidden field
		        memcpy(dirp->d_name, dirp_64->d_name, dirp_64->d_reclen - 8 - 8 - 2);
		        res -= dirp_64->d_reclen;
                dirp_64 = (struct linux_dirent_64 *) ((long)dirp_64 + dirp_64->d_reclen);
                res_32 += dirp->d_reclen;
                count -= dirp->d_reclen;
                dirp = (struct linux_dirent_32 *) ((long)dirp + dirp->d_reclen);
	        }
        }
    }

	return res_32;
}
