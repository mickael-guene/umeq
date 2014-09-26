#ifdef __cplusplus
extern "C" {
#endif

#ifndef __SYSCALL32_64_PRIVATE__
#define __SYSCALL32_64_PRIVATE__ 1

extern int stat64_s3264(uint32_t pathname_p, uint32_t buf_p);
extern int fstat64_s3264(uint32_t fd, uint32_t buf);
extern int fstat64_s3264(uint32_t fd_p, uint32_t buf_p);
extern int getdents64_s3264(uint32_t fd, uint32_t dirp, uint32_t count);
extern int llseek_s3264(uint32_t fd_p, uint32_t offset_high_p, uint32_t offset_low_p, uint32_t result_p, uint32_t whence_p);
extern int clock_gettime_s3264(uint32_t clk_id_p, uint32_t tp_p);
extern int ugetrlimit_s3264(uint32_t resource_p, uint32_t rlim_p);
extern int futex_s3264(uint32_t uaddr_p, uint32_t op_p, uint32_t val_p, uint32_t timeout_p, uint32_t uaddr2_p, uint32_t val3_p);
extern int statfs64_s3264(uint32_t path_p, uint32_t dummy_p, uint32_t buf_p);
extern int fnctl64_s3264(uint32_t fd_p, uint32_t cmd_p, uint32_t opt_p);
extern int lstat64_s3264(uint32_t pathname_p, uint32_t buf_p);
extern int gettimeofday_s3264(uint32_t tv_p, uint32_t tz_p);
extern int wait4_s3264(uint32_t pid_p,uint32_t status_p,uint32_t options_p,uint32_t rusage_p);
extern int execve_s3264(uint32_t filename_p,uint32_t argv_p,uint32_t envp_p);
extern int prlimit64_s3264(uint32_t pid_p, uint32_t resource_p, uint32_t new_limit_p, uint32_t old_limit_p);
extern int writev_s3264(uint32_t fd_p, uint32_t iov_p, uint32_t iovcnt_p);
extern int getrusage_s3264(uint32_t who_p, uint32_t usage_p);
extern int fstatat64_s3264(uint32_t dirfd_p, uint32_t pathname_p, uint32_t buf_p, uint32_t flags_p);
extern int newselect_s3264(uint32_t nfds_p, uint32_t readfds_p, uint32_t writefds_p, uint32_t exceptfds_p, uint32_t timeout_p);
extern int recvmsg_s3264(uint32_t sockfd_p, uint32_t msg_p, uint32_t flags_p);
extern int utimes_s3264(uint32_t filename_p, uint32_t times_p);
extern int statfs_s3264(uint32_t path_p, uint32_t buf_p);
extern int pselect6_s3264(uint32_t nfds_p, uint32_t readfds_p, uint32_t writefds_p, uint32_t exceptfds_p, uint32_t timeout_p, uint32_t data_p);
extern int utimensat_s3264(uint32_t dirfd_p, uint32_t pathname_p, uint32_t times_p, uint32_t flags_p);
extern int nanosleep_s3264(uint32_t req_p, uint32_t rem_p);
extern int sysinfo_s3264(uint32_t info_p);

#endif

#ifdef __cplusplus
}
#endif
