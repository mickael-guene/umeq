#include <stdlib.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __GDB__
#define __GDB__ 1

enum gdb_state {
	GDB_STATE_SYNCHRO,
	GDB_STATE_DATA,
	GDB_STATE_CHECKSUM_1,
	GDB_STATE_CHECKSUM_2
};

struct gdb {
    enum gdb_state state;
    int commandPos;
    char command[1024];
    int isContinue;
    int isSingleStepping;
    void (*read_registers)(struct gdb *gdb, char *buf);
};

int gdb_tohex(int v);
int gdb_handle_breakpoint(struct gdb *gdb);

#endif

#ifdef __cplusplus
}
#endif
