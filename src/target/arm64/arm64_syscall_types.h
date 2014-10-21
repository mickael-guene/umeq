#ifdef __cplusplus
extern "C" {
#endif

#ifndef __ARM64_SYSCALL_TYPES__
#define __ARM64_SYSCALL_TYPES__ 1

#include <sys/syscall.h>
#include <sys/types.h>
#include <stdint.h>

/* x86_64 struct stat is not standard ....
   So we need to convert between arm64 <-> x86_64
*/
struct stat_arm64 {
	uint64_t st_dev;
	uint64_t st_ino;
	uint32_t st_mode;
	uint32_t st_nlink;
	uint32_t st_uid;
	uint32_t st_gid;
	uint64_t st_rdev;
	uint64_t __pad1;
	int64_t st_size;
	int32_t st_blksize;
	int32_t __pad2;
	int64_t st_blocks;
	struct timespec st_atim;
	struct timespec st_mtim;
	struct timespec st_ctim;
	int32_t __unused4;
	int32_t __unused5;
};

#endif

#ifdef __cplusplus
}
#endif