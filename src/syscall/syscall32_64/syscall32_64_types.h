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

typedef struct {
	int32_t	val[2];
} __kernel_fsid_32_t;

struct statfs_32 {
	uint32_t f_type;
	uint32_t f_bsize;
	uint64_t f_blocks;
	uint64_t f_bfree;
	uint64_t f_bavail;
	uint64_t f_files;
	uint64_t f_ffree;
	__kernel_fsid_32_t f_fsid;
	uint32_t f_namelen;
	uint32_t f_frsize;
	uint32_t f_flags;
	uint32_t f_spare[4];
};

struct flock_32 {
	uint16_t  l_type;
	uint16_t  l_whence;
	uint32_t l_start;
	uint32_t l_len;
	int  l_pid;
};

struct flock64_32 {
	uint16_t  l_type;
	uint16_t  l_whence;
	uint64_t l_start;
	uint64_t l_len;
	int  l_pid;
};

struct msghdr_32 {
    uint32_t msg_name;
    uint32_t msg_namelen;
    uint32_t msg_iov;
    uint32_t msg_iovlen;
    uint32_t msg_control;
    uint32_t msg_controllen;
    uint32_t msg_flags;
};

struct sysinfo_32 {
    int32_t uptime;
    uint32_t loads[3];
    uint32_t totalram;
    uint32_t freeram;
    uint32_t sharedram;
    uint32_t bufferram;
    uint32_t totalswap;
    uint32_t freeswap;
    uint16_t procs;
    uint32_t totalhigh;
    uint32_t freehigh;
    uint32_t mem_unit;
};

struct itimerval_32 {
   struct timeval_32 it_interval; /* next value */
   struct timeval_32 it_value;    /* current value */
};

union sigval_32 {
    uint32_t sival_int;
    uint32_t sival_ptr;
};

struct sigevent_32 {
    union sigval_32 sigev_value;
    uint32_t sigev_signo;
    uint32_t sigev_notify;
    /* FIXME : union for SIGEV_THREAD+ SIGEV_THREAD_ID */
};

struct itimerspec_32 {
    struct timespec_32 it_interval;  /* Timer interval */
    struct timespec_32 it_value;     /* Initial expiration */
};

#endif

#ifdef __cplusplus
}
#endif
