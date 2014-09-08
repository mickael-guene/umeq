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

#endif

#ifdef __cplusplus
}
#endif
