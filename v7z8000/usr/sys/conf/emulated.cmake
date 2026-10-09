# Custom paged-MMU Z8001 machine implemented by emu/test_driver.cpp.
set(KERNEL_ROM machine/emurom.s)
set(KERNEL_TRAPS machine/trap.s)
# First object supplies the fixed trap-to-C entry table at 0x0200.
set(KERNEL_ASM machine/krt.s machine/pagert.s)
set(KERNEL_MACHINE_C
    machine/cpu.c machine/trap.c machine/fpe.c machine/paged.c
    conf/emulated.c machine/dump.c)
set(KERNEL_DRIVERS md hd cons mem sys)
set(KERNEL_OPTIONAL_C sys/fakemx.c)
set(KERNEL_TEST_FILE conf/emulated-tests.cmake)
