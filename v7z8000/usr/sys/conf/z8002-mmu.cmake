# Z8002, mode-selected split I/D and privileged page-table/window registers.
set(KERNEL_ROM machine/rom2.s)
set(KERNEL_TRAPS machine/trap2.s)
set(KERNEL_VECTOR_FLAGS -zg)
set(KERNEL_CPPFLAGS -DZ8002_MMU)
set(KERNEL_DATA_LIMIT 0x8000)
set(KERNEL_ASM machine/krt2.s machine/pagert.s)
set(KERNEL_MACHINE_C machine/cpu.c machine/trap.c machine/paged.c
    machine/access2.c machine/fpe.c conf/emulated.c machine/dump.c)
set(KERNEL_FPE_ADAPTER fpe/unix2.s)
set(KERNEL_FPE_LINK_FLAGS -T 32768 -M 49152)
set(KERNEL_DRIVERS md hd cons mem sys)
set(KERNEL_TEST_FILE conf/z8002-tests.cmake)
