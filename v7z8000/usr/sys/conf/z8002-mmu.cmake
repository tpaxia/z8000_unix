# Z8002, mode-selected split I/D and privileged page-table/window registers.
set(KERNEL_ROM machine/boards/unixv7/z8002/emurom.s)
set(KERNEL_TRAPS machine/z8000/z8002/trap.s)
set(KERNEL_VECTOR_FLAGS -zg)
set(KERNEL_CPPFLAGS -DZ8002_MMU)
set(KERNEL_DATA_LIMIT 0x8000)
set(KERNEL_ASM machine/z8000/z8002/krt.s machine/mmu/paged/pagert.s)
set(KERNEL_MACHINE_C machine/z8000/cpu.c machine/z8000/trap.c machine/mmu/paged/paged.c
    machine/mmu/paged/z8002/access.c machine/z8000/fpe.c machine/boards/unixv7/devices.c machine/boards/unixv7/dump.c)
set(KERNEL_FPE_ADAPTER machine/z8000/z8002/unix.s)
set(KERNEL_FPE_LINK_FLAGS -T 32768 -M 49152)
set(KERNEL_DRIVERS md hd cons mem sys)
set(KERNEL_TEST_FILE conf/z8002-tests.cmake)
