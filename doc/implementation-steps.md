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

Implemented the SYSCALL trap round-trip — the foundation for all system calls.

**What was built:**

- ROM init code (`kernel/rom.s`, segment 0): sets up the system stack (RR14 = seg1:0xFFF0), configures PSAP to point to the PSA table (seg1:0x0000), sets the normal-mode stack pointer, and uses a fake IRET frame to jump from SEG+SYS mode to NONSEG+SYS mode in the kernel segment.
- PSA table (`kernel/trap.s`): 8 interrupt/trap vector entries, each pointing to handlers in the kernel segment.
- SYSCALL entry/exit stub: saves R0-R12, transitions SEG+SYS -> NONSEG+SYS, calls the C handler, writes the return value into the saved-R0 slot, transitions back to SEG+SYS, restores registers, and executes IRET.
- Test driver (`kernel/test_driver.cpp`): loads ROM + kernel into the emulator's 8MB Z8001 address space and verifies the result.

**Simplifications** for initial bring-up (see [kernel-technical-reference.md](kernel-technical-reference.md) for architecture details):

- No bootloader — the emulator front end loads ROM, kernel, and C handler binaries directly at their target physical addresses. This avoids building a bootloader or file system before the kernel itself works.
- Kernel hardcoded to segment 1, no MMU, identity-mapped.
- All code runs in the same segment — no separate user segments, no copyin/copyout.
- System stack at a fixed address (seg1:0xFFF0).
- Test code embedded in the kernel binary.

**Key challenges solved:**

- Z8001 PSA table entry format (8 bytes with segmented PC encoding).
- CHANGE_FCW R14/R15 swap semantics — R14 swaps with the saved stack segment register when F_SEG changes within system mode, R15 stays.
- PSAPSEG register encoding: `(seg << 8) | 0x8000`.
- The trap stub is assembled in z8001 mode (for `@RR14`) but must emit z8002 encodings for the NONSEG section. The z8k-coff-as `.unsegm`/`.segm` directives switch encoding mode within the file.

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

## Current State

The kernel bring-up has a working SYSCALL trap round-trip with:
- ROM boot -> kernel entry via IRET
- `sc` instruction -> PSA vector -> trap stub -> C handler -> IRET return
- V7-style `sysent[]` dispatch table
- `write()` (syscall #4) with console TTY output
- `exit()` (syscall #1)
- All C code compiled with ACK in Z8002 mode, no assembly workarounds

## Planned Steps

- **read() syscall**: read from console (requires emulator interrupt support)
- **Line discipline**: echo, erase, kill processing
- **clist buffering**: V7's character block allocator
- **File descriptor table / u-area**: proper fd validation
- **Separate user segments**: copyin/copyout for user memory access via segment numbers
- **Process management**: fork/exec using segment-based isolation
