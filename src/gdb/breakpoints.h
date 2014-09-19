#ifndef GDB_BREAKPOINTS_H
#define GDB_BREAKPOINTS_H

void gdb_insert_breakpoint(unsigned int addr, int len, int type);
void gdb_remove_breakpoint(unsigned int addr, int len, int type);
void gdb_remove_all();
unsigned int gdb_get_opcode(unsigned int addr);
void gdb_reinstall_opcodes();
void gdb_uninstall_opcodes();

#endif /* GDB_BREAKPOINTS_H */