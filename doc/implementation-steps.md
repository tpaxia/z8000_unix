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

## Step 10: Console Read with TTY Subsystem

Added the V7 TTY subsystem for console input — line discipline (echo, erase, kill), clist character buffering, and the `read()` syscall. Process 1 now reads from stdin and echoes back to stdout.

- Imported V7 TTY subsystem sources: `tty.c` (line discipline), `prim.c` (clist character buffering with `getc`/`putc`/`b_to_q`/`cinit`), `partab.c` (character type table), `tty.h` (tty structures and constants).
- Adapted `tty.c` for Z8000: removed PDP-11 multiplexer dependencies (`mx.h`, `reg.h`, `t_chan` references, `sdata()`/`scontrol()` calls), removed `ioctl()`/`stty()`/`gtty()`/`ttioccomm()` (syscalls not needed yet). Kept all core routines: `ttyopen`, `ttychars`, `ttyclose`, `ttread`, `ttwrite`, `canon`, `ttyinput`, `ttyoutput`, `ttstart`, `ttrstrt`, `wflushtty`, `flushtty`, `ttyblock`, `ttyrend`.
- Fixed V7 `tty.h` anonymous struct member (`struct tc;` inside union) — ACK doesn't support this PDP-11 C extension. Changed to named member `struct tc t_tc;` with updated `tun` macro (`tp->t_un.t_tc` instead of `tp->t_un`).
- Replaced `cinit()` in `machdep.c` with V7's `prim.c:cinit()` which both initializes the clist freelist AND counts character devices.
- Rewrote `cons.c` following V7 `kl.c` pattern: `cons_tty[]` struct, `consopen` sets `t_oproc`/`t_state`/`t_flags`/`ttychars`, `consread`/`conswrite` call `ttread`/`ttwrite`, `consrint()` interrupt handler reads from status/data ports and calls `ttyinput()`, `consstart()` `t_oproc` callback drains `t_outq` via `putchar()` with delay character handling via `timeout(ttrstrt)`.
- Updated `conf.c`: `cdevsw` entries now have `d_ttys = &cons_tty[0]`.
- Added `read()` syscall in `sys1.c` (mirror of `write()`), wired as syscall #3 in `sysent.c`.
- Added `signal(pgrp, sig)` in `sig.c` — sends signal to process group, called by `ttyinput()` for SIGINT/SIGQUIT. Routes through `psignal()` (still a no-op stub).
- Removed `_putc` from `krt.s` — name collision with V7's clist `putc()` in `prim.c`. All console output uses `_putchar` (called by `prf.c:printf` and `cons.c:consstart`).
- Updated `vi_dispatch` in `krt.s` to call both `_hdintr` and `_consrint` — all devices share VI vector 0, each handler guards itself (hdintr checks `hd_bp==0`, consrint checks RX-ready status register).
- Emulator (`test_driver.cpp`): added console input FIFO (`std::queue<uint8_t>`), `queue_console_char()` method that pushes a character and asserts VI(0). Port 0xF0 read dequeues from FIFO, port 0xF2 status bit 1 reflects FIFO non-empty. Main loop delivers "hi\n" after 100 clock ticks.
- Test program (`tools/init.s`): replaced write-only test with `read(0, buf, 80)` then `write(1, buf, n)` echo loop.

**Test:** CPU halted, console output = "boot\nZ8000 Unix\nhi\nhi\n", no panics. PASS. The first "hi\n" is echo from `ttyinput()` (ECHO+CRMOD flags), the second is the write-back from init's read+write.

## Step 11: Switch from ACK to PCC, Bourne Shell Running

Replaced the ACK (Amsterdam Compiler Kit) toolchain with a PCC (Portable C Compiler) port for Z8000, and brought up the Bourne shell (`/bin/sh`) running `echo hello` end-to-end.

### PCC Toolchain Switch

ACK was used for Steps 1-10 but has several limitations: it is not self-hosting on the target, its code generator is table-driven with a custom DSL that is difficult to debug, and it has no path to running on Z8000 Unix itself. PCC is the historical V7 Unix compiler and was designed to be self-hosting, making it the natural choice for a V7 port.

The PCC-z8000 toolchain consists of:
- **cz8** — PCC code generator backend for Z8000 (non-segmented mode)
- **az8** — Z8000 assembler (b.out object format)
- **ldz8** — Linker for b.out objects

The toolchain produces b.out format objects which are converted to:
- V7 a.out (0407 magic) for user programs via `bout2v7.py`
- Flat binary for kernel `handler.bin` via `bout2bin.py`

ROM (`rom.s`) and trap table (`trap.s`) remain assembled with `z8k-coff-as` in Z8001 segmented mode, as they contain segmented-mode instructions that az8 does not handle.

Key differences from ACK:
- Frame pointer R13 (same as ACK convention, chosen for Z8001 RR14 compatibility)
- `krt.s` rewritten for az8 syntax (`.globl` instead of `.define`, `#` instead of `$` for immediates, `@sp` instead of `*SP`, named labels instead of numeric)
- User-space assembly (crt0, syscalls, setjmp) rewritten for PCC symbol naming and calling convention
- 32-bit arithmetic library (`arith.az8`) provides `lmul`/`ldiv`/`lrem`/`ulmul`/`uldiv`/`ulrem` using Z8000 hardware `mult`/`div`

### PCC Compiler Bugs Fixed

Three cz8 code generation bugs were found and fixed during kernel bring-up:

1. **MUL/DIV/MOD writeback** — The `mult`/`div` instruction templates did not write the result back to the destination register when it wasn't already R1. Fixed by adding `ld AL,r1` after each `mult`/`div` sequence. This caused kernel buffer cache corruption (incorrect block numbers from `bmap()`).

2. **Big-endian byte access (INT→CHAR conversion)** — The SCONV template for truncating a word to a byte from memory operands (SNAME/SOREG) used the word offset directly. On big-endian Z8000, the low byte of a word at address N is at N+1, not N. Added a new template with `ZT` escape that invokes `local2.c`'s type-adjustment code to add +1 for CHAR. This caused `putc()` in the clist code to store NUL bytes instead of actual characters, breaking all tty output.

3. **incode() shift for data initialization** — The `incode()` function used `SZINT` (16) as the shift base, but the emission code extracted bits 16-31 of a 32-bit `long`. Changed to shift by `(32 - sz - inwd)` so initialized data lands in the correct bits. This caused corrupted static data (e.g., `sysent[]` function pointers, device switch tables).

Additionally, the `cbranch()` zero-elision optimization was excluding signed comparisons (GT/GE/LT/LE) which only works for unsigned ops, and `rl` was corrected to `rlc` in the unsigned long division routine.

### Bourne Shell

With the compiler fixes, the V7 Bourne shell (`/bin/sh`) runs:
- init exec's `/bin/sh`
- Shell prints `# ` prompt
- `echo hello` produces `hello` output
- `exit` terminates cleanly

The shell sources (`v7z8000/usr/src/cmd/sh/`) are compiled with cz8 and linked with the user-space libc. The shell binary is installed into the root filesystem image via `tools/proto.small`.

**Test:** Console output = "boot\nZ8000 Unix\n...\n# echo hello\nhello\n# exit\n", no panics. PASS.

## Step 12: Change PCC Frame Pointer from R14 to R13

Changed the PCC calling convention to use R13 as frame pointer instead of R14, for Z8001 segmented mode compatibility. In Z8001 SEG mode, RR14 (R14:R15) is the system stack pointer — using R14 as frame pointer would conflict if PCC is later extended to generate segmented code. R13 matches the ACK convention used in Steps 1-10.

### Changes

- Callee-saved register set changed from `{R4-R7, R10-R13}` (FP=R14) to `{R4-R7, R10-R12, R14}` (FP=R13). Same count (8 registers).
- PCC backend (`cz8`): `STKREG`/`ARGREG` changed to 13, prologue/epilogue generation updated, `savemask` updated from `0x3CF0` to `0x5CF0`, `rstatus[]` marks R13 as non-allocatable.
- Kernel assembly (`krt.s`): all function prologues/epilogues updated (17 functions). `save()`/`resume()` label_t layout: `[0-3]` R4-R7, `[4-6]` R10-R12, `[7]` R14, `[8]` caller's R13 (FP), `[9]` retaddr, `[10]` SP, `[11]` unused. `retu()` unchanged — it uses R14 for the architectural SEG mode stack pointer, not the calling convention.
- User-space `setjmp.az8`: jmp_buf layout updated to match label_t.
- `label_t` and `jmp_buf` comments updated in `param.h`.

### PCC Register Allocation Bug Fixed

The initial change caused a kernel boot failure: PCC's pointer register variable allocator in `pftn.c` assigned R13 for the first pointer register variable. The initialization `regvar = MAXRVAR | ((MAXRVAR-2)<<8)` set the address register counter to 5, mapping to R13 (5+8=13) — clobbering the frame pointer.

Fixed by changing `(MAXRVAR-2)` to `(MAXRVAR-3)` so pointer register variables start at R12 (4+8=12). Added safety guards in `setregs()` to force `rstatus[13]=SBREG` and `rstatus[15]=SBREG`. This reduces available pointer register variables from 2 to 1, which is acceptable.

**Test:** Kernel boots, shell runs, `echo hello` succeeds. PASS.

## Step 13: Pipes, cat, and PCC Indirect Call Fix

Added pipes and the `cat` command, enabling `echo hello | cat` — the first shell pipeline. This required fixing a critical PCC codegen bug for indirect function calls through global/static variables.

### PCC Indirect Function Call Bug

The shell crashed with a privilege violation when running `echo hello | cat`. The crash occurred at address 0x6F2E in the shell's DATA section — the CPU was executing from a data address instead of code.

Investigation traced the bug through several layers:

1. **Watchpoint blind spot**: The emulator's memory watchpoints did not trigger on the write that corrupted 0x6F2E. The Z8000 CPU's `WRMEM_B` (byte write) converts byte writes to masked word writes via `write_word(addr, val, mask)`, and only the non-masked `write_word` had watchpoint checks. Since `bcopy()` copies byte-by-byte, all `copyseg()` writes (and any other byte writes) bypassed the watchpoint. Fixed by adding the watchpoint check to the masked `write_word` override.

2. **The corrupting write**: With the fixed watchpoint, the write was found: `namscan(exname)` in the shell's `service.c` stores `exname`'s address (0x3A58) to the static function pointer `namfn` at 0x6F2E. This is correct — the bug is in what happens next.

3. **Root cause in PCC**: `namwalk()` calls `(*namfn)(np)` — an indirect call through a global function pointer variable. PCC represents direct function calls as ICON nodes and indirect calls through variables as NAME nodes. The `zzzcode()` 'C' case in `local2.c` treated both identically, generating `call fnptr` (direct call to the variable's address) instead of `ld r8, fnptr; call @r8` (load the pointer value, then indirect call). The CPU jumped to address 0x6F2E (the location of `namfn`) instead of 0x3A58 (the value stored in `namfn`), executing data as code.

4. **Fix**: Separated NAME from ICON in `zzzcode()` case 'C'. NAME now generates `ld r8, <name>; call @r8` (indirect), while ICON still generates `call <name>` (direct).

### PCC INCR/DECR Byte-Width Bug

The INCR/DECR templates in `table.c` used `inc`/`dec` (always word-width) instead of `incZB`/`decZB` (width-aware via the ZB escape). This caused byte post-increment expressions like `*p++` to generate word-width `inc` instructions, incrementing the pointer by the wrong amount.

### Kernel Changes

- **NPROC increased from 4 to 8**: Pipes require additional process slots (parent shell + two children for `cmd1 | cmd2`).
- **`copyseg()` rewritten**: Replaced the 4KB static `copybuf[]` with a 256-byte stack buffer, copying in 256-byte chunks. The stack buffer is safe during copy window remaps because it lives below 0xE000.
- **`frame_used[4096]` → `frame_bmap[512]`**: Physical frame allocator changed from a byte-per-frame array to a bitmap, saving 3.5KB of BSS.
- **Trap handlers improved**: Added a privilege violation handler in `trap.s` that prints the faulting PC, opcode, and FCW before halting (instead of silent halt). Added a minimal SEGTRAP handler. Added `segtrap_handler()` in `trap.c` for kernel-mode diagnostic output and user-mode SIGSEG delivery.
- **Boot entry relocated**: Moved from 0x0180 to 0x01F0 to accommodate the larger trap handler stubs (privilege violation hex printing).
- **Removed debug instrumentation**: Cleaned up all debug putchar markers from `krt.s` (retu, main-return), `slp.c` (context switch, fork), `sys1.c` (exec), and `trap.c` (syscall tracing).

### cat Command and Test Driver

- Added `/bin/cat` to the filesystem image (`tools/cat.c`, `tools/Makefile`, `tools/proto.small`).
- Test driver (`test_driver.cpp`): input is now `echo hello | cat\nexit\n`. Input delivery waits for the shell prompt (`# `) instead of a fixed tick count. Better termination logic with idle-after-input counter. Console output escapes `\r` and control characters.

**Test:** Console output = "boot\nZ8000 Unix\n# echo hello | cat\nhello\n# exit\n". PASS.

## Step 14: Switch PCC Toolchain from b.out to a.out Object Format

Switched the PCC cross-toolchain (az8 assembler, ldz8 linker) from the custom b.out object format to standard V7 a.out format. This eliminates the host-side `bout2v7.py` conversion script and is a prerequisite for self-hosting PCC on Z8000 Unix.

### a.out Format (Z8000)

16-byte header with 8 x 16-bit big-endian words, matching Z8000's native `int` size:

| Offset | Field | Description |
|--------|-------|-------------|
| 0 | a_magic | 0407 (OMAGIC) or 0410 (NMAGIC) |
| 2 | a_text | text segment size |
| 4 | a_data | data segment size |
| 6 | a_bss | BSS size |
| 8 | a_syms | symbol table size |
| 10 | a_entry | entry point |
| 12 | a_trsize | text relocation size |
| 14 | a_drsize | data relocation size |

Section order: header → text → data → text relocation → data relocation → symbols. This differs from b.out which had symbols before relocation.

Symbol entries are 12-byte `struct nlist` (8-byte name + 2-byte n_type + 2-byte n_value), replacing b.out's variable-length format (2-byte type + 4-byte value + NUL-terminated name).

### Changes

- **az8 assembler** (`az8/rel.c`, `az8/sym.c`): Writes 16-byte header with 2-byte fields. `Sym_Write()` produces 12-byte nlist entries with type conversion `stype >> 8`. Symbols written after relocation (deferred from `Rel_Header()` to `Fix_Rel()`).
- **ldz8 linker** (`ldz8.c`): Reads 16-byte headers via `short tmp` + 2-byte `get68()`. `getsym()` reads 12-byte nlist with `n_type << 8` conversion. `finishout()` writes 12-byte nlist at correct SYMPOS. Default output changed from `b.out` to `a.out`.
- **b.out.h**: Renamed to reflect a.out layout. `HDRSIZE=16` (was 32), `NLIST_DISKSIZE=12` (was variable), position macros reordered for a.out section order.
- **a.out.h**: `a_unused` → `a_trsize`, `a_flag` → `a_drsize`.
- **user.h**: `ux_unused` → `ux_trsize`, `ux_relflg` → `ux_drsize` (cosmetic, exec() doesn't use these fields).
- **tools/Makefile**: Removed `bout2v7.py` conversion steps — ldz8 now produces a.out directly.
- **bout2bin.py**: Updated for 16-byte a.out header (kernel handler.bin extraction).
- **bout2v7.py**: Deleted (no longer needed).

### Bug Fixed

The initial implementation had a linker bug: `finishout()` used the `SYMPOS` macro (which references `filhdr` struct fields) to seek to the symbol table position. But by the time `finishout()` runs, `filhdr` has been overwritten with the last input file's header during pass 2, causing symbols to be written within the text segment. Fixed by using global accumulated size variables (`tsize`, `dsize`, `rtsize`, `rdsize`) instead of the macro.

**Test:** handler.bin matches reference binary byte-for-byte. Console output = "boot\nZ8000 Unix\n# echo hello | cat\nhello\n# exit\n". PASS.

## Step 15: Convert Kernel Build System from Makefile to CMake

Converted the kernel build system (`v7z8000/usr/sys/Makefile`) to CMake (`CMakeLists.txt`) for consistency with the z8000_emu submodule which already uses CMake.

### Challenge

The kernel uses two non-standard cross-toolchains — neither is a CMake-native compiler:
1. **Z8K GNU toolchain** (`z8k-coff-as/ld/objcopy`) for `rom.s` and `trap.s` (segmented mode)
2. **PCC toolchain** (`cpp → cz8 → az8 → ldz8`) for all kernel C code and `krt.s`

The entire build uses `add_custom_command` + `add_custom_target`, except for the test driver which uses native `add_executable` + `target_link_libraries`. The z8000_emu library is pulled in via `add_subdirectory`.

### Issues Solved

1. **Shell quoting**: The PCC pipeline includes `grep -v '^"'` to filter cz8 warnings. The double-quote character cannot be properly nested in CMake's `sh -c "..."` strings due to layered escaping (CMake → Makefile → shell). Fixed by generating a `pcc_compile.sh` helper script at configure time using CMake's `[=[...]=]` bracket syntax, which avoids all escape processing.

2. **az8 buffer overflow**: The PCC assembler (`az8`) has a 32-byte filename buffer (`STR_MAX` in `mical.h`). CMake's absolute paths (80+ characters for this project) cause a silent buffer overflow, corrupting the `Source_name` pointer and breaking the fallback path that handles `.az8` extensions. Fixed by using relative paths in all COMMAND arguments to PCC tools (e.g., `sys/main.az8` instead of `/Users/.../build/sys/main.az8`).

3. **CMake DEPENDS resolution**: CMake resolves relative paths in `DEPENDS` relative to the *source* directory, but `OUTPUT` relative to the *binary* directory. With stale build artifacts in the source tree (from the old Makefile era), the dependency chain was silently satisfied by wrong files. Fixed by using `${CMAKE_CURRENT_BINARY_DIR}/` prefix on all `OUTPUT` and `DEPENDS` paths.

### Build Commands

```sh
cd v7z8000/usr/sys
cmake -S . -B build          # configure
cmake --build build           # build all targets
cmake --build build --target test   # run boot test
```

**Test:** PASS — identical output to the Makefile build.

## Current State

The kernel boots, mounts a root filesystem, runs the Bourne shell, and executes commands including pipelines:
- All V7 C source compiled with PCC (cz8) in Z8002 mode, K&R style unchanged
- PCC toolchain produces native a.out binaries directly (no conversion scripts)
- Bourne shell running with fork/exec/wait/pipe
- Shell pipelines work (`echo hello | cat`)
- Paged MMU with KDSA6-equivalent for per-process u-area remapping
- V7-style context switching (save/resume/swtch) — no bcopy of u-areas, no per-process kernel stacks
- Process creation via fork (newproc) with u-area copy through MMU window
- sleep/wakeup, run queue management, priority scheduling
- Buffer cache (bio.c) with 8 buffers
- Interrupt-driven IDE hard drive with VI
- Root filesystem mounted from HD image via ATA PIO
- exec() syscall loading binaries from filesystem
- Directory traversal (namei) and inode management (iget/iput)
- File descriptor table (falloc) and device open (openi → cdevsw)
- V7 TTY subsystem: line discipline (echo, erase, kill), clist buffering, canon
- Console input via VI interrupt (consrint → ttyinput → sleep/wakeup)
- Console output through ttwrite → ttyoutput → consstart → putchar → outb
- read() and write() syscalls for character and block devices
- Pipes (pipe.c) for inter-process communication
- Clock interrupts via NVI with timeout() callouts
- Cross-segment user memory access (copyin/copyout) via SEG mode toggle
- SPL functions controlling VIE/NVIE enable/disable
- Trap diagnostic handlers (privilege violation, segmentation trap)

## Planned Steps

- **stty/ioctl**: terminal parameter control
- **More commands**: ls, cp, wc, etc.
- **Multi-stage pipelines**: `ls | grep foo | wc`
- **Self-hosting**: PCC compiling itself on Z8000 Unix
