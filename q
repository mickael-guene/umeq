* [33md73809a[m[33m ([1;36mHEAD[m[33m, [1;31morigin/master[m[33m, [1;31morigin/HEAD[m[33m, [1;32mtmp[m[33m)[m Allow to run Static.libc.Hello.a.out.O0.arm
* [33mbc095de[m WIP to backup
* [33m6186517[m WIP commit before holidays
* [33mab0055c[m Add top level testes
* [33m68f7b2b[m Allow to run static/nolib/helloloop
* [33m85ec94a[m Allow to run static/nolib/hello thumb version
* [33ma0ac217[m come back to maxInsn
* [33mb931679[m Allow to run static/nolib/hello arm version
* [33mbe0bbfd[m Add runtime (main + support) and basic arm skeleton
* [33mfdfdf95[m Add basic 32 bits loader
* [33mffb1e16[m Remove arm reference in cache since it's generic
* [33m0d2f858[m Add cache testes
* [33m2f44570[m Add cache library
* [33m39faee5[m Add git ignore file
* [33m43ada30[m Don't override CMAKE_C_FLAGS/CMAKE_CXX_FLAGS but augment them
* [33m1184a5e[m Add testsuite for jitter lib
* [33m41d05b7[m Update file hierarchy
* [33m84475bc[m Add jitter lib
* [33mc6012ab[m Initial commit
* [33m6a19886[m[33m ([1;31morigin/backup[m[33m)[m Add missing commit
* [33m6252fd6[m Increase stack size since MAP_GROWSDOWN seems not to work
* [33m8f4c6da[m Add remainder to implement network ancilliary data
* [33mab65628[m Add quotactl syscall support
* [33mc3bfbb0[m Fix timeout support for futex
* [33m4626527[m Fix bug into cache reset handling
* [33m8b78b31[m Fix bug into rt_sigtimedwait_arm due to missing parameter
* [33m7b30555[m Fix recvmsg_arm/sendmsg_arm when more than one iovec
* [33mb48f825[m Add sync_file_range2 syscall
* [33m0325293[m Add pselect6 syscall
* [33m9e0dd90[m Add lgetxattr syscall
* [33mf1be58c[m  Implement ioctl using i386 kernel syscall to use compatibility mode.
* [33m709f165[m Allow to call cleanCaches from a signal handler
* [33mb969da2[m Add IPC_SET support for shmctl
* [33mf90bb9b[m Add semop syscall
* [33m3c021dc[m Add begin of semctl syscall support
* [33mf8fddb7[m Fix bugs for recvmsg_arm and sendmsg_arm
* [33m86a4d8b[m Add fcntl syscalls
* [33m618d6db[m Add stat and fstat syscalls
* [33mf66606f[m Add non atomic swp instruction support
* [33mba614c1[m Add rt_sigtimedwait syscall
* [33ma6d091d[m Add smulxy opcode
* [33mf836e03[m Fix important class of bug. Be sure to clean cache when we map are (could be done only if x is set)
* [33mffc67c0[m Add PTRACE_GET_THREAD_AREA support
* [33m3c07d8b[m Add ptrace support to allow target gdb usage
* [33m2d1d93c[m Fix compile bug
* [33m12f7f95[m Always call dumpState
* [33m3baf744[m Fix bug into arm_hlp_multiply_flag_update which scratch cpsr flags
* [33m26bcdd6[m Add IPC_STAT command for shmctl syscall
* [33m83e69f7[m Fix return value of llseek
* [33m4df833f[m dynamic calculation of emulated stack size
* [33m8e7d05e[m Add support when reading /proc/self/auxv
* [33m83de9a5[m Force OPENSSL_armcap environment variable so that openssl don't use dynamic cpu feature detection.
* [33mbfa1cab[m Add support to generate illegal instructions
* [33m721fc30[m Fix mremap syscall
* [33m5939b33[m Add a custom uname machine description
* [33mb36c47a[m add waitid syscall
* [33m4dbfa42[m add ustat syscall
* [33m8312ab6[m add truncate64 syscall
* [33m9badd71[m add timer_gettime syscall
* [33mb29ee05[m add timer_getoverrun syscall
* [33m70a5f51[m add timerfd_settime and timerfd_gettime syscalls
* [33m1240f68[m Add sysfs syscall
* [33m03e66d2[m Add signalfd4 syscall
* [33md3db27e[m Add setuid syscall
* [33me583e35[m Add setreuid syscall
* [33mcc4aa05[m Add setregid syscall
* [33m6c8fd36[m Add setgid syscall
* [33m6f29113[m Add setfsuid syscall
* [33mc452880[m Add setfsgid syscall
* [33m518b47f[m Add sendfile64 syscall
* [33m03a0019[m Add sendfile syscall
* [33md34b18a[m Add sched_getscheduler syscall
* [33m00f8ff2[m Add sched_setparam syscall
* [33m062380f[m Add sched_setscheduler syscall
* [33m1b37c61[m Add sched_getparam syscall
* [33m8331a8e[m Add rt_sigqueueinfo syscall
* [33mfec8904[m Add rt_sigpending syscall
* [33mbe3b94e[m Add pwrite64 and readv syscall
* [33mb2304e1[m Add mincore syscall
* [33m5b966fd[m Add msync syscall
* [33m37fe94e[m Add mknodat syscall
* [33m13fdb9d[m Add setrlimit syscall
* [33m012e6bc[m Add some shm syscalls
* [33md5dfe4b[m Add getuid syscall
* [33mf41b221[m Add getitimer syscall
* [33m8c8a740[m Add getgroups syscall
* [33m853db99[m Add setgroups32 syscall
* [33m65acf1f[m Add getgid syscall
* [33m38df980[m Add geteuid syscall
* [33m549556a[m Add getegid syscall
* [33mdf563ed[m Refix getdents in error cases
* [33mbcf2ccb[m Add futimesat syscall
* [33m3de2d2c[m Add clock_nanosleep syscall
* [33m688ba32[m Add capset syscall
* [33mf26e9ff[m Add bdflush syscall as not supported by arch
* [33mf1f0df1[m Add add_key syscall
* [33m7ed6a46[m Add infra support for signal_sigaction handler
* [33m73b159d[m Fix bug in writev
* [33ma6486d9[m Increase robustness for sysinfo
* [33m46d1084[m Increase robustness for statfs
* [33m95fac18[m Increase robustness for setitimer
* [33mfdf965b[m Fix return value of llseek
* [33mf9a1655[m allow syscall test to pass  for gettimeofday
* [33m404c2c2[m Increase robustness for getrusage
* [33mdb8d643[m Increase robustness for getrlimit
* [33m4740809[m Improve error cases for getdents
* [33m7ecf624[m Increase robustness for fstatfs
* [33m663e5c2[m Fix lot of problems for fcntl
* [33m08664ef[m Fix bug into fallocate syscall. Use arm 64 bits parameters
* [33m7f87201[m Fix bug into getrlimit syscall
* [33m4b7ca32[m Fix bug into clock_getres syscall
* [33m4cca6b8[m Only force write page when under the debugger
* [33m5513a0b[m Add times syscall
* [33md9d7dea[m Fix bug into clock_gettime + clock_getres
* [33m38ebb27[m Add sendmsg syscall
* [33m7b51e5b[m Add accept syscall
* [33m2e381c0[m Add socketpair syscall
* [33m1612e73[m Add recvmsg syscall
* [33m8ab1719[m Add socket4 + getsockopt(not sure its portable) syscall
* [33mca13b79[m Add epoll_ctl + epoll_wait syscall
* [33m21999c9[m Add eventfd2 syscall
* [33me2f92a3[m Add epoll_create1 syscall
* [33m4165f50[m Add clock_getres syscall
* [33m2b00df6[m Add pipe2 syscall
* [33mbeb029e[m add creat and rt_sigsuspend syscall
* [33m2672dab[m getdents64_arm now return d_type
* [33ma185ef0[m Add fstatfs and fix bug into prlimit64
* [33m97ba516[m Add fstatfs syscall
* [33m2d9c071[m Fix bug in fstatfs64_arm
* [33me8fdc08[m Add timer_settime syscall and fix timer_create_arm
* [33mbc6656f[m Add partial implementation of imer_create syscall
* [33m44a8014[m Add removexattr syscall
* [33m82d6ef7[m Add flistxattr syscall
* [33mef9b645[m Add llistxattr syscall
* [33m4284fff[m Add listxattr syscall
* [33m4a8fc92[m Add fgetxattr syscall
* [33m3dc29f2[m Add getxattr syscall
* [33m9ef9b09[m Add capget syscall
* [33m669e465[m Fix latent bug into timeout value for poll
* [33mf68d6b3[m fix bug into loader
* [33m684d920[m Add sigaltstack syscall support
* [33m4902cf3[m IMPORTANT : siglongjump is a BIG problem for current architecture. try to mitigate problem ...
* [33ma849eb2[m Add semget syscall support
* [33m6aa6c62[m Add prctl syscall support
* [33mf2659b7[m Add rsc support to arm_hlp_compute_next_flags
* [33mc0ff532[m Add -U command + avoid multiple environment vairable definition
* [33m81bf189[m Add F_SETLK64 command for fcntl
* [33m3551237[m Add recv syscall
* [33m8338fad[m Add fcntl F_DUPFD_CLOEXEC command
* [33m537cc89[m Map vdso page so kuser_helper_version can be read
* [33m7e1e3d0[m Return an error when unable to load a binary
* [33m104af1e[m Add ftruncate64 syscall + fix pread64
* [33m0f44653[m Add pread64 syscall
* [33m267b23d[m Cast syscall parameters to long
* [33ma046b12[m Fix bug in thread clone
* [33mc612b55[m Increase cache size
* [33mc8accb8[m Add handling of -E, -0 and -g command line options for umeq
* [33m67c3cce[m Add sched_getaffinity syscall
* [33m6e7b384[m Add fsetxattr syscall
* [33m7e58efb[m Add eor support for flags
* [33m9fe818a[m Add statfs syscall
* [33m285af33[m Add utimensat syscall
* [33mc6b6514[m Add getrusage
* [33m6541309[m use fork to emulate vfork. Do we need to add a synchro here ?
* [33m348f76d[m Move umeq further in v space to avoid mapping problem
* [33mc95907c[m Fix bug in mapSegment
* [33m3279c3a[m Add prlimit64 syscall
* [33m0ec3447[m Quick and dirty commit to allow umeq to be launch as qemu with proot
* [33mba5fa09[m Add writev syscall
* [33mf673532[m Add support to smlaxy
* [33m7596897[m Add null arm_fadvise64_64 syscall implementation
* [33mcc205ac[m Add fstatfs64 syscall
* [33mae1907e[m Add fstatat64 syscall
* [33mf931097[m Improve fcntl syscall
* [33m66ee4df[m Add _new_select syscall (at least try ...)
* [33m005091e[m Add getdents syscall
* [33m2150be0[m Add rsc support into mk_data_processing
* [33ma29d311[m Add clock_gettime syscall
* [33m7e7f48b[m Add getxattr syscall
* [33mdd19b84[m Add setsockopt syscall
* [33mbfc97d8[m Add fchmod syscall
* [33mecbf593[m Add mremap syscall
* [33mdd06a90[m Add mknod syscall
* [33m67f9844[m Add vfork syscall
* [33m6f44136[m Add recvfrom syscall
* [33mee67ccd[m Add getsockname syscall
* [33mce67d7d[m Add bind syscall
* [33mcf41bcb[m Add dis_load_signed_halfword_byte_register_offset support
* [33mf4df3b2[m Add recvfrom syscall
* [33m412f31e[m Add send syscall
* [33m1647431[m Fixing for the last time? execve
* [33m0c83843[m Still fixing execve
* [33m5cd0de7[m Add connect syscall
* [33m52e0648[m clean caches after bp insert or delete
* [33mbe52930[m Force write in map so we can insert breakpoints
* [33mc5f9849[m Fix bug into openat
* [33m9d8dace[m Allow gdb to reconnect immediatly
* [33mf390ffb[m fix execve
* [33m1cd9b3b[m Fix bug in loader when bss but not need to map more memory ....
* [33mf2a6033[m Fix bug into loader
* [33m4e2d83f[m Fix loader bug when bss larger than mapped file
* [33mb484b34[m gdb support is back again
* [33m122bdd2[m Fix bug in case we use %s in printf
* [33ma91da73[m Fix bug whith gdb and disassemble of more that 1 instruction length
* [33m093ef2f[m re-implement raw clone now we don't have libc
* [33mc553ffb[m Implement basic mutex lock/unlock but need some cleanup
* [33ma395408[m  Add custom printf + manual use of tls to save previous thread context when in signal
* [33mb9952b3[m Allow umeq to work for basic test
* [33m63f26e0[m Allow to compile without clib
* [33mabed103[m Add utimes syscall
* [33m9ef3577[m Add mkdir support
* [33m4d9018d[m Add getgroups32 support
* [33m0dc2ad1[m Add accumulation support for smlal and umlal
* [33m1a523d6[m Add statfs64 syscall
* [33mb528aef[m Disable cache for interpreted be since not working
* [33m058578e[m Add sysinfo call
* [33mf57ed31[m Implement sco for ror+rrx and fix latent bug
* [33m8c4c4ca[m Disable cache with interpreted backend
* [33m057093f[m Add smc support when using caches
* [33mac5ba7d[m Improve threading sp pthread_join work
* [33md3feb58[m Add some infra for smc support with cache
* [33mb5a7358[m Add cache support
* [33m4cc1473[m By default compile in O2
* [33m5fe38bb[m Fix bug that appear when compile into O2
* [33m94223de[m Drop unused read_context result
* [33m81af7b3[m Increase number of disassemble insn from 1 to 10
* [33m8f24fe3[m use x86_64 backend
* [33ma7fcbc7[m binop need masking to avoid overflow
* [33mb19257e[m Add code to help debugging register alloc
* [33m4ce3e71[m Fix bug that prevent register free
* [33mcf65ad5[m Add param 4 support and save caller save regs before calling helper
* [33m647fd42[m add more ir call testes
* [33m56e76f0[m add possibility to use x86_64 backend for umeq
* [33mc5f2075[m add read/write context ir support for x86_64 backend
* [33mb1a064e[m add partial call ir support for x86_64 backend
* [33m32bce8d[m add full exit ir support for x86_64 backend
* [33me21f4eb[m add exit ir testes
* [33m278fb2c[m add function call testes
* [33m852f3a7[m add read/write context testes
* [33m2138988[m Add cast ir support for x86 backend
* [33m380ca1e[m Add cmpeq ir and cmpne ir support for x86 backend
* [33mf1fa8fe[m Add ite ir support for x86 backend
* [33m4f83828[m Add asr ir support for x86 backend
* [33m89a5818[m Add shr ir for x86_64 backend
* [33m8859bf0[m Add shl ir for x86_64 backend
* [33m5b43008[m Add xor,and,or and ir for x86_64 backend
* [33ma5d92c6[m Add sub ir for x86_64 backend
* [33m22f22ba[m Add support for add ir
* [33m3f674b2[m Fix problem of exec stack
* [33m66b2cff[m Allow unitary const test to run with x86_64 backend
* [33m26cfcb4[m Add wip on x86_64 backend
* [33m24d5042[m backend execute function must not use a backend context so it be execute in multiple threads
* [33m5d761f4[m Fix bug in thread creation
* [33mb7832c5[m Remove dependency of jitter for execute() for interpreted back-end
* [33mf17cece[m Syscall params must be long
* [33mdfe8bab[m Build as a static to avoid library clash
* [33mf15df54[m Add thread support
* [33mbbd6866[m Add getrlimit syscall
* [33meffeffe[m Add rt_sigprocmask
* [33m58a5ca9[m Add unimplemented set_robust_list
* [33m3c3bb92[m Add dummy cacheflush syscall
* [33m72b2530[m Add setitimer and sigreturn syscall
* [33m3c5f60d[m Add fork and exec support
* [33meea4128[m add poll support
* [33mfd47f77[m add wait4 support
* [33m907fc75[m add sigaction support
* [33m417898b[m split arm syscall implementation in different files
* [33m36793e5[m Start to build running loop
* [33m00d4c96[m Add syscall translation table
* [33m3c1fdbd[m Move syscall.S to jitter directory
* [33m68138e9[m Embedded my own syscall function that don't set errno
* [33me183066[m Add sysinfo and nanosleep syscall
* [33mf52ef0d[m add missing environment variables copy
* [33mc87b634[m add mvn flags
* [33mbe07855[m add register ror
* [33m06e7864[m add ror and signed multiplication
* [33m999d55e[m adding syscalls
* [33m98d4800[m Fix multiple bugs into load/store half and double
* [33m5122862[m Add dis_load_store_halfword_register_offset and bic flags
* [33m3b33b20[m Add read syscall
* [33ma72b7af[m Add extented rotate right
* [33m741327a[m Allow ls to work
* [33md713c91[m add load store double register offset support
* [33m4c23d8c[m continue to add syscall to ls work
* [33mefaf192[m Implement syscalls that allow to see busybox help
* [33mdbba2a8[m Add adc support for flags
* [33m71424f3[m Add pc support for dis_load_store_imm_offset
* [33mff0dec5[m Add mull and fix mul
* [33mb4e0c90[m Add get_tls vdso support
* [33mdc06ef3[m Fix some bugs in flags
* [33m4443700[m Add exit_group syscall
* [33m8e567a6[m Add load store half work with immediate offset
* [33m0fb3c0f[m Add sbc
* [33m437d5f6[m Add teq support
* [33m45544ce[m Add load of signed byte/halfword
* [33m6704616[m Add mmap syscall
* [33m06a7a67[m Add ubfx
* [33maa5596d[m Move arm syscall in their own file
* [33m45ec7ac[m Add vdso_memory_barrier
* [33m4f8b9c1[m Add adc
* [33m6c706c4[m Add rsb in flags + update missing  gdb_handle_breakpoint call
* [33ma0e232c[m All external fonction called are given context as first parameters
* [33m0ef55ae[m Add blx
* [33m073e429[m Add sxth
* [33m30b0738[m Add load/store double immediate and orr support in flags
* [33m14c9cbc[m Add clz
* [33m13c858f[m add eor
* [33m12a8dd9[m Add pc load in load_store_register_offset
* [33m39e311c[m Add support of pc in mk_data_processing and fix bug in startup stack layout
* [33m12b9427[m Add pld
* [33m6b5b133[m Add bfc
* [33mf903ca1[m Add set_tls syscall
* [33m1ca072c[m Continue adding insn + bug. Add sco support for cpsr
* [33m9ce2632[m sbrk emulation add
* [33mc07abbd[m Continue to add opcode and to fix bugs
* [33m1b67035[m Add uxtb
* [33m55c6da9[m Add load/store byte to arm
* [33m993252d[m Add arm insn, fix bugs ...
* [33m68d9d5e[m Try to avoid to access unmapped memory area for debugger
* [33mc18923b[m Add basic gdbserver support
* [33mc532333[m Add target context read/write instruction to allow jitter code usage accross threads
* [33mb9424dd[m populate target stack
* [33m998505a[m Add ldm/stm and immediate shift
* [33m8616b6f[m Add support for conditionnal instructions
* [33m7127dfd[m use pc return by excecute loop to start new disassembly sequence
* [33m290870d[m Can run miniHelloWorld test case
* [33m447471a[m Start arm v5 target
* [33m33eb9e3[m Add narrowing operators
* [33m9326a59[m Add widening operators
* [33md73e6e2[m Add comparaison operator
* [33m1042f95[m Add if then else (ite) operator
* [33mef2503e[m Add shift operators
* [33mb486d72[m Add xor, and, or opcode to ir
* [33mdf6c96f[m Remove constraint about op2 size from binop.
* [33md4bb787[m Add sub op + testsuite
* [33m38cdc79[m Refactor jitter test Fixture
* [33me124cfd[m Working memory is provide by user so it can be on stack
* [33m7062277[m Use a binop operator to factorize code. Add add testcode
* [33m9934a84[m Add gtest environment + const testsuite
* [33m1f77be1[m move jitter code into a library
* [33m4d37d3a[m Add call of helper
* [33m51d140e[m Add reset support
* [33mc530b9f[m Initial commit
