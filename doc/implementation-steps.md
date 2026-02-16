# Implementation Steps

Step-by-step journal of the Z8000 Unix kernel bring-up. Each step builds on the previous one and is verified by an automated test before moving on.

## Step 1: Project Setup and Toolchain

Set up the project structure, imported dependencies, and verified the cross-compilation toolchain works end-to-end.

- Added ACK (Amsterdam Compiler Kit) as a submodule from a fork with Z8000 cross-compilation support. This required wiring the existing Z8000 machine support (assembler, code generator, runtime library) into ACK's modern Python-based build system, and fixing the assembler's separate `as`/`led` mode (it had only worked in combined `asld` mode).
- Added the Z8000 software emulator as a submodule.
- Imported the V7 Unix source tree from the TUHS archive as a baseline for the port.
- Created a test infrastructure (`tests/run_test.sh`) that compiles C with ACK, prepends a Z8001 reset vector, and runs the binary on the emulator.
- Verified with a simple C test program (`test_add.c`: 3+4=7, result in R0).

## Step 2: Z8002 Non-Segmented Mode

Extended ACK to support Z8002 (non-segmented) mode, since the kernel runs in NONSEG mode with 16-bit pointers.

- Added the `*SP` assembler mnemonic that resolves to `*RR14` (segmented) or `@R15` (non-segmented) based on a `-n` flag, allowing all runtime libraries to share the same source.
- Built a separate `cg_z8002` code generator with `EM_BSIZE=4` (2-byte return addresses instead of 4-byte segmented).
- Replaced `*RR14` with `*SP` across libem (30 files), libmon, and boot.s.
- Updated the test script to accept `-p z8001|z8002` flags.
- Created an assembler mode test to verify correct instruction encoding differences between segmented and non-segmented mode.

## Step 3: Trap Infrastructure

Implemented the SYSCALL trap round-trip — the foundation for all system calls. See [step3-trap-infrastructure.md](step3-trap-infrastructure.md) for details.

- ROM init code (`kernel/rom.s`): sets up system stack, PSAP, and uses IRET to enter NONSEG+SYS mode.
- PSA table (`kernel/trap.s`): 8 trap vector entries pointing to handlers in the kernel segment.
- SYSCALL entry/exit stub: register save/restore, SEG+SYS <-> NONSEG+SYS transitions, C handler call, IRET return.
- Test driver (`kernel/test_driver.cpp`): loads binaries into the emulator and verifies results.
- Key challenges: PSA entry format, CHANGE_FCW R14/R15 swap semantics, mixed-mode assembly (`.unsegm`/`.segm`).

**Test:** `sc #0` -> trap handler sets R0=7 -> IRET -> halt with R0=7. PASS.

## Step 4: Console Device and C Handler

Connected ACK-compiled C code to the trap stub and added console output.

- Added a console I/O port (0x00F0) to the test front end that outputs characters to the host's stdout.
- Created the kernel runtime stub (`kernel/krt.s`): an ACK-compiled entry trampoline at offset 0x0200 that the trap stub calls, plus a `putc()` function using the `outb` instruction.
- Created `kernel/syscall.c` with a C handler called from the trampoline.
- The trap stub calls the C handler at a fixed address (0x0200) after switching to NONSEG+SYS mode.

**Test:** trap handler calls C code, C code outputs "Hi\n" via `outb` and returns 7, R0=7 after IRET. PASS.

## Step 5: Syscall Dispatch and write()

Added proper syscall dispatch with argument passing and the `write()` system call.

- Trap stub now extracts the syscall number from the SC instruction's tag word (the CPU pushes the tag word onto the stack on trap entry), passes it and a pointer to the saved registers to the C handler.
- Implemented syscall dispatch by number in C.
- Implemented `write(fd, buf, count)` as syscall #4: R1=fd, R2=buf pointer, R3=count. For fd 1 (stdout) or 2 (stderr), calls `cons_write()` which loops over the buffer outputting each character via `putc()`.
- The C handler's return value is written into the saved-R0 slot on the stack, so it appears in R0 after IRET.

**Test:** `sc #4` with write(1, "Hello from Z8000 Unix!\n", 23) -> R0=23, correct console output. PASS.

## Step 6: sysent[] Dispatch Table and exit()

Replaced the switch-based syscall dispatch with a V7-style `sysent[]` function-pointer table and added more syscalls.

- Created `sysent[64]` array of `{ sy_call, sy_narg }` structs, indexed by syscall number. `syscall_handler()` bounds-checks the number, NULL-checks the handler, and calls through the function pointer.
- Added `exit()` as syscall #1 (returns its argument as the process exit status in R0) and `sys_nosys()` as the default handler (returns -1).
- This validated indirect function calls through a struct array, which exercises ACK's function pointer relocations in initialized data.

**Test:** `write(1, msg, 23)` then `exit(42)` -> console output correct, R0=42. PASS.

## Step 7: V7 Kernel — Process 0, Filesystem, /dev/console

Replaced the test syscall handler with a real V7 kernel that boots to process 0, mounts a root filesystem from a RAM disk, opens `/dev/console`, and prints a message. This proves the entire V7 filesystem + buffer cache + device driver stack works end-to-end. See [step7-v7-kernel.md](step7-v7-kernel.md) for details.

- Imported V7 kernel headers (`kernel/h/`) and source (`kernel/sys/`): buffer cache, inode/block allocation, pathname resolution, read/write I/O, file descriptor operations, printf. Adapted for Z8000: big-endian 3-byte inode addresses, PDP-11 fields removed, reduced table sizes.
- Created a simplified `main()` — no fork/exec/sched, just process 0: `binit()`→`iinit()`→`namei("/dev/console")`→`open1()`→`printf("Z8000 Unix\n")`→`idle()`.
- Created device drivers: RAM disk (`md.c`, I/O port DMA), console (`cons.c`), device switch tables (`conf.c`).
- Extended `krt.s` with BSS zeroing, `inb()`/`outb()`/`putchar()`/`idle()`.
- Extended `test_driver.cpp` with a DMA controller and disk image loading.
- Fixed 8 ACK compiler/assembler/runtime bugs uncovered by compiling real V7 code (assembler relocations, `ldb` encoding, libem return addresses and `*SP` register encoding, `inb()` return register, BSS zeroing). Details in [ack-compiler.md](ack-compiler.md).

**Test:** CPU halted, console output = "boot\nZ8000 Unix\n", no panics. PASS.

## Step 8: Fork, Paged MMU, and V7-Style Context Switching

Added process management (fork/exit/wait), a paged MMU emulation, and V7-style context switching. Process 0 forks process 1, which writes a message via syscall and exits. See [step8-fork-mmu.md](step8-fork-mmu.md) for details.

- Emulated a paged MMU in the test driver (128 segments x 32 pages x 2KB pages, identity-mapped). Two I/O ports remap pages: UPAGE (0x00B0) acts as a KDSA6 equivalent remapping the u-area (seg1 pages 30-31), WPAGE (0x00B4) provides a copy window for `newproc()`.
- u-area at fixed virtual address (`#define u (*(struct user *)0xF000)`), remapped per-process by the MMU — exactly like the PDP-11.
- V7-style `save()`/`resume()` in assembly: `resume()` writes KDSA6 to remap the u-area before restoring registers. `label_t[12]` stores all state including return address and SP (the PDP-11 uses `label_t[6]`) because `bcopy()` in `newproc()` clobbers save's deallocated stack frame.
- V7-style `swtch()` — the save/resume dance with `u_rsav`/`u_qsav`/`u_ssav`, proc[0] as idle process, run queue search.
- V7-style `newproc()` — allocates u-area frames, copies parent u-area to child via MMU copy window, no per-process kernel stacks or trampolines.
- `sleep()`/`wakeup()` with hash-table sleep queues, `setrq()`/`setrun()`/`setpri()`.
- `fork()`/`exit()`/`wait()` syscalls in `sys1.c`.
- `retu()` for user-mode entry via IRET frame (NONSEG+NORM).
- Cross-segment memory access: `fubyte`/`subyte`/`fuword`/`suword`/`copyin`/`copyout` using SEG+SYS mode toggle.
- Physical frame allocator (`frame_alloc`/`frame_free`) and segment allocator for user processes.
- Fixed ACK code generator byte zero-extension bug (`mach/z8000/cg/table` MOVES rule): the old pattern cleared the destination register before loading the byte, clobbering the index register when source addressing used the same register. Fixed by reversing the order (load byte first, then `clrb` high byte) and adding `HR0`-`HR7` assembler aliases. The `nami.c` comparison loop now uses original V7 code with no workaround.

**Test:** CPU halted, console output = "boot\nZ8000 Unix\nhello from process 1\n", no panics. PASS.

## Step 9: exec() Syscall, IDE Hard Drive, and Interrupt-Driven I/O

Added exec() syscall, an IDE hard drive driver, and converted the HD driver to interrupt-driven I/O using the Z8000 NVI (Non-Vectored Interrupt).

- exec() syscall (#11): loads a binary from the filesystem into the user segment, sets up user stack, and enters user mode. Process 1's icode now calls exec("/etc/init") instead of hardcoded write()+exit().
- IDE hard drive driver (hd.c): ATA PIO driver at standard x86 register addresses (0x1F0-0x1F7). The root filesystem is now loaded from an HD image (hd.img) instead of the RAM disk.
- Interrupt-driven I/O: hdstrategy() issues the ATA command and returns. The emulator asserts NVI on command completion. The NVI handler (trap.s nvi_entry) saves registers, switches to NONSEG+SYS, calls hdintr() which performs data transfer and calls iodone().
- NVI trap infrastructure: PSA NVI vector points to nvi_entry handler in trap.s, which follows the same SEG/NONSEG mode switching pattern as syscall_entry. The nvi_dispatch entry at 0x0204 in krt.s calls the C interrupt handler.
- SPL functions: spl0/spl1/spl4/spl5/spl6/spl7/splx implemented in assembly (krt.s), controlling the NVIE bit (0x0800) in the FCW. Replaced the C no-op stubs in machdep.c.
- NVIE enabled at boot: boot_entry sets FCW to 0x4800 (NONSEG+SYS+NVIE) before calling main().
- idle() fixed: added ret after halt so NVI can wake the CPU from HALT and return to swtch().
- Emulator: added assert_nvi() method to z8002_device. KernelIOPorts takes a CPU pointer and asserts NVI on ATA read completion and write flush.
- Build system: all build artifacts now go into build/ subdirectory.
- Directory restructure: moved kernel/ to v7z8000/usr/sys/ to mirror the V7 directory layout. The V7 import commit now seeds the full V7 user-space tree (libc, commands, man pages, include headers, etc.) under v7z8000/, and the kernel dev/ files (conf.c, cons.c) with their V7 originals. This ensures all future modifications show as diffs from the V7 baseline.

**Test:** CPU halted, console output = "boot\nZ8000 Unix\nhello from exec\n", no panics. PASS.

## Current State

The kernel boots to process 0 with a working V7 filesystem stack, forks process 1, exec's /etc/init from the HD, and runs user-mode code:
- Paged MMU with KDSA6-equivalent for per-process u-area remapping
- V7-style context switching (save/resume/swtch) -- no bcopy of u-areas, no per-process kernel stacks
- Process creation via fork (newproc) with u-area copy through MMU window
- sleep/wakeup, run queue management, priority scheduling
- Buffer cache (bio.c) with 8 buffers
- Interrupt-driven IDE hard drive with NVI
- Root filesystem mounted from HD image via ATA PIO
- exec() syscall loading binaries from filesystem
- Directory traversal (namei) and inode management (iget/iput)
- File descriptor table (falloc) and device open (openi -> cdevsw)
- Console output through the V7 printf -> putchar -> outb path
- Cross-segment user memory access (copyin/copyout) via SEG mode toggle
- SPL functions controlling NVI enable/disable
- All V7 C source compiled with ACK in Z8002 mode, K&R style unchanged

## Planned Steps

- **Clock interrupts**: timer-driven scheduling and preemption
- **read() syscall**: read from console (requires emulator interrupt support)
- **Line discipline**: echo, erase, kill processing
- **clist buffering**: V7's character block allocator
- **Pipes**: inter-process communication
