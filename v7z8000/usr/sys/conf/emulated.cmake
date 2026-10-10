# Custom paged-MMU Z8001 machine implemented by emu/test_driver.cpp.
set(KERNEL_ROM machine/boards/unixv7/z8001/emurom.s)
set(KERNEL_TRAPS machine/z8000/z8001/trap.s)
# First object supplies the fixed trap-to-C entry table at 0x0200.
set(KERNEL_ASM machine/z8000/z8001/krt.s machine/mmu/paged/pagert.s)
set(KERNEL_MACHINE_C
    machine/z8000/cpu.c machine/z8000/trap.c machine/z8000/fpe.c machine/mmu/paged/paged.c
    machine/boards/unixv7/devices.c machine/boards/unixv7/dump.c)
set(KERNEL_DRIVERS md hd cons mem sys)
set(KERNEL_OPTIONAL_C sys/fakemx.c)
set(KERNEL_TEST_FILE conf/emulated-tests.cmake)
