# Z8000 Unix

Porting Unix Seventh Edition to the Zilog Z8001 segmented microprocessor, with a custom paged MMU.

## Background

The Zilog Z8000 was a 16-bit microprocessor introduced in 1979. It came in two variants: the Z8002 (non-segmented, 64KB address space) and the Z8001 (segmented, 8MB address space via 7-bit segment numbers + 16-bit offsets). Several groups ported Unix to the Z8000 between 1980 and 1983:

| System | Processor | Unix Version | Year |
|--------|-----------|--------------|------|
| Onyx C8002 (ONIX) | Z8002 | V7 | 1980 |
| Zilog System 8000 (ZEUS) | Z8001 | V7 + BSD | 1981 |
| Central Data (Xenix) | Z8001 | Xenix | 1981 |
| Commodore 900 (Coherent) | Z8001 | V7 work-alike | 1983 |
| EAW P8000 (WEGA) | U8001 (clone) | System III | 1987 |

The most relevant precedent is the Onyx C8002, which ported V7 Unix with only 60 lines of C code changes by using the non-segmented Z8002 with a custom paged MMU. This made the architecture PDP-11-like, keeping the port straightforward.

See [doc/PCC-Research.md](doc/PCC-Research.md) for the full compiler research and historical analysis.

## Design Decisions

### Memory Model: Non-Segmented Kernel

The kernel and user processes run in NONSEG mode (16-bit pointers). The Z8001 CPU automatically maps non-segmented 16-bit addresses using the current PC's segment, so C code compiled for Z8002 works correctly without modification. This avoids the complexity of 32-bit segmented pointers, where `sizeof(char *) = 4` but `sizeof(int) = 2` — a mismatch that would require extensive changes to V7 code that conflates ints and pointers.

Trap handlers must briefly enter SEG+SYS mode (forced by CPU hardware on trap entry) to access the segmented system stack pointer (RR14), then switch to NONSEG+SYS for C code execution.

### MMU: Segment Numbers as Map Set Selectors

Instead of using the Z8001's segmentation with Zilog's Z8010 base+limit MMU, the 7-bit segment number is repurposed as a map set selector for a custom paged MMU. This combines hardware-assisted context selection with fine-grained paged translation: each segment number selects a map set, context switching is free (segment number is embedded in PC), and the kernel accesses user memory by constructing pointers with the target process's segment number.

See [doc/z8001_mmu_design_notes.md](doc/z8001_mmu_design_notes.md) for the full design.

### Syscall Convention

System calls use the Z8000 `sc` instruction. The syscall number is encoded in the tag word, arguments are passed in registers (R1-R3), and the return value comes back in R0. Dispatch uses a V7-style `sysent[]` function-pointer table.

### Compiler: ACK

The Amsterdam Compiler Kit was chosen because it already has a Z8000 code generator backend and its ANSI C frontend compiles V7 K&R C source unchanged. The original backend only supported Z8001 segmented mode; we extended the assembler, code generator, and runtime libraries to generate non-segmented code, and changed the default build to compile all libraries in non-segmented mode.

See [doc/ack-compiler.md](doc/ack-compiler.md) for details.

### Emulator

The Z8000 software emulator is used as a library with a custom front end (`kernel/test_driver.cpp`) that can simulate I/O and load code segments and data from files at arbitrary physical addresses without needing bootstrap code. This simplifies development considerably — the full kernel trap round-trip can be tested without a real boot ROM or hardware.

See [doc/z8000-emulator.md](doc/z8000-emulator.md) for details.

## Building and Testing

Prerequisites: z8k-coff binutils, ACK built with Z8000 support, C++17 compiler.

```sh
cd ack && gmake HOSTCC=cc CC=cc -j8 NINJA='ninja -k0'   # build ACK
cd kernel && make test                                    # build + run kernel test
```

The test verifies R0 == 42 (exit status) and console output == "Hello from Z8000 Unix!\n".

## Files

| File | Purpose |
|------|---------|
| `kernel/rom.s` | Reset vector + init code (segment 0) |
| `kernel/trap.s` | PSA table + syscall entry/exit stubs + test code |
| `kernel/krt.s` | Kernel runtime stub: entry trampoline, `putc()` |
| `kernel/syscall.c` | Syscall dispatch, `sysent[]` table, `write()`, `exit()`, `cons_write()` |
| `kernel/test_driver.cpp` | Emulator-based test driver |
| `kernel/Makefile` | Build rules for all components |
| `tests/run_test.sh` | ACK C test runner for standalone programs |
| `v7unix/` | V7 Unix source tree (from TUHS, to be modified for Z8000) |
| `ack/` | ACK submodule (tpaxia/ack fork, z8000unix branch) |
| `z8000_emu/` | Z8000 emulator submodule |

## Documentation

| Document | Contents |
|----------|----------|
| [doc/implementation-steps.md](doc/implementation-steps.md) | Step-by-step implementation journal |
| [doc/kernel-technical-reference.md](doc/kernel-technical-reference.md) | Kernel internals: PSA table, CPU modes, SYSCALL flow, stack layout |
| [doc/ack-compiler.md](doc/ack-compiler.md) | ACK compiler: Z8000 backend, Z8002 extensions, known limitations |
| [doc/z8000-emulator.md](doc/z8000-emulator.md) | Z8000 software emulator and its use in the project |
| [doc/z8001_mmu_design_notes.md](doc/z8001_mmu_design_notes.md) | Custom paged MMU design using Z8001 segment numbers as map set selectors |
| [doc/PCC-Research.md](doc/PCC-Research.md) | Compiler research: PCC history, ACK assessment, Z8000 Unix history |
