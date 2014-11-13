#ifndef LOADER_H
#define LOADER_H

#include "target64.h"

struct load_auxv_info_64 {
	guest_ptr load_AT_PHDR; /* Program headers for program */
	unsigned int load_AT_PHENT; /* Size of program header entry */
	unsigned int load_AT_PHNUM; /* Number of program headers */
	guest_ptr load_AT_ENTRY; /* Entry point of program */
};

extern guest_ptr load64(const char *file, struct load_auxv_info_64 *auxv_info);

#endif
