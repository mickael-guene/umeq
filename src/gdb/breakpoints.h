#ifndef GDB_BREAKPOINTS_H
#define GDB_BREAKPOINTS_H

#if 1
void gdb_insert_breakpoint(unsigned int addr, int len, int type);
void gdb_remove_breakpoint(unsigned int addr, int len, int type);
void gdb_remove_all();
unsigned int gdb_get_opcode(unsigned int addr);
void gdb_reinstall_opcodes();
void gdb_uninstall_opcodes();
#else
void gdb_insert_breakpoint(unsigned long addr, int len, int type);
void gdb_remove_breakpoint(unsigned long addr, int len, int type);
void gdb_remove_all();
unsigned int gdb_get_opcode(unsigned long addr);
void gdb_reinstall_opcodes();
void gdb_uninstall_opcodes();
#endif

#endif /* GDB_BREAKPOINTS_H */