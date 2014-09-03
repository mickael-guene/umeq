#ifdef __cplusplus
extern "C" {
#endif

#ifndef __SYSCALL32_64_PRIVATE__
#define __SYSCALL32_64_PRIVATE__ 1

extern int fstat64_s3264(uint32_t fd, uint32_t buf);
extern int getdents64_s3264(uint32_t fd, uint32_t dirp, uint32_t count);

#endif

#ifdef __cplusplus
}
#endif
