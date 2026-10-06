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

The emulated machine also supports separate instruction/data spaces: `ldz8 -i` produces 0411 executables with up to 64 KB of instruction addresses and 64 KB for data, BSS, heap, and stack. Pointers remain 16-bit. Existing 0407 combined-space programs continue to work. See [the kernel reference](doc/kernel-technical-reference.md#separate-instruction-and-data-spaces).

Floating-point arithmetic uses the historical Zilog software EPU engine from
CP/M-8000, in reserved segment 127. User programs execute EPA instructions
through PCC-compatible wrappers; the engine is not linked into each program.
See [software EPU service](doc/kernel-technical-reference.md#software-epu-service).

Trap handlers must briefly enter SEG+SYS mode (forced by CPU hardware on trap entry) to access the segmented system stack pointer (RR14), then switch to NONSEG+SYS for C code execution.

### MMU: Segment Numbers as Map Set Selectors

Instead of using the Z8001's segmentation with Zilog's Z8010 base+limit MMU, the 7-bit segment number is repurposed as a map set selector for a custom paged MMU. This combines hardware-assisted context selection with fine-grained paged translation: each segment number selects a map set, context switching is free (segment number is embedded in PC), and the kernel accesses user memory by constructing pointers with the target process's segment number.

See [doc/z8001_mmu_design_notes.md](doc/z8001_mmu_design_notes.md) for the full design.

### Syscall Convention

System calls use the Z8000 `sc` instruction. The syscall number is encoded in the tag word, arguments are passed in registers (R1-R3), and the return value comes back in R0. Dispatch uses a V7-style `sysent[]` function-pointer table.

### Compiler: PCC

The Portable C Compiler is the historical V7 Unix compiler and was designed to be self-hosting — making it the natural choice for a V7 port. The PCC-z8000 toolchain consists of cz8 (code generator), az8 (assembler), and ldz8 (linker), producing V7 a.out object files natively.

Steps 1-10 used ACK (Amsterdam Compiler Kit); the switch to PCC happened in Step 11, and ACK has since been removed from the tree. See [doc/PCC-Research.md](doc/PCC-Research.md) for the compiler research that motivated the switch.

The native two-pass compiler can rebuild itself under Unix, including its
optimizer; two successive native generations produce identical objects and
executables. The toolchain provides
`cc`, preprocessing, native `-O` assembly optimization, assembly and linking. Build its disk with
`python3 tools/native-cc/build.py` and run its guest tests with
`python3 tools/native-cc/test.py`. See [compiler self-hosting results and
reproduction](doc/implementation-steps.md#step-29-native-compiler-self-hosting).

The native development environment also rebuilds `make`, `ar`, `yacc`, the
supporting compiler tools and libc inside Unix. Native `ar`, `make` and `ldz8`
share the portable ASCII archive format. Archive framing is independent of
the Z8001 execution mode; current executable support is NONSEG combined or
split I/D. See [native development environment](doc/implementation-steps.md#step-30-native-development-environment).

### Kernel Configuration

The V7 layout keeps shared services in `usr/sys/sys`, device support in
`usr/sys/dev`, and headers in `usr/sys/h`. `usr/sys/machine` contains Z8000
CPU and MMU implementations; `usr/sys/conf` selects the machine, drivers,
device tables, boot devices and interrupt routing.

The default configuration is `emulated`. Select it explicitly with
`-DKERNEL_CONFIG=emulated`; use a separate build directory for each machine.
`-DKERNEL_HOST_TESTS=OFF` builds kernel artifacts without the emulator harness.
See [kernel configuration and adding a machine](v7z8000/usr/sys/conf/README.md).
Only the current emulated machine is implemented; the M20 is not yet a kernel
configuration.

### Emulator

The Z8000 software emulator is used as a library with a custom front end (`emu/test_driver.cpp`, kept outside the V7 tree since it is host code, not Unix source) that can simulate I/O and load code segments and data from files at arbitrary physical addresses without needing bootstrap code. This simplifies development considerably — the full kernel trap round-trip can be tested without a real boot ROM or hardware.

See [doc/z8000-emulator.md](doc/z8000-emulator.md) for details.

## Building and Testing

Prerequisites: z8k-coff binutils (for rom.s/trap.s), PCC-z8000 toolchain (cz8/az8/ldz8), C++17 compiler, Python 3.

```sh
git submodule update --init --recursive                   # PCC-z8000 + z8000_emu
make -C PCC-z8000/z8000/cz8                               # build compiler
make -C PCC-z8000/z8000/az8                               # build assembler
make -C PCC-z8000/z8000/test ../ldz8                       # build linker
make -C tools                                            # build user programs + filesystem images
cmake -S v7z8000/usr/sys -B v7z8000/usr/sys/build          # configure kernel build
cmake --build v7z8000/usr/sys/build                       # build kernel
cmake --build v7z8000/usr/sys/build --target test          # run kernel boot test
```

The kernel build pulls the emulator in via `add_subdirectory(z8000_emu)`, so the submodule must be initialised before configuring.

The test verifies the kernel boots, the Bourne shell prints a prompt, the pipeline `echo hello | cat` produces correct output, and no panics occurred.

Run these commands from the repository root. The basic boot disk contains a
small command set. The larger native development disk, including sources and
makefiles, is built separately using the [Step 29 and Step 30 instructions](doc/implementation-steps.md#step-29-native-compiler-self-hosting).

## Files

| File | Purpose |
|------|---------|
| `v7z8000/usr/sys/machine/emurom.s` | Reset vector + init code (segment 0) |
| `v7z8000/usr/sys/machine/trap.s` | PSA table + syscall entry/exit stubs |
| `v7z8000/usr/sys/machine/krt.s` | CPU runtime: entry table, BSS zeroing, I/O, user access, idle, SPL and save |
| `v7z8000/usr/sys/h/` | V7 kernel headers adapted for Z8000 |
| `v7z8000/usr/sys/sys/main.c` | Simplified V7 main: process 0, binit, iinit, open /dev/console |
| `v7z8000/usr/sys/sys/bio.c` | V7 buffer cache |
| `v7z8000/usr/sys/sys/alloc.c` | Block and inode allocation |
| `v7z8000/usr/sys/sys/iget.c` | Inode read/write (big-endian 3-byte address conversion) |
| `v7z8000/usr/sys/sys/nami.c` | Pathname resolution (namei) |
| `v7z8000/usr/sys/sys/rdwri.c` | Read/write I/O |
| `v7z8000/usr/sys/sys/subr.c` | bmap, bcopy, utilities |
| `v7z8000/usr/sys/sys/fio.c` | File descriptor operations |
| `v7z8000/usr/sys/sys/prf.c` | printf, panic |
| `v7z8000/usr/sys/machine/paged.c` | Paged-MMU allocation, mapping and process-memory copying |
| `v7z8000/usr/sys/sys/prim.c` | V7 clist character buffering (getc, putc, b_to_q, cinit) |
| `v7z8000/usr/sys/sys/slp.c` | Scheduler: sleep/wakeup, run queues, setpri, swtch, newproc |
| `v7z8000/usr/sys/sys/sys1.c` | Process syscalls: fork, exec, exit, wait, setregs |
| `v7z8000/usr/sys/sys/sys2.c` | File syscalls: read, write, open, creat, close, seek, link, mknod |
| `v7z8000/usr/sys/sys/sys3.c` | stat/fstat, dup, mount/umount |
| `v7z8000/usr/sys/sys/sys4.c` | Misc syscalls: time, uid/gid, unlink, chdir, chmod, kill, alarm, pause |
| `v7z8000/usr/sys/sys/sysent.c` | Syscall dispatch table (`sysent[]`) |
| `v7z8000/usr/sys/machine/trap.c` | C trap handlers: syscall dispatch, segmentation trap |
| `v7z8000/usr/sys/sys/sig.c` | Signals: psignal, signal, issig, psig |
| `v7z8000/usr/sys/sys/pipe.c` | Pipes: pipe syscall, readp/writep, plock/prele |
| `v7z8000/usr/sys/sys/clock.c` | Clock interrupt handler and `timeout()` callouts |
| `v7z8000/usr/sys/dev/md.c` | RAM disk driver (I/O port DMA) |
| `v7z8000/usr/sys/dev/hd.c` | IDE hard drive driver (ATA PIO, interrupt-driven) |
| `v7z8000/usr/sys/dev/cons.c` | Console driver with V7 TTY subsystem |
| `v7z8000/usr/sys/dev/tty.c` | V7 TTY line discipline (echo, erase, kill, canon) |
| `v7z8000/usr/sys/dev/partab.c` | Character type/parity table for TTY |
| `v7z8000/usr/sys/conf/emulated.c` | Device switch tables (bdevsw, cdevsw) |
| `v7z8000/usr/sys/CMakeLists.txt` | CMake build rules for all components |
| `emu/test_driver.cpp` | Emulated machine: MMU, IDE/ATA, console, RAM disk DMA, interrupt injection |
| `tools/v7mkfs.c` | V7 filesystem image builder |
| `tools/proto.small` | Filesystem prototype (/dev/console, /dev/tty, /etc/init, /bin/sh, /bin/echo, /bin/cat, /tmp) |
| `tools/libc/` | User-space C library: crt0, syscalls, setjmp, sbrk |
| `tools/bout2bin.py` | a.out → flat binary converter (for kernel handler.bin) |
| `v7z8000/` | V7 source tree adapted for Z8000 (kernel, libc, commands, man pages) |
| `v7unix/` | V7 Unix source tree (from TUHS, pristine reference) |
| `PCC-z8000/` | PCC compiler submodule with Z8000 backend (cz8/az8/ldz8) |
| `z8000_emu/` | Z8000 emulator submodule |

## Documentation

| Document | Contents |
|----------|----------|
| [doc/implementation-steps.md](doc/implementation-steps.md) | Step-by-step implementation journal |
| [doc/kernel-technical-reference.md](doc/kernel-technical-reference.md) | Kernel internals: PSA table, CPU modes, SYSCALL flow, stack layout |
| [doc/z8000-emulator.md](doc/z8000-emulator.md) | Z8000 software emulator and its use in the project |
| [doc/z8001_mmu_design_notes.md](doc/z8001_mmu_design_notes.md) | Custom paged MMU design using Z8001 segment numbers as map set selectors |
| [doc/PCC-Research.md](doc/PCC-Research.md) | Compiler research: PCC history, ACK assessment, Z8000 Unix history |
