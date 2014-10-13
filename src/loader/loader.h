#ifndef LOADER_H
#define LOADER_H

#include "target32.h"
#include "target64.h"

struct load_auxv_info_32 {
	guest_ptr load_AT_PHDR; /* Program headers for program */
	unsigned int load_AT_PHENT; /* Size of program header entry */
	unsigned int load_AT_PHNUM; /* Number of program headers */
	guest_ptr load_AT_ENTRY; /* Entry point of program */
};

struct load_auxv_info_64 {
	guest64_ptr load_AT_PHDR; /* Program headers for program */
	unsigned int load_AT_PHENT; /* Size of program header entry */
	unsigned int load_AT_PHNUM; /* Number of program headers */
	guest64_ptr load_AT_ENTRY; /* Entry point of program */
};

extern guest_ptr load32(const char *file, struct load_auxv_info_32 *auxv_info);
extern guest64_ptr load64(const char *file, struct load_auxv_info_64 *auxv_info);

#endif

