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

struct timespec_32 {
   int32_t tv_sec;                /* seconds */
   int32_t tv_nsec;               /* nanoseconds */
};

struct timeval_32 {
   int32_t tv_sec;                /* seconds */
   int32_t tv_usec;               /* microseconds */
};

struct rlimit_32 {
	uint32_t rlim_cur;
	uint32_t rlim_max;
};

struct rlimit64_32 {
	uint64_t rlim_cur;
	uint64_t rlim_max;
};

struct rusage_32 {
	struct timeval_32 ru_utime;	/* user time used */
	struct timeval_32 ru_stime;	/* system time used */
	int32_t	ru_maxrss;		/* maximum resident set size */
	int32_t	ru_ixrss;		/* integral shared memory size */
	int32_t	ru_idrss;		/* integral unshared data size */
	int32_t	ru_isrss;		/* integral unshared stack size */
	int32_t	ru_minflt;		/* page reclaims */
	int32_t	ru_majflt;		/* page faults */
	int32_t	ru_nswap;		/* swaps */
	int32_t	ru_inblock;		/* block input operations */
	int32_t	ru_oublock;		/* block output operations */
	int32_t	ru_msgsnd;		/* messages sent */
	int32_t	ru_msgrcv;		/* messages received */
	int32_t	ru_nsignals;		/* signals received */
	int32_t	ru_nvcsw;		/* voluntary context switches */
	int32_t	ru_nivcsw;		/* involuntary " */
};

struct iovec_32 {
	uint32_t iov_base;
    uint32_t iov_len;
};

#endif

#ifdef __cplusplus
}
#endif
