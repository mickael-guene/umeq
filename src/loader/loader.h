#ifndef LOADER_H
#define LOADER_H

struct load_auxv_info {
	void *load_AT_PHDR; /* Program headers for program */
	unsigned int load_AT_PHENT; /* Size of program header entry */
	unsigned int load_AT_PHNUM; /* Number of program headers */
	void *load_AT_ENTRY; /* Entry point of program */
};

extern void *load32(const char *file, struct load_auxv_info *auxv_info);

#endif

