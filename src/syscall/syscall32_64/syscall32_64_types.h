#ifdef __cplusplus
extern "C" {
#endif

#ifndef __SYSCALL32_64_TYPES__
#define __SYSCALL32_64_TYPES__ 1

#include <sys/syscall.h>
#include <sys/types.h>
#include <stdint.h>

#define g_2_h(ptr)  ((uint64_t)(ptr))
#define h_2_g(ptr)  ((ptr))

struct stat64_32 {
	uint64_t	st_dev;
	uint8_t   __pad0[4];

	uint32_t	__st_ino;
	uint32_t	st_mode;
	uint32_t	st_nlink;

	uint32_t	st_uid;
	uint32_t	st_gid;

	uint64_t	st_rdev;
	uint8_t   __pad3[4];

	int64_t		st_size;
	uint32_t	st_blksize;
	uint64_t 	st_blocks;

	uint32_t	_st_atime;
	uint32_t	st_atime_nsec;

	uint32_t	_st_mtime;
	uint32_t	st_mtime_nsec;

	uint32_t	_st_ctime;
	uint32_t	st_ctime_nsec;

	uint64_t	st_ino;
};

struct linux_dirent_32 {
	uint32_t	d_ino;
	uint32_t	d_off;
	uint16_t	d_reclen;
	char		d_name[1];
};

struct linux_dirent64_32 {
	uint64_t	d_ino;
	uint64_t	d_off;
	uint16_t	d_reclen;
    uint8_t	    d_type;
	char		d_name[1];
};

struct linux_dirent_64 {
	uint64_t	d_ino;
	uint64_t	d_off;
	uint16_t	d_reclen;
	char		d_name[1];
};

#endif

#ifdef __cplusplus
}
#endif
