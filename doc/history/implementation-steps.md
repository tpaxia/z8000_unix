# Implementation Steps

Step-by-step journal of the Z8000 Unix kernel bring-up. Each step builds on the previous one and is verified by an automated test before moving on.

Entries preserve results and sizes from their implementation stage. For the
present status, see [Current State](#current-state), [compiler self-hosting](#step-29-native-compiler-self-hosting)
and the [native development environment](#step-30-native-development-environment).

## Step 1: Project Setup and Toolchain

Set up the project structure, imported dependencies, and verified the cross-compilation toolchain works end-to-end.

- Added ACK (Amsterdam Compiler Kit) as a submodule from a fork with Z8000 cross-compilation support. This required wiring the existing Z8000 machine support (assembler, code generator, runtime library) into ACK's modern Python-based build system, and fixing the assembler's separate `as`/`led` mode (it had only worked in combined `asld` mode).
- Added the Z8000 software emulator as a submodule.
- Imported the V7 Unix source tree from the TUHS archive as a baseline for the port.
- Created a test infrastructure (`tests/run_test.sh`) that compiles C with ACK, prepends a Z8001 reset vector, and runs the binary on the emulator. (Removed with ACK after Step 11; the kernel boot test under `v7z8000/usr/sys` superseded it.)
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
- Fixed 8 ACK compiler/assembler/runtime bugs uncovered by compiling real V7 code (assembler relocations, `ldb` encoding, libem return addresses and `*SP` register encoding, `inb()` return register, BSS zeroing).

**Test:** CPU halted, console output = "boot\nZ8000 Unix\n", no panics. PASS.

## Step 8: Fork, Paged MMU, and V7-Style Context Switching

Added process management (fork/exit/wait), a paged MMU emulation, and V7-style context switching. Process 0 forks process 1, which writes a message via syscall and exits. See [step8-fork-mmu.md](step8-fork-mmu.md) for details.

- Emulated a paged MMU in the test driver (128 segments x 32 pages x 2KB pages, identity-mapped). Two I/O ports remap pages: UPAGE (0x00B0) acts as a KDSA6 equivalent remapping the u-area (seg1 pages 30-31), WPAGE (0x00B4) provides a copy window for `newproc()`.
- u-area at fixed virtual address (`#define u (*(struct user *)0xF000)`), remapped per-process by the MMU — exactly like the PDP-11.
- V7-style `save()`/`resume()` in assembly: `resume()` writes KDSA6 to remap the u-area before restoring registers. `label_t[12]` stores all state including return address and SP (the PDP-11 uses `label_t[6]`) because `bcopy()` in `newproc()` clobbers save's deallocated stack frame.
- V7-style `swtch()` — the save/resume dance with `u_rsav`/`u_qsav`/`u_ssav`, proc[0] as idle process, run queue search.
- V7-style `newproc()` — allocates u-area frames, copies parent u-area and its kernel stack to the child via the MMU copy window; no separate stack allocation or trampolines.
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
- Emulator: added an `assert_nvi()` method to z8002_device. KernelIOPorts takes a CPU pointer and asserts NVI on ATA read completion and write flush. (Superseded in Step 16 by the emulator's own `pulse_input_line()`.)
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

## Step 16: Remove ACK, Update the Emulator, Separate the Machine Model

Housekeeping step: retired the dead ACK toolchain, moved to the current emulator, and moved the host-side machine model out of the V7 source tree.

### ACK removed

ACK was replaced by PCC in Step 11 but was still carried as a 68MB submodule and still described in the present tense, so a cold read gave the wrong compiler. Removed the submodule, `doc/ack-compiler.md`, `tools/ack2v7.py`, and `tests/` — the latter was entirely ACK bound (`run_test.sh` hard requires `$ACK/.obj/staging/bin/ack`; the rest are its inputs) and had been superseded by the kernel boot test. The ACK history in the Steps 1–11 journals is left intact; only the dangling links and present-tense claims were repaired.

### Emulator updated

The pinned emulator was 12 commits behind, and the `assert_nvi()`/`assert_vi()` methods the front end depended on existed only as an uncommitted local edit — a fresh clone could not build. Moved to upstream `f376c54`.

Upstream restructured into a library layout, so `z8000.h` is now `<z8000/z8000.h>` and `memory.h` moved from the public `include/` to the driver's `src/`, reached via an explicit include directory. The `z8000` target name is unchanged, so the link line is untouched.

The emulator gained a committed interrupt line API (`set_input_line`, `set_input_line_and_vector`, `pulse_input_line`), filling in the `NVI_LINE`/`VI_LINE`/`NMI_LINE` enum it had declared but never wired up.

### Interrupt delivery: pulse, not level

Interrupt injection now uses `pulse_input_line()`. The clock tick, ATA completion and console receive are momentary events, not held lines. Driving them with the level-sensitive `set_input_line()` re-latches the request in `CHANGE_FCW` every time the handler's `IRET` re-enables NVIE:

```c
if (!(m_fcw & F_NVIE) && (fcw & F_NVIE) && (m_irq_state[0] != CLEAR_LINE))
    m_irq_req |= Z8000_NVI;
```

which re-enters the handler forever. The symptom was a kernel that printed `boot`, then ran exactly 38 instructions of `main()` — as far as `clkstart()` → `spl0()`, where interrupts are first enabled — and never resumed. `cinit()`, `binit()` and `iinit()` never ran. Diagnosed by tracing the same kernel binary on both emulators and diffing the instruction streams: identical for 21,457 instructions, then the working one returns from `iret` to the interrupted code while the broken one re-enters `nvi_entry`.

### Machine model moved out of the V7 tree

`test_driver.cpp` is host C++ modelling the machine — MMU, IDE/ATA controller, console, RAM disk DMA — not Unix source, so keeping it in `v7z8000/usr/sys/` diluted the property that everything under `v7z8000/` is a diff against the V7 baseline. Moved to `emu/test_driver.cpp`, reached from the kernel build through a `DRIVER_DIR` cache variable in the same style as the existing `EMU_DIR` and `TOOLS_DIR`.

**Test:** PASS at 39,258,572 cycles — identical to the previous emulator, so none of the 12 upstream fixes changes behaviour for this kernel.

## Step 17: Compiler Update, Underscore Names, Two Kernel Races, the V7 C Library

Brought the compiler up to date under a regression gate, moved to PDP-11 style symbol names, fixed two interrupt races the new compiler's timing exposed, closed three ways the build and test could hide a failure, and built the Seventh Edition C library.

### Compiler

`PCC-z8000` moved from `510a0f5` to its current head. The details are in that repository's README and commit messages; what matters here:

- A round of compiler changes that passed every compiler test had broken the V7 shell and seven kernel files. The compiler repository now has a gate (`make -C z8000/test gate`) that includes a *ratchet*: it compiles the V7 kernel, shell, commands and libc (686 files) and fails on any new diagnostic or unreviewed change in generated assembly. It needs this repository's `v7z8000` tree.
- `cgram.c` is generated from `cgram.y` by the Seventh Edition yacc, which now lives in the compiler tree.
- Plain `int` bit-fields are unsigned and `08`/`09` are accepted in octal constants, as in the original compiler. V7 `make` depends on the first.

### C symbols carry an underscore

`cz8` now names C symbols as the PDP-11 compiler does: a leading underscore, eight characters in all, so seven of the C name are significant. The compiler's support routines (`lmul`, `ldiv`, `fadd`, ...) have none. Before this a C program that defined `flt` or `ldiv` collided with the runtime.

- `krt.s`: every label called from C is `_name` (`_save`, `_resume`, `_spl0`, `_fubyte`, ...) and references to C symbols are `_trap`, `_clock`, `_hdintr`, `_consrint`, `_main`, `_useg`.
- `tools/libc`: system call stubs, `setjmp`/`longjmp` and `errno` are underscored; the C-level `_exit` is `__exit` in assembly.
- `tools/libc/end.c` is gone: C's `end` is now the linker's own `_end`.
- In `libc.a`, `errno.b` comes last. The linker loads an archive member only if it defines something, and uninitialized data does not count.

### Two interrupt races in `krt.s`

Both were latent; the new compiler shifted instruction timing enough to hit them.

1. **`resume()`** remapped the u-area and restored SP about ten instructions later with interrupts enabled. In that window the stack pages belong to the new process while SP is still the old one, so a clock interrupt pushed its frame over the new process's kernel stack. Symptom: the shell prompt appeared, the command never ran, and the CPU ran off into BSS. `resume()` now masks interrupts from the remap until SP is restored, as V7's PDP-11 `resume` does with `bis $340,PS`.
2. **`spl5` was backwards.** On the PDP-11 it blocks devices and leaves the clock running. Ours set VIE and cleared NVIE, so `ttstart()`, called from inside the console interrupt handler, turned device interrupts on and a second character's handler ran in the middle of the first one's echo. Symptom: echoed characters dropped and reappearing later (`eco hello`, `hhello`); the build from before the compiler update showed it too. `spl4`/`spl5` now clear VIE and leave NVIE alone.

### Build and test no longer hide failures

- **Compiler errors.** `pcc_compile.sh` used to merge `cz8`'s stderr into the assembly and strip every line starting with `"`, which is how `cz8` prefixes diagnostics. A file the compiler rejected still "built". Diagnostics now go to the terminal and any failure fails the build. The kernel has one warning: `sys/trap.c` line 31, "illegal pointer combination".
- **Dependencies.** Kernel objects depend on `cz8`, `az8`, `ldz8` and every header in `h/`. Before, a compiler update or a header edit rebuilt nothing.
- **Boot test.** It used to pass if the output contained `hello`, which the echoed command line always does. It now requires the pipeline's own output line, a prompt after it, the system at rest in `idle()`, and the exact transcript.
- **Linker.** `ldz8` used to exit 0 after reporting undefined or multiply defined symbols.

### The Seventh Edition C library

`tools/libv7.a` is built from the unmodified V7 sources in `v7z8000/usr/src/libc/gen` and `stdio` (77 files). V7's PDP-11 assembly is replaced by Z8000 files in `tools/libc/`:

| File | Replaces | Contents |
|------|----------|----------|
| `doprnt.c` | `stdio/doprnt.s`, `fltpr.s`, `ffltpr.s` | the `printf` conversion engine |
| `fpsup.c` | `gen/modf11.s`, `ldexp11.s`, `frexp11.s` | `modf`, `ldexp`, `frexp` for IEEE doubles |
| `exit.c`, `fakcu.c` | `gen/cuexit.s`, `fakcu.s` | `exit` flushing stdio, and the dummy `_cleanup` |
| `abort.c` | `gen/abort.s` | `abort` |
| `syscalls.az8`, `setjmp.az8`, `sbrk.c` | `sys/*.s`, `gen/setjmp.s` | system calls |

`crt0` now calls C `exit()` after `main`, as V7's does. Not built: `mon.c`, `mpx.c`, `pkon.c`, `nlist.c` (needs the `a_flag` field our `a.out.h` renamed) and `stty.c` (the system call stubs already provide `stty`/`gtty`). Every `printf` user links the floating-point formatter; V7 avoids that with the `fltused` trick, not reproduced here.

`tools/libctest.c` runs 33 checks under the kernel: strings, `ctype`, `malloc`, `qsort`, `sprintf` with ints, longs and doubles, `atof`, `sscanf`, and file I/O with `fopen`/`fgets`/`fclose`.

### Emulator

`z8000_emu` moved from `f376c54` to `193db6f`. Upstream now counts cycles in 64 bits, so `get_cycles()` returns `uint64_t`; `emu/test_driver.cpp` prints and compares it as such and `-c` accepts limits above 2^31. Upstream also changed how bit 15 of the PC segment word is carried and restored the EPU trap for extended instructions; neither affects this kernel, whose boot test runs in exactly the same number of cycles as before. The compiler repository's own copy of the emulator moved to the same commit.

### Tests

```sh
cmake --build build --target test        # boot test, exact transcript
cmake --build build --target test-libc   # libctest under the kernel
make -C PCC-z8000/z8000/test gate         # compiler gate, including the ratchet
```

`emu/test_driver.cpp` takes `-d <hd image>`, `-i <console input>` and `-x <expected text>` to run something other than the boot test.

**Test:** boot test PASS with the exact transcript `boot\nZ8000 Unix\n# echo hello | cat\r\nexit\r\nhello\r\n# # `. `test-libc`: `libc: 33 passed, 0 failed`.

## Step 18: Interrupt State, Preemption, and Signal Return

Restored the V7 interrupt rules across the Z8000 entry/return paths and
memory helpers. User mode and syscall C code run with interrupts enabled;
helpers restore their caller's mask. `spl5()` blocks devices while admitting
the clock, and interrupt return schedules only when returning to user mode.
A shared `userret()` checks signals and `runrun`, preserves the user SP
across switches, and masks the final return through IRET.

Restored the syscall `u_qsav` save and wait-channel handling in `setrun()`
so a signal can interrupt a blocking syscall. Fork copies the u-area and
user segment in short masked chunks, restoring the MMU window between
chunks; it charges clocks to the parent until the child actually resumes.

Added `test-preempt`: syscall-free spinning children, kill/alarm delivery,
interrupted pipe reads, computation across context switches, and delayed
console input while another process spins. Boot, all 33 libc checks, and
both preemption runs pass. A negative control with user interrupts disabled
fails the new test. The boot trace services 14,556 of 14,579 generated ticks,
compared with 537 of 7,871 before. Direct measurement separates the remaining
difference into 22 merged pulses before the first shell prompt and one pending
request at shutdown. This boot ratio is not a sustained clock-drift rate.

The driver now supports fixed-length post-boot measurements, independently
counting generated pulses, actual NVI dispatches, merged pulses, and pending
requests at both sample boundaries. Ten nominal timer hours each of idle,
syscall-free spinning, and repeated preemption-suite workloads delivered all
6,480,000 measured ticks, with zero merges and no pending boundary requests.
The busy suite completed 66 iterations. These results apply to the current
5,000-cycle-slice test harness, not calibrated M20 hardware. Details and
reproduction commands are in
[interrupt-masking.md](interrupt-masking.md).

## Step 19: User Startup and libc Global Ownership

PCC `crt0.az8` now defines `_environ`, initializes it directly from the exec
stack, and calls `main(argc, argv, envp)`. Previously a C helper initialized
the environment, while startup reserved unrelated unprefixed `errno` and
`environ` symbols. Those obsolete definitions are gone. Returning from main
still calls `exit()` to flush stdio before the termination syscall.

The shared `cerror` in `syscalls.az8` declares `.comm _errno,2`, as V7's
error handler does. Both libc archives omit `errno.b` and `crtinit.b`,
removing the common-only errno member's ordering dependency. The unused
`errno.c` was removed at this step. The unbuilt ACK `crt0.s` and its
`crtinit.c` helper were subsequently removed during repository cleanup,
along with the old ACK syscall/setjmp/end assembly, init stub and unused
filesystem prototype. Library targets depend on the Makefile so membership changes
recreate the archives even when the remaining objects are already current.

Expanded `libctest` from 33 to 37 checks: startup argument/environment
pointers, syscall `EBADF` through the shared errno, and two fork/exec cases
with populated and empty environments. The children check the third main
argument, environment termination, `getenv()`, and initially zero errno.

**Validation:** `test`, `test-libc` (37 passed, 0 failed), and both
`test-preempt` runs pass. No kernel behavior or toolchain changes were needed.

## Step 20: Caught Signals and User Context Restoration

`psig()` now delivers caught signals at user return using an eight-byte user
frame. Libc registers a shared trampoline and keeps the application's handler
addresses in a per-process table. The trampoline preserves R0-R14, restores
condition flags with unprivileged LDCTLB, and returns to the interrupted PC
and SP. No new syscall or privileged user-context restore is needed. Default,
ignored, one-shot, and persistent SIGILL/SIGTRAP dispositions follow V7 rules.

The assembler lacked LDCTLB, so PCC's az8 now recognizes FLAGS and both byte
register transfer directions, using the Z8000 manual's encodings. A dedicated
assembler regression exercises both high and low byte registers. All 78
assembler/compiler regression cases pass.

The handler longjmp test exposed an existing libc bug: setjmp saved an SP
that already excluded its argument, although the resumed caller removes that
argument itself. Saving SP just past the return address fixes the two-byte
stack imbalance and the later nested-call argument corruption it caused.

Added `test-signal`: handler return and old-disposition results, asynchronous
restoration of every general register and the six condition flags, syscalls
inside handlers, an interrupted pipe read returning EINTR, nested signals,
longjmp, fork inheritance, default second delivery, exec disposition reset,
and rejection of an unusable signal stack. The driver waits for completion
before sending exit, allowing idle time while the alarm is pending.

**Validation:** signal, boot, 37 libc checks, and both preemption tests pass.
See [kernel reference](../kernel/processes-and-exec.md#caught-signals)
for the ABI and remaining limits.

## Step 21: Terminal parameter control

The existing terminal-control code now follows `ioctl(fd, command, address)`.
`stty` and `gtty` remap their two arguments into that convention, and `ioctl`
dispatches through the character driver's `d_ioctl` entry. Previously it read
the command and address in the opposite order and bypassed the driver.
The console now provides `consioctl`, using the common V7 tty handler.
Raw console output preserves high-bit bytes instead of treating them as delay
markers. The public `sgtty.h` now exposes `TIOCFLUSH` as defined by the kernel.

`cmake --build v7z8000/usr/sys/build --target test-tty` runs four emulator
sessions, delivering input after the program has installed its settings. It
checks `gtty`/`stty`/`ioctl` settings round trips and restoration, special
characters, descriptor errors, close-on-exec flag commands, flushing an empty
queue, echo on/off, cooked erase/kill editing, raw and cbreak reads without a
newline, and preservation of raw output bytes 0x80 and 0xff.

This validates the host emulator console. Baud settings are stored metadata;
the driver does not program physical serial hardware. Alternate line disciplines
and modem-control commands remain unsupported (`ENOTTY`). This step adds no
`stty` command-line utility.

## Native PCC bootstrap audit

Run `python3 tools/native-audit.py` after building the kernel and its boot disk
(`cmake --build v7z8000/usr/sys/build --target kernel disk_image`). The audit
cross-compiles the native tools against V7 headers and `libv7.a`, generates
cpp's expression parser with the repository's V7 yacc, and saves objects,
per-stage diagnostics, a JSON report, and an emulator smoke-test log under
`tests/build/native-audit/`. It does not alter toolchain or Unix sources to
make failed files compile. A completed audit is not a successful bootstrap;
individual build failures remain in `report.json`.

Initial results with PCC `ffdbb7d`:

| Tool | Result | Measured size in bytes |
|---|---|---|
| `cpp` | Links and runs under Unix | Text 34,656; data 2,820; BSS 17,406; total 54,882 |
| `cz8` | 13 of 15 files assemble | Partial text 71,824; data 20,140; common 23,572; total 115,536, excluding two failed files and libc |
| `az8` | 8 of 9 files assemble | Partial text 32,424; data 7,096; common 1,466; total 40,986, excluding the scanner and libc |
| `ldz8` | Compilation fails against V7 archive headers | Target layout probe: symbol table 56,042; hash table 8,010; local-symbol pointers 8,000; these three tables alone total 72,052 |
| `ccz8` | Compiles and assembles; link fails | Undefined `_execv` in the current library |

The native cpp smoke test uses `cpp -P /tmp/probe.c /tmp/probe.i` and reads the
result with `cat`. It verifies an included header, object-like and function-like
macros, a `#if` expression, suppression of the unselected branch, and file output.
It is a small functional check, not yet preprocessing the compiler's own sources.
Only 10,654 bytes remain above cpp's static image for heap and stack.

Blockers at the time of the initial audit (later steps record their fixes):

- `cz8/pftn.c:1085` and `az8/scan.c:417` fail with PCC's “expression causes
  compiler loop” diagnostic. Fix code generation; do not simplify the source
  expressions as a workaround.
- `cz8/local.c` uses `stdint.h`, `string.h`, `uint64_t`, and `memcpy` for
  floating-point constant output. V7 lacks those headers and this compiler has
  no native 64-bit integer type. Its constant emitter needs a native-capable
  representation while retaining correct host builds.
- `ldz8` expects text-header archives (`SARMAG`, `ARFMAG`, `ar_fmag`), whereas
  the installed V7 header describes binary archives. Settle the format shared
  by the linker, native archiver, and installed libraries.
- The monolithic compiler cannot fit even in a separate 64 KB instruction
  space: its incomplete code alone exceeds that limit. Investigate separate
  compiler passes or overlays as well as reducing data. The linker needs
  smaller or redesigned symbol storage. Swapping does not enlarge either
  virtual address space.
- The driver needs the missing `execv` library entry before it can link.

A longer console transcript (cpp without `-P`, followed by `cat`) stalled
mid-output until further console input arrived. This is consistent with
`ttwrite` draining through synchronous `consstart` before setting `ASLEEP` and
sleeping: the wakeup happens too early. The audit does not fix this kernel
issue. The short `-P` smoke test avoids that queue-length boundary; sustained
compiler diagnostics will need the console drain/wakeup path fixed.

### Two-pass feasibility experiment

An isolated copy under `tests/build/twopass/` was built with `ONEPASS` removed.
Both halves compiled with the host C compiler, but initially failed to link:
the front end still called the in-process pass-2 interface, and the back end
still depended on front-end helpers and shared state.

A small experimental adapter connects the existing `prtree` writer to
`mainp2`/`eread`, transfers register-use and return-label state, gives each pass
its own common routines, and supplies the small helpers needed on each side.
The prototype uses `@` for expression records and passes assembly lines through
the reader, avoiding conflicts with assembly labels beginning with `.`. It also
restores a missing newline when serializing a local label. The production
compiler and build files were not changed.

Results: all 15 core compiler tests plus 8 selected floating-point, register,
and long-operation probes generated byte-for-byte identical assembly to the
one-pass compiler. All 23 generated programs passed in the Z8000 emulator.
This checks the split compiler running on the host, not native compiler
execution. The experimental patch, build helpers, intermediate files, and
results are retained in that ignored build directory (`prototype.patch`,
`connect.py`, `cases.json`, `extra-cases.json`, and `sizes.json`).

Cross-compiling the separated back end succeeded, including linking libc:
58,456 bytes text + 10,996 data + 9,630 BSS = **79,082 bytes**. At the time of this experiment, the loader only supported combined space;
this image could not run in that 64 KB address space. A successful linker
exit did not establish that the image fit. The front end still encounters
the `pftn.c` compiler error and `local.c` header/type dependency. Its successful
objects already total 69,980 bytes before those files and libc.

Conclusion: reconnecting two passes is a bounded integration task, supported
by a working prototype, rather than a compiler rewrite. Production work still
needs build/driver integration, a reviewed intermediate format and helper
implementations, broader regression coverage, and the native-build fixes above.
Splitting alone did not meet the then-current 64 KB combined-space limit. Separate
instruction/data maps would accommodate the measured back end's static sizes,
but the full front-end size and stack/heap requirements remain unmeasured.

## Step 22: Separate Instruction/Data Executables

Implemented 0411 executables (`ldz8 -i`) alongside existing 0407 programs.
The emulated MMU now selects a distinct instruction bank through port 0x00B8;
C pointers remain 16-bit. Exec loads text and data into their respective spaces,
fork copies both, and context switches restore the mapping. Kernel user-copy
helpers distinguish instruction and data accesses. Loader and linker checks
reject invalid or overflowing layouts before size fields wrap.

PCC's dense switch tables now reside in data space. Linker header and symbol
reads preserve unsigned 16-bit values, including code addresses above 0x8000.
The production compiler remains one-pass; this change provides memory support
for a future native two-pass compiler, not completed self-hosting.

`test-split` exercises over 91 KB of static program storage, including code
above 32 KB and a PC-relative instruction-space constant, initialized data,
BSS, switch tables, file I/O, signals, fork isolation, malformed exec, and
exec transitions between layouts. It also runs the libc and signal suites as
split binaries and rejects instruction, data, and combined-space overflow.
Existing boot, libc, preemption, signal, and terminal tests pass. The PCC gate
passes; reviewed assembly-baseline changes move switch tables between sections.

This implements the mapping in the emulator, with fixed backing banks and no
text sharing or write protection. FPGA implementation is separate work. The
native compiler front-end errors, assembler error, linker archive support, and
compiler-driver integration recorded above still need resolution.

### Native two-pass back-end execution

Cross-built the experimental back end as a 0411 executable and ran it under
Unix. The rebuilt image has 58,348 bytes of text, 11,100 bytes of initialized
data, and 9,630 bytes of BSS (79,078 bytes total static storage). The executable
file is 73,136 bytes including its retained global symbols.

All 15 core cases and eight additional floating-point/register cases passed.
For each case, the host front end produced intermediate input, the back end
ran as a Unix process with file input/output, and a guest helper exported its
output. The resulting assembly was byte-identical to the host back end's
output. Host assembly/linking and execution of all 23 generated programs also
passed. This verifies native back-end execution, not a fully native toolchain:
the front end, assembler, and linker in this experiment still ran on the host.

The experiment and logs are retained in the ignored directory
`tests/build/twopass/native-run/`: `back`, `runner.c`, `run.py`, per-case assembly
and logs, `results.json`, and `extra-results.json`. After building the prototype
and native back end, `python3 tests/build/twopass/native-run/run.py` repeats the
core cases; adding `extra` runs the eight additional cases. These remain
experimental build artifacts, not production two-pass integration.

The larger executable exposed a host image-builder limit: `v7mkfs` previously
held only 128 file block addresses and could not include this file. It now
supports the full single-indirect block plus double-indirect blocks, and checks
prototype-token lengths while accepting long host paths. The independent
`python3 tools/test-v7mkfs.py` check reconstructs six files across direct,
single-indirect, and double-indirect boundaries, including a second indirect
leaf and partial final block. Boot and split-space regression tests pass with
the updated builder. No kernel changes were needed for this experiment.

### Native front-end compilation fixes

Fixed the two front-end source failures in PCC itself. Integer-to-long
conversion templates incorrectly requested sharing with the right operand of
a unary conversion. Under register pressure, that prevented reusing the input
register and sent code generation into a loop. They now share the left operand;
unsigned conversion copies the low word before clearing the high word, so
sharing cannot destroy the source. `widen_pressure.c` reproduces the old failure
and checks both signed and unsigned results. This also resolves the previously
recorded `az8/scan.c` compilation failure.

`local.c` now emits IEEE floating-point constants through unsigned-byte views
of float/double values, with host byte-order detection. It needs neither modern
`stdint.h`/`string.h` nor a 64-bit integer type. The same routine was compiled
for Unix and emitted 16 exact float/double initializers, checked against
independent IEEE encodings (including signed zero). Probe sources and logs are
in `tests/build/frontend-fixes/`.

All ten experimental front-end sources now compile and assemble. The fresh
split-space link is still rejected: **78,668 bytes of text**, **14,988 bytes of
data**, and **22,298 bytes of BSS**, including rebuilt libc. Data/BSS fit;
text exceeds the 64 KB instruction address space by about 13 KB. No native
front-end execution is claimed. Code-size reduction or an overlay design is
needed before this pass can run; stripping symbols cannot reduce mapped text.

The compiler execution suites, Unix boot/libc/preemption/signal/terminal/split
checks, and rebuilt native back-end cases pass. Assembly-baseline changes were
reviewed for the conversion sequences and resulting register allocation; the
ratchet also records the newly compilable sources. Production PCC remains
one-pass, and the two-pass build remains an experiment.

## Step 23: Native Front End Fits and Runs

The original V7 distribution provides the relevant comparison. Its ordinary
PDP-11 compiler builds separate `c0`/`c1` programs (`v7unix/usr/src/cmd/c/makefile`).
Its PCC builds with `-i`, and uses shared `csv`/`cret` entry/return routines
(`v7unix/usr/src/cmd/pcc/code.c`). The shipped `v7unix/usr/lib/ccom` a.out header
records 59,264 bytes of text, 22,308 data, and 24,384 BSS: the whole PDP-11 PCC
fits separate spaces. Our larger generated code was a toolchain problem to
address, not proof that a native front end was impossible.

Added optional `PCC-z8000/z8000/c2z8.py` size optimization for generated C
assembly, plus `lib/csv.az8`. It shares function entry/return code without
changing stack layouts, removes redundant jumps/dead instructions, uses short
stack adjustments and zero loads, and combines suitable word transfers into
long transfers. Ordinary compilation remains unchanged. Compact output must
link the shared helper. The pass is for PCC-generated C assembly, not arbitrary
hand assembly. Its regression mode is:

```
python3 PCC-z8000/z8000/test/regress/run.py --compact --strict --build-dir tests/build/compact-regress
```

Native execution exposed overlapping allocation of an integer register and a
following `register long`; fixed pair placement and availability checks in
PCC, with `register_pair_overlap.c` covering the failure. Also guarded a debug
call omitted by release builds and reserved the first data word in user crt0:
otherwise a global at split data address zero could be mistaken for a null
pointer by libc.

The reproducible experimental two-pass build is now retained in
`tools/pcc-native/`, rather than depending on previous ignored build artifacts:

```
python3 tools/pcc-native/build.py
python3 tools/pcc-native/test.py
python3 tools/pcc-native/test.py extra
```

It constructs separate passes in `tests/build/native-pcc`, disables compiler
debug tracing, cross-compiles them, and compacts both passes and their private
libc archive. Sizes before the separate EPU service (Step 24) were:

| Pass | Text | Data | BSS |
|------|-----:|-----:|----:|
| Front | 65,380 | 13,920 | 22,338 |
| Back | 48,420 | 10,624 | 9,630 |

Both fit 0411 spaces. That front end had only 154 bytes below the largest even
text size, so future changes must keep the link-time size check. The native
test runs both passes under Unix, exports the generated assembly, then uses
the host assembler/linker and emulator to validate the resulting programs.
Floating-point output can differ in the last bit because V7 `atof` and the
host decimal parser round differently; output comparisons and execution
results are recorded separately. All 25 native two-pass cases execute correctly;
24 produce byte-identical assembly, while `float_general` differs by one low
bit in one double constant. All 80 compact-code regression cases, the full PCC
gate, and Unix regression targets pass. This is native compiler-pass execution,
not completed self-hosting: native preprocessing/assembly/linking and compiler
driver integration still need to be assembled into the full workflow.

## Step 24: Separate Zilog Software EPU Service

Imported the preserved Zilog arithmetic/decoder from CP/M-8000 into
`v7z8000/usr/sys/fpe/fpe.z8k` (SHA-256
`d4173895dc6e4bacbbdd4966faac0fd1883dcb7a13e9c7c6f0ba63d3eb541ae8`).
The accompanying `VERIFICATION.md` is the upstream report, not a claim that
its entire TestFloat suite was rerun here. Source syntax is translated for
GNU as; the engine itself is unchanged. Unix supplies trap entry, per-process
state and split I/D memory helpers. Segment 127 is reserved for its code/data,
with current u-area/stack pages aliased through UPAGE.

PCC-compatible wrappers execute EPA instructions. The CPU traps into the
service, which runs guest integer assembly with interrupts enabled. Fork,
exec and caught signals preserve/reset state appropriately. The signal frame
now includes 96 EPU bytes and libc initially restored them using syscall 52
(moved to 62 in upper-layer batch 5), requiring
relinking programs that use caught signals. Unsupported opcodes, including
upstream's broken FINT, are rejected instead of silently miscomputing or hanging.

The new kernel code initially crossed the 0xe000 MMU copy window. Applying the
existing, tested C compaction pass to kernel C provides space; an explicit
build check now prevents another overlap. Hand assembly is not compacted.

Native compiler sizes with the service runtime:

| Pass | Text | Data | BSS |
|------|-----:|-----:|----:|
| Front | 60,728 | 13,920 | 22,338 |
| Back | 43,768 | 10,624 | 9,630 |

Each pass saves 4,652 bytes of text; the front now has 4,806 bytes below the
largest even text size. Its data/stack space is unchanged. The compact user
FP components total 4,412 bytes (ABI wrappers 1,848, glue 2,084, EPA primitives
480), replacing 9,080 bytes of arithmetic/wrappers; the signal trampoline
accounts for the 16-byte difference in executable savings.

Validation: six arithmetic/vector programs plus the process/signal EPU test
run in both combined and split I/D layouts. Tests include signed/unsigned
conversion, memory transfers, fork/exec, asynchronous signal preservation,
concurrent arithmetic, and fault delivery. All 25 native two-pass compiler
cases compile and execute successfully, with the existing one-low-bit
`float_general` decimal-parser difference. Kernel boot, libc (37 checks),
preemption, signal, split I/D and terminal targets pass. Numerical caveats
from the original FPE remain documented in the kernel reference.

## Step 25: Native Assembler and Linker

`az8` and `ldz8` now build as 0411 executables and run under Unix. Reproduce
the build and guest tests after building the kernel and boot disk:

```
python3 tools/native-binutils/build.py
python3 tools/native-binutils/test.py
```

Artifacts, sizes, guest transcripts, exported objects/executables and test
results go to `tests/build/native-binutils/`. These builds use ordinary PCC
output and `libv7.a`; they do not require the Python compaction pass.

| Tool | Text | Data | BSS | Data space above static storage |
|------|-----:|-----:|----:|-------------------------------:|
| `az8` | 58,324 | 8,768 | 2,754 | 54,014 |
| `ldz8` | 32,200 | 2,268 | 17,646 | 45,622 |

The final column is shared by heap, stack and startup arguments; it is not a
measurement of peak free memory. The assembler has 7,210 bytes below the
largest even text size.

The linker reads an explicitly defined portable ASCII archive format,
independent of the installed V7 binary-archive header. Bootstrap libraries
must use that format. Step 30 adds the matching native archiver and make reader.
Symbol records are allocated in stable blocks of 32, preserving insertion
order and the 4,003-symbol limit. Fixed symbol/hash/local tables occupy
16,262 bytes instead of 72,052. Actual capacity still depends on available
heap; rejected archive members release their names and reuse symbol slots.

Guest testing also exposed host assumptions in the assembler and linker:
header output read the first two bytes of a `long`, which wrote the wrong
half on the big-endian target. Header fields now serialize numeric values.
Register helpers now receive explicit `int` arguments instead of K&R calls
passing `long` operands; the assembler's seek offset and dot-padding count
also use the correct long width.

Tests compare native objects and final executables byte-for-byte with host
output, then execute the native-linked programs under Unix in both 0407 and
0411 layouts. Coverage includes archive selection and rollback across symbol
blocks, common/data/BSS and pointer relocation, dot padding, long arithmetic,
byte registers, and floating-point storage/arithmetic through the EPU service.

Step 26 integrates native cpp, compiler passes, assembler/linker and the driver.
Steps 27–28 remove the host Python compaction dependency; Step 29 records
self-hosting and workload-specific heap/stack measurements.

## Step 26: Native C Compiler Driver

The native toolchain disk now installs `/bin/cc`, `/bin/az8`, `/bin/ldz8`,
`/lib/cpp`, `/lib/front`, `/lib/back`, startup code, a portable-format
`/lib/libc.a`, and the V7 headers under `/usr/include`. Build and test it with:

```
python3 tools/native-cc/build.py
python3 tools/native-cc/test.py
```

The build refreshes both compiler passes and the assembler/linker first.
Its bootable disk is `tests/build/native-cc/hd.img`; the tests leave a clean
compiler disk after completing. Boot it with the kernel test driver from
`v7z8000/usr/sys/build`. The installed `/usr/src/hello.c` supports a quick
guest-shell check:

```
cc /usr/src/hello.c -o /tmp/hello
/tmp/hello
cc -i /usr/src/hello.c -o /tmp/hello
/tmp/hello
```

`ccz8.c` builds with `TWOPASS` for this installation, selecting front-end
tree output and then the back end before assembly. Its historical one-pass
configuration remains available. The driver supports preprocessing (`-E`,
`-P`), assembly output (`-S`), object output (`-c`), ordinary linking and
split-I/D linking (`-i`), multiple inputs, and `-D`/`-U`/`-I` options.
At this stage `-O`, `-p` and `-f` were rejected. Step 28 adds native `-O`;
alternate profiling/no-FP startup objects remain unavailable.

| Newly installed tool | Text | Data | BSS |
|----------------------|-----:|-----:|----:|
| `cc` | 19,976 | 1,556 | 3,248 |
| `cpp` | 29,064 | 3,268 | 17,406 |

Both use 0411 layouts. `execv` is supplied as the Z8000 C counterpart of
V7's PDP-11 assembly wrapper, passing `environ` through to `execve`.
Driver fixes include preprocessing-only setup, closing the temporary-name
reservation descriptor, argument-vector capacity, missing-option checks,
and propagating any assembler failure. Direct assembly tests exposed another
V7 issue: appending `.tmpr` and `.tmpd` to a source name could collide at the
14-character directory limit. The assembler now uses short distinct names
containing its process ID; the assembler sizes above include this fix.

Guest tests exercise multi-file C compilation in 0407 and 0411 layouts,
header lookup and macro options, integer and EPU arithmetic, environment
inheritance across driver/pass execution, each intermediate-output mode,
object-only linking, and failures in preprocessing, compilation, assembly and
linking. Checks also verify temporary-file cleanup. Logs and results are in
`tests/build/native-cc/`.

This establishes a complete native C compilation pipeline for programs that
fit its segments. At this step the initial compiler binaries still required
host-side compaction; Step 27 removes that size requirement. Compiler rebuilds
inside Unix and peak heap/stack measurements are recorded in Step 29.

## Step 27: Shared Function Entry and Return Emitted by PCC

PCC's front end now emits `ld r8,#_F...; call csv` directly, and the back end
emits `jp cret` at the return label. Both one-pass and two-pass compilation
use these sequences. The existing helpers retain the fixed frame, argument
offsets, callee-saved registers, and R0–R3 return values; this is compatible
with older inline-frame C and hand-written assembly using the same ABI.

Both Unix libc archives now supply `csv.b`. Standalone compiler tests and
runtime builds link it explicitly; the kernel already linked its own copy.
The optional Python optimizer recognizes the new output and continues its
other instruction and branch simplifications. It can still convert older
inline-frame assembly, but new output does not need that conversion.

The native compiler passes now fit even without that optimizer:

| Pass | Text without Python optimization | Text with it | Data | BSS |
|------|---------------------------------:|-------------:|-----:|----:|
| Front | 64,084 | 60,640 | 13,764 | 22,338 |
| Back | 45,960 | 43,564 | 10,464 | 9,630 |

The unoptimized front has 1,450 bytes below the largest even text size;
the optimized front has 4,894. These are code margins, not measurements of
heap/stack headroom. The default build retains the remaining optimizations.
To reproduce the build without running the Python optimizer:

```
python3 tools/native-cc/build.py --no-compact
python3 tools/native-cc/test.py
python3 tools/pcc-native/test.py
python3 tools/pcc-native/test.py extra
```

`tools/pcc-native/build.py --no-compact` also builds just the passes and their
runtime without applying the optimizer. These are still cross-build scripts;
native rebuilding is established separately in Step 29.

Direct entry/return generation also reduces the ordinary native tools, whose
builds do not use the Python optimizer: assembler text is 51,884 bytes, linker
28,840, driver 18,032, and preprocessor 26,432.

Validation covers all 25 native compiler cases with optimization disabled,
all six driver integration suites in both optimized and unoptimized builds,
and the host compiler gate components. The existing `float_general` one-bit
decimal-parser difference remains. Against compiler revision `7a2036c`, all
520 changed assembly files in the source-corpus comparison differ only in
function entry/return sequences; the reviewed baselines were refreshed,
including earlier source changes. Kernel boot, libc, split I/D, preemption,
signal and terminal tests pass.

## Step 28: Native Assembly Optimizer

`PCC-z8000/z8000/oz8.c` implements the remaining assembly optimizations in
K&R C and runs under Unix as `/lib/oz8`. The two-pass driver now supports
`cc -O`, including `-O -S`, `-O -c` and split I/D linking with `-i`.
It removes unreachable instructions and redundant moves, redirects branches,
eliminates jumps to following labels, shortens zero loads and small stack
adjustments, and combines compatible word pairs into long operations.
Shared function entry/return sequences still come directly from PCC.

The pass streams through temporary files, so input size is independent of
the 64 KB data address space. Its optional jump map has a 24,000-byte
accounting budget; when full, further entries are skipped without changing
program semantics. Input lines must be shorter than 512 bytes. It is intended
for current PCC-generated C assembly using `csv`, not arbitrary assembler
programs. Errors and handled termination signals remove its temporary files;
an optimizer failure stops the driver before assembly/linking.

Native `oz8` uses 21,524 bytes of text, 932 of data and 1,922 of BSS, plus
dynamic storage and stack. The driver now uses 18,036 bytes of text. The
compiler passes remain at 60,640 bytes of text for the front end and 43,564
for the back end, with the data/BSS sizes recorded in Step 27 unchanged.
Peak compiler heap/stack usage and a native compiler rebuild remain untested.

The compiler cross-build and kernel build now use a host build of the same
C optimizer. Python `c2z8.py` remains a regression reference and converter
for older inline-frame assembly, not a production build dependency.

Validation includes byte-for-byte comparison with the Python reference over
520 generated source-corpus files, branch/register-pair fixtures, execution
after jump-map saturation, overlong-line rejection, and all 80 optimized
compiler regressions. The native optimizer processes an 82,889-byte assembly
file under Unix and produces exactly the reference output. Driver tests cover
optimized/unoptimized combined and split executables, intermediate output,
failure propagation and temporary-file cleanup. The compiler gate components
and kernel boot, libc, split I/D, preemption, signal and terminal checks pass.

Reproduce with:

```
make -C PCC-z8000/z8000/test optimizer
python3 tools/native-cc/build.py
python3 tools/native-cc/test.py
```

## Step 29: Native Compiler Self-Hosting

The two-pass compiler and optimizer now rebuild themselves under Unix.
`tools/native-cc/selfhost.py` installs the prepared compiler sources, then
uses native `cc -O -c` and `cc -i` to build `front`, `back` and `oz8`.
It repeats the build using the first generation's passes through
`-B/tmp/s1/ -t012`. All 19 object files and all three linked executables are
byte-for-byte identical between generations. The native front and back also
match their cross-built seed executables. Native optimized `oz8` has 20,344
bytes of text, versus 21,524 for the unoptimized seed.

The first attempt exposed assembler memory exhaustion on `pftn.c`.
Each span-dependent branch allocated a private copy of identical range
tables. `az8/sdi.c` now interns those immutable tables and frees the shared
pool once after resolution. No instruction forms or range limits changed.
The formerly failing file assembles successfully; all 18 existing compiler
translation units produce exactly the same object bytes as before the fix.
A new 700-branch native regression checks both combined and split linking.

The emulator can now save the guest HD with `-o` and record user memory
observations with `-P`. The runner calls `sync()` before completion, allowing
each successful build step to be checkpointed. Failed steps retain their
log and memory report without replacing the last successful disk; when the
emulator exits normally, its diagnostic disk is retained too. Run the script
again without `--setup` to resume.

Observed memory use during the native rebuild is:

| Executable | Text | Data | BSS | Peak break | Lowest SP | Minimum heap/stack gap |
|------------|-----:|-----:|----:|-----------:|----------:|-----------------------:|
| Front | 60,640 | 13,764 | 22,338 | 37,184 | 64,328 | 27,144 |
| Back | 43,564 | 10,464 | 9,630 | 20,096 | 64,328 | 44,232 |
| Optimizer | 20,344 | 932 | 1,922 | 5,952 | 63,138 | 57,186 |
| Assembler | 52,036 | 8,768 | 2,754 | 56,640 | 65,016 | 8,378 |
| Linker | 28,840 | 2,268 | 17,646 | 33,280 | 65,036 | 31,756 |

These are observations for this workload, not worst-case bounds for arbitrary
source. The break includes data/BSS, heap and the kernel's 64-byte rounding.
The gap is the smallest per-invocation difference between its lowest observed
SP and highest break; those extrema need not occur together. Consequently,
subtracting the independently aggregated table columns can give a slightly
different result. The front end retains 4,894 bytes of code headroom below
the maximum even text size. The assembler has the tightest observed data-space
margin. A separate probe checks the observer against a known heap allocation,
a rejected break request, recursive stack frames and a persisted guest file.

Validation: all 52 native build/link/execution steps pass, as do the 25
compiler execution cases using the second-generation passes, the compiler
gate, and ten native assembler/linker cases. The previously documented
`float_general` decimal-parser bit difference remains; its generated program
passes execution.

This establishes compiler self-hosting against the existing native
preprocessor, assembler, linker and runtime archive. Step 30 extends the native
build to those supporting tools, libc and yacc. Source preparation still
uses `tools/pcc-native/prepare.py` on the host to stage the two-pass headers
and glue; neither native generation invokes a host compiler or optimizer.

With the kernel already built, reproduce with:

```
python3 tools/native-cc/build.py
cmake -S v7z8000/usr/sys -B tests/build/selfhost/host -DCMAKE_BUILD_TYPE=Release
cmake --build tests/build/selfhost/host --target test_driver -j4
python3 tools/native-cc/selfhost.py --setup
python3 tools/native-cc/selfhost.py --summary
```

`--limit N` runs at most N additional steps. Results, saved executables,
per-step logs and memory observations are under `tests/build/selfhost/`;
`convergence.json` records executable hashes and `memory.json` records peak
usage. `tools/pcc-native/test.py` accepts `--native-front` and `--native-back`
to test the saved `s2-link-front.out` and `s2-link-back.out` executables
(add positional `extra` for the additional ten cases).

## Step 30: Native Development Environment

`tools/native-cc/environment.py` builds the development tools inside Unix,
starting with the second-generation compiler from Step 29. Its 82 steps build
`make`, `ar`, `yacc`, `cp`, `rm`, `mv`, `cmp`, `cc`, `cpp`, `az8`, `ldz8`,
libc and startup code, then rebuild the compiler passes and optimizer with
those native tools. The support executables are finally relinked against
the native libc. The guest `/usr/src/makefile` provides `make all`.

Native `ar` and make's archive dependency reader now use portable ASCII
archives: `!<arch>\n`, 60-byte member headers, and even-byte member alignment.
The writer emits space-padded short names; readers accept an optional trailing
slash. Names are limited to 14 characters, matching the V7 filesystem.
These libraries are unindexed; GNU/BSD long-name and archive-index extensions
are not implemented by native `ar`/`make`. The linker retains its existing
portable archive reader; no original binary V7 archive support was added.
Legacy utilities such as `nm`, `ranlib` and `arcv` remain unported.

The archive is a container of object bytes. NONSEG, SEG and separate I/D
requirements belong in the object format, relocations, linker and loader.
Current programs use NONSEG 0407 or 0411 executables; this work does not add
full segmented compilation or linking.

Native yacc generates the make, cpp and PCC parsers. Its Z8000 MEDIUM
configuration reserves 7,000 state words because PCC's grammar needs 6,492.
The resulting PCC parser is byte-identical to the checked-in parser. Yacc
uses 35,596 text bytes, 5,736 initialized data bytes and 52,114 BSS bytes;
PCC parser generation leaves an observed 4,156-byte heap/stack gap. This is
a workload measurement, not a bound for arbitrary grammars.

The libc build produces 92 members, all byte-identical and in the same order
as the compact cross-built library. A missing `ftime` syscall wrapper was
added for `ctime`, used by `ar tv`. Its 16 bytes in the shared syscall object
bring the front end to 60,656 text bytes and the back end to 43,580.
Source staging still uses the host to prepare two-pass glue and EPU wrapper
assembly; this is not a rebuild of every V7 command or of the kernel.

After building the kernel and completing Step 29, reproduce with:

```
python3 tools/pcc-native/build.py
python3 tools/native-cc/environment.py --setup
python3 tools/native-cc/environment.py --summary
python3 tools/native-cc/test-archives.py
```

Running without options resumes the saved disk. `--limit N` limits additional
steps, `--refresh` updates staged sources while retaining guest outputs, and
`--from-step N` restarts at a zero-based step index. Artifacts are under
`tests/build/native-environment/`: `hd.img`, per-step logs and memory traces,
`results.json`, `summary.json`, and exported executables in `native/`.

Validation includes all 82 build steps, 37 native libc checks, 25 compiler
execution cases, ten native assembler/linker cases, the compiler gate and
kernel libc/split-I/D suites. Portable archive tests cover host/native
interoperability, odd-sized and 90,001-byte members, replacement, extraction,
deletion, malformed headers, library linking, and make's member/symbol
dependencies and C/yacc/assembly suffix rules. The previously documented
`float_general` decimal-parser bit difference remains; execution passes.

## Step 31: Selectable Kernel Machine Configuration

The kernel now keeps shared services in `sys/`, drivers in `dev/`, CPU/MMU
support in `machine/`, and machine selection in `conf/`. The earlier journal
paths `sys/machdep.c`, `sys/trap.c`, `sys/fpe.c`, `dev/conf.c`, and top-level
runtime assembly refer to the layout before this step.

`conf/emulated.cmake` selects reset/trap assembly, runtime objects, machine C
sources and drivers. `conf/emulated.c` owns device switch tables, root/pipe/swap
devices, console output, clock enabling and device interrupt dispatch. The
CPU entry code calls `devintr(vector)` instead of naming disk/console drivers.
`main()` delegates initial mapping and boot-device selection; the scheduler
calls `copyuarea()` and `copyproc()` instead of programming a copy-window port
or deriving banks from process slots. The paged-MMU implementation owns those
operations and the atomic mapping/context restore in `machine/pagert.s`.

Select the configuration with `-DKERNEL_CONFIG=emulated`. This is the default
and currently the only implemented machine. `kernel` builds guest artifacts
without depending on the host emulator; `-DKERNEL_HOST_TESTS=OFF` omits the
emulator/test targets entirely. See [configuration and machine interfaces](../../v7z8000/usr/sys/conf/README.md)
for adding drivers or an MMU implementation and for the remaining fixed ABI
requirements. This reorganization does not restore omitted V7 memory policy
or add support for another physical machine.

Validation: a fresh Release build passes boot, libc, preemption, signals,
terminal, split-I/D and EPU suites. A separate kernel-only build produces
identical guest artifacts. ROM, trap and EPU binaries are byte-identical to
the pre-reorganization versions. Invalid configuration names are rejected.
The compiler ratchet includes `machine/*.c` and `conf/*.c`, covering all 29
kernel C sources after the split.

## Current State

The maintained status is [current status](../status.md).

## Divergence from Pristine V7

Historical snapshot from the early port. For current differences, see
[V7 compatibility](../development/v7-compatibility.md).

`v7unix/` holds the pristine TUHS V7 tree, so divergence is measurable at any time by diffing it against `v7z8000/`. As of Step 16, the 47 kernel files with a V7 ancestor total 6,835 lines with 3,438 diff lines (that metric double counts, since a modified line is one delete plus one add). Fifteen files are byte-identical to V7: `alloc.c`, `prim.c`, `partab.c`, `buf.h`, `callo.h`, `conf.h`, `dir.h`, `fblk.h`, `filsys.h`, `ino.h`, `inode.h`, `mount.h`, `stat.h`, `timeb.h`, `tty.h` (twelve as of Step 16; the other three since the header workarounds were removed). Four have no V7 ancestor at all — `cons.c`, `hd.c`, `md.c`, and `conf.c` (V7 generates that one with `mkconf`).

The divergence falls into three kinds, only two of which should shrink:

1. **Machine dependent** — `machdep.c`, `trap.c` and `slp.c` are rewrites because V7's are PDP-11; `iget.c` carries big-endian 3-byte inode addresses; `param.h`, `seg.h`, `reg.h`, `user.h` and `proc.h` carry the `label_t` layout and MMU model; much of `sys1.c` is `exec()`. This is the port. It stays.
2. **Missing features** — caught/default signals and `psignal` are implemented, but ptrace, core dumps and automatic stack growth remain absent. `tty.c` lacks alternate line disciplines and multiplexor support; `bio.c` lacks `physio` and swapping; unsupported syscall slots still use `nosys`; `main.c` is a reduced startup. Restoring these moves the files back toward pristine.
3. **Toolchain workarounds** — introduced for ACK, which is gone. The two header workarounds have been removed, see below.

### The ACK-era header workarounds are gone

`tty.h` and `inode.h` used to diverge because ACK's frontend rejected the PDP-11 C extension of unnamed struct members inside a union. Both are now byte-identical to V7.

- **`tty.h`**: `struct tc;` inside the union is unnamed again and `tun` is `tp->t_un`.
- **`inode.h`**: the union `i_un` with its two unnamed structs is back, along with `struct group` and `mpxip` for the multiplexor (declared, still unused). The workaround had not been confined to the header: 21 accesses in eight C files had been rewritten from `ip->i_un.i_addr` to `ip->i_addr` (and likewise `i_rdev`, `i_lastr`). Those are restored to the V7 form, which also made `alloc.c` byte-identical.

The header and the accesses have to change together. PCC resolves a member of an unnamed struct relative to the union that contains it, so `ip->i_un.i_rdev` is right and a bare `ip->i_rdev` against the pristine header silently reads offset 0 of the inode. Restoring only the header gave `panic: no fs` at boot. (An earlier note here said the bare form had been checked and found correct. It had not: the old and the current compiler both get it wrong.)

Verified by compiling every kernel file before and after: the generated assembly is identical apart from label numbers and the new `_mpxip` common. Boot test and `test-libc` pass.

The cost is noise. `cz8` warns about the idiom, as V7's own PCC source does, 73 lines per full kernel build:

```
tty.h, line 59: warning: structure typed union member must be named
tty.c, line 85: warning: illegal member use: t_intrc
```

They are left on. "illegal member use" is exactly what flagged the header and source being out of step, and the compiler repository's ratchet records the exact set, so a new one is noticed.

## V7 upper-layer restoration: batch 1

Restored original V7 `pipe.c`, `sys3.c`, `sys4.c` and the syscall return union,
including named time/offset accesses. Corrected default signal termination to
put the signal in the low wait-status byte; ordinary exit remains high-byte.
Updated signal, preemption and EPU assertions and added independent normal-exit
and Bourne shell reporting checks. Libc now has 39 regression checks, including
32-bit time and seek returns, exercised in combined and split I/D layouts.

Validation: boot, libc, signals, preemption, TTY, split I/D and EPU tests pass.
All 29 kernel assembly outputs are unchanged except the signal-status fix;
the full 689-file compiler ratchet passes with reviewed kernel baselines.
`time(&value)` pointer storage is an existing missing libc behavior, recorded
in the audit; the time-return regression compares `time(0)` with `ftime()`.

## V7 upper-layer restoration: batch 2

Restored V7 `passc()`/`cpass()` I/D selection and byte-error accounting, with
parentheses correcting the original `passc()` ternary comparison. Restored
original `iomove()` bulk/byte selection and bulk-error handling. No machine
assembly changes or new memory-protection claims accompany this step.

Added `test-copy`: 655 cases per combined/split executable, using actual shared
functions, target headers and injected machine-helper failures. Covers all
spaces, directions, alignment, high-bit bytes, zero count, boundaries and
partial accounting. Boot, libc, signals, preemption, TTY, split and EPU tests
pass. The technical reference now defines the helper return/fault contract and
recorded the missing fault recovery, implemented in the following step.

## Machine-layer access-fault recovery

SEGTRAP now dispatches through a ten-site user-access recovery table. Faults in
those byte/word/bulk helper instructions return -1 after restoring mode, stack
and caller interrupt state. Other kernel faults panic; user faults deliver
SIGSEGV. Bulk helpers and read/write requests reject 64 KB address wrap; word
helpers reject odd addresses. The fixed-bank MMU still maps whole user spaces;
this does not introduce protected gaps or read-only text.

Exec vector faults now report EFAULT; faults during image/stack construction
take the existing fatal-image path. Signal-frame failures terminate with SIGSEGV,
and failed EPU restores leave previous state intact. Added a test-driver denied
bus access option, armed after a guest marker, which uses real CPU SEGTRAP
handling. The driver waits for an expected verdict before stopping at an idle
HALT, avoiding premature termination while longer tests sleep.

Validation: 30 guest fault scenarios (both layouts), 655 copy-policy cases per
layout, and boot/libc/signals/preemption/TTY/split/EPU suites pass. Tests include
mid-copy failure accounting and subsequent syscall/clock operation.

## V7 upper-layer restoration: batch 3

Restored pristine `file.h`, `mx.h`, `fakemx.c`, `fio.c` and `nami.c`, plus the
multiplexor branches in `sys2`, `iget` and TTY processing. The configuration
selects `fakemx.c` through `KERNEL_OPTIONAL_C`; syscall 56 returns V7's disabled
`EINVAL` result. No multiplexed device or active channel is installed.

Restored `ttioccomm()` recognized/unrecognized returns, discipline queries and
selection, and discipline ioctl dispatch. The console driver supplies ENOTTY
for an unhandled request. Preserved validation before flush and interrupt-safe
parameter updates; special-character updates now also commit only after a
successful copy. Discipline zero has configured callbacks; no alternate is
installed.

Validation: `test-v7-interfaces` covers >64 KB directory offsets, pathname and
file lifecycle operations, dup, pipes, disabled mpx, driver fallback and TTY
failure handling. Expanded real TTY tests and all existing runtime suites pass;
all 30 kernel C files compile and assemble, and the full compiler ratchet has
690 files. Kernel-only builds still work.

## V7 upper-layer restoration: batch 4

Restored ordinary `bio.c` code, DISKMON counters (including initialized NBUF)
and V7 word-based buffer clearing. Removed obsolete RAM-disk-only comments.
Swap/raw/physical-map paths remain separate because their machine contracts
are not yet implemented.

Fixed an HD driver prerequisite: queue outstanding buffers instead of replacing
one active pointer. V7 `bflush()` submits asynchronous writes with interrupts
masked, so multiple requests must survive until completion. The driver unlinks
before `iodone()`, handles errors without copying failed read data, and starts
the next queued request.

Validation: target-ABI tests using real cache/driver code with deferred/error
completions pass; a real-kernel 24-block write/partial-update/sync/save/reboot
round trip verifies disk contents. All existing runtime suites and compiler
ratchet pass. Panic flushing remains unchanged after source review: ordinary
`update()` can wait on a buffer held by the panicking path and needs a separate
bounded panic protocol before reinstatement.

## V7 upper-layer restoration: batch 5

Restored V7 syscall numbering and exec semantics together: slot 11 takes
pathname/argv and clears the environment; slot 59 takes pathname/argv/envp;
umask/chroot use 60/61. Both exec paths preserve the newly installed user
stack on trap return. Libc wrappers match, and execl now inherits environ
and returns failure to its caller. The EPU signal-state restore extension
moved from sysphys slot 52 to unused slot 62; sysphys remains unimplemented.

This is a coordinated binary ABI migration. Rebuild both libraries, relink
programs and regenerate disks with the matching kernel; old syscall aliases
would collide with the restored V7 meanings. Boot icode still uses slot 11.

Added test-abi for direct-slot and libc behavior in combined and split I/D:
exec environments (including poisoned R3 on SC11), execv/execl inheritance,
umask file modes, chroot isolation and slot 52 rejection. Existing runtime
suites and the full 690-file compiler ratchet pass. The 52-step native
self-host rebuild produces identical front/back/optimizer objects and
executables across two generations.

The full recursive native build exposed the old eight-process ceiling: V7's
shell sleeps and retries fork when the table is full. Raised NPROC to 16
(+224 bytes kernel BSS); the MMU bank reservations already derive from NPROC.
Added exhaustion/reaping/reuse coverage and updated the emulator memory profiler
to recognize both exec numbers. All 82 native development stages now pass,
including recursive make all; all 92 libc archive members and the native
compiler parser match their host-built counterparts. Runtime suites and the
compiler ratchet pass with the final 16-process configuration.

## V7 upper-layer restoration: batch 6, resource maps and RAM sizing

Restored V7's first-fit resource-map allocator and map declarations. Replaced
the private u-area bitmap and removed the unused segment allocator. The core
map uses 2 KB frames; `USIZE=64` now describes the actual 4 KB u-area/system
stack in V7 accounting clicks. The map is sized from board-reported installed
RAM, excluding fixed user banks and the dedicated EPU service.

The emulator's `-R KiB` setting controls the board RAM-size register and real
bus availability. Accesses to absent RAM cannot alias populated memory.
Fork returns EAGAIN on storage exhaustion and rolls back its provisional slot;
exit returns frames to the map. Test coverage includes original allocation
and coalescing logic, hardware bounds, repeated exhaustion/reaping/reuse in
both executable layouts, odd RAM tails and insufficient-memory boot rejection.

This first increment retained fixed user-bank reservations: with NPROC=16 they
consumed 2112 KiB before the u-area pool. The follow-up below replaces those
reservations. Neither increment enables swapping or shared text.

Validation: all existing runtime suites, 13 combined/split memory guest
scenarios, host physical-bus bounds and the 691-file compiler ratchet pass.
The kernel-only build and all ten native compiler pipeline cases also pass.

## Batch 6 continued: on-demand user-bank backing

Replaced per-slot physical reservations with core-map allocation of a 4 KiB
u-area and one or two 64 KiB user banks. Logical segments remain stable;
indexed MMU ports map them to allocated physical frames. Only ROM/kernel
(128 KiB) and the EPU service are reserved at boot. Fork rolls back partial
allocations, exit frees all backing, and split exec acquires its additional
bank before altering the old image. Combined exec returns the instruction bank.
Init's user bank and fresh instruction banks are cleared before use.

The combined memory probe now runs with init and shell at 332 KiB, the split
probe at 396 KiB. Whole-bank allocation still requires contiguous space;
page-level estabur/expand and shared text/swap remain separate work.

Validation: boot and all runtime suites, 17 memory guest scenarios, host MMU
bounds/remapping, the kernel-only build, the 691-file compiler ratchet and all
ten native compiler pipeline cases pass. Memory scenarios cover every partial
split-fork allocation boundary, failed exec preservation and repeated layout
changes at constrained RAM.

## Batch 6 continued: page-granular sections and real break allocation

Replaced whole-bank backing with separate text/data/stack extents rounded to
2 KiB pages. `estabur` validates and commits layouts; `expand` and `sbreak`
allocate/release data storage. Resize failure preserves old mappings and size
accounting. New pages and the exposed part of a retained partial page are zeroed.
Fork copies mapped sections only; unmapped gaps reject kernel transfers with
EFAULT and ordinary user accesses with SIGSEGV. Extents remain contiguous, and
growth temporarily needs both old and replacement storage.

Exec reserves a minimum 4 KiB stack including arguments, enlarged when arguments
plus 256 bytes need more. Automatic stack growth remains deferred: current SEGT
delivery does not provide general instruction restart/backout. Bourne shell
workspace stores now acquire heap space explicitly instead of relying on the
PDP-11-style SIGSEGV retry path. Long word expansion and here-documents exercise
those checks. Allocation failures retain the shell's prior break pointer.

Validation: runtime suites, 17 memory scenarios including both layouts at
256/258/320 KiB, host MMU tests, kernel-only build, compiler ratchet, ten native
compiler pipeline cases and the recursive-stack/profile/persistence probe pass.

## Batch 6 combined follow-up: stack backout, shared text and swapping

Added bus-visible first-word PC and fault-address/reason latches to the emulated
MMU, together with read-only/system-only page flags and stack write warnings.
Z8001 SEGT remains a post-instruction trap. Warnings preserve successful stores;
failed LD/LDM stores and selected CALL/PUSH forms can grow the stack and retry
with explicit SP backout. Failed reads, read-modify-write, protection violations
and unsupported forms remain SIGSEGV. Signal delivery grows its frame area first.
This does not invent Z8003/4 ABORT or hidden emulator register rollback.

Shared 0411 text now has inode ownership, resident/reference counts, ITEXT write
exclusion and immutable swap backing. Fork shares its read-only physical pages;
exec prepares text before replacing the old image; exit releases references.
The final reference frees RAM, swap and the inode. No unused sticky-text cache
is retained. The common exec argument buffer is serialized across sleeping I/O.

The kernel itself now uses split I/D, keeping its 16-bit C ABI. ROM and kernel
D/I banks reserve 192 KiB; vectors are in ROM data space at 0:1000. Code remains
at logical 1:0200, backed by physical bank 2, with kernel data starting at 1:0000.
A separate handler-data.bin joins the boot artifacts. This removes the combined
kernel's pressure against the e000 copy window without moving the u-area/stack.

Memory pressure swaps other eligible processes to a dedicated ATA secondary unit.
The root disk remains unit zero. Process 0 loads runnable nonresident images;
fork can write its saved child continuation directly to swap when two copies do
not fit in RAM. A private bounce buffer uses the configured block driver, with
interrupts enabled while waiting and bounded physical-copy masking. Swap size is
reported by the board; -S 0 disables it. Exhaustion rolls back allocations.

Validation covers 25 memory scenarios, including page-skipping stack frames,
CALL/PUSH backout, unsafe retry rejection, successful warning stores below SP,
text inode lifetime, low-RAM/full/disabled swap and direct-to-swap fork at 256 KiB.
The runtime suites, 692-file compiler ratchet (32 kernel files), kernel-only build,
all ten native compiler cases and the profile/persistence probe pass.
Raw physio, core dumps, ptrace, scattered-page allocation and general instruction
restart remain separate work; contiguous growth can still return ENOMEM.

## Batch 7: raw physical I/O

Shared physio now manages the special buffer and uninterruptible completion wait;
the MMU owns whole-range validation, process pinning and the opaque transfer
mapping. Raw ATA is character major 3, with aligned multi-sector transfers and
byte residuals after partial errors. A single staging sector keeps interrupt-time
copies independent of the currently running process. The active swap device
rejects raw access. Both emulator disks report errors beyond their attached size.

Tests include delayed completion with an alternate current process and real
0407/0411 syscalls at 8 MiB and 320 KiB, including concurrent workers, actual
swap traffic, page crossings and disk-end partial progress. The runtime and
25-scenario memory suites, kernel-only build, new test-physio target and
693-file compiler ratchet (33 kernel files) pass. See the
[raw-I/O contract](../kernel/devices-and-io.md#raw-physical-io) for alignment,
cache-coherency and replacement-MMU requirements.

## Batch 8: V7 core-file creation

Restored V7 fatal-signal/core-file policy, keeping filesystem operations in
sys/sig.c and isolating noncontiguous process-image writing in the MMU layer.
The core image has the port's 4 KiB u-area followed by click-sized data and stack;
shared instruction text is omitted. Z8000 register snapshots include all general
registers, FCW and PC, with R13/R14 passed from entry wrappers and SP captured
from the process-local return state. The existing trap frame is unchanged.

Two intentional policy corrections reject unequal effective/real group IDs as
well as user IDs before creating a file, and avoid reporting success for a
nonregular target. Incomplete writes retain a partial file and clear the core
wait-status bit. See the [format and tests](../kernel/processes-and-exec.md#core-dumps).

Validation: both layouts pass the core-image, permission and disk-full cases;
low-RAM runs verify swap traffic. Runtime suites, all 25 memory scenarios,
raw I/O, kernel-only build, the 693-file compiler ratchet and all ten native
compiler pipeline cases pass.

## Batch 9: V7 ptrace requests 0–8

Restored V7 stop/wait/IPC and signal selection, with CPU/MMU hooks for word and
register access. Traced exec stops before entry. Stopped children can swap and
resume requests against restored mappings. Register changes include R13/R14/SP;
privileged FCW fields and process metadata remain protected. Exclusive instruction
patches invalidate swap backing and prevent fresh exec sharing. SC 255 supplies
a breakpoint trap, with debugger-managed restoration and resume PC.

Single-step request 9 returns EIO. Per the selected scope, it will remain optional
and require hardware support; no software stepping is implemented. Wrapper growth
also exposed jump relaxation moving fixed kernel entry addresses. Nearby veneers
and a build-time short-jump check now protect that ABI.

Validation: tracing passes in both executable layouts at 8 MiB and 320 KiB,
including swap, breakpoint restoration, register writes and concurrent debuggers.
Runtime, raw-I/O, core-dump and all 25 memory scenarios pass, as do the kernel-only
build, 693-file compiler ratchet and all ten native compiler pipeline cases.

## Batch 10: exec credentials and CPU helpers

Restored V7 set-UID/set-GID exec rules, including tracing suppression and the
existing-effective-root exception. Credentials commit only after successful
image/stack construction. CPU helpers now own startup stack sizing and layout,
register/EPU reset and signal frames; shared code retains filesystem, credential,
signal-disposition and accounting policy. R13/R14 now reset with the other user
registers. Exec SP stays process-local through sleeping cleanup until trap return.

The new test-exec target covers credentials, tracing, initial registers/stack,
signal dispositions, failed exec and core suppression in both executable layouts
at 8 MiB and 320 KiB.

Validation: test-exec, ABI, signals, preemption, split I/D, EPU, access faults,
all 25 memory scenarios, core dumps and ptrace pass. The kernel-only build,
693-file compiler ratchet and all ten native compiler pipeline cases pass.

## Batch 11: public ABI, accounting, profiling and memory locking

Installed sys headers now match the kernel via an export/check tool. Core and
ptrace tests consume those public layouts. Libc adds effective-ID queries,
time-pointer stores, acct/lock/profil, and the unchanged V7 monitor routine.
The pstat user dump selects the Z8000 register image instead of PDP-11 MMU fields.

Restored original accounting routine bodies and flags, with serialization around
sleeping accounting-file changes and writes. Restored V7 CPU/disk tick counters
and user PC sampling through fault-safe CPU helpers. The original syslock policy
now reaches an eviction scan that respects SULOCK. Tests cover both layouts at
8 MiB and 320 KiB, accounting concurrency/full-disk recovery, profiling lifecycle,
monitor output and deterministic sampling/eviction policy.

The optimized split-I/D native compiler case completed at 2.011 billion cycles
with restored clock statistics, just above the former two-billion test limit.
Normal native cases now have a three-billion-cycle allowance; correctness
checks and the optimizer-specific budget are unchanged.

Validation: public-header core/ptrace/exec tests, the new services suite, all
existing runtime targets and 25 memory scenarios pass. Kernel-only and normal
builds, the 694-file compiler ratchet (34 kernel files), and all ten native
compiler pipeline cases pass. Accounting routine bodies and acct.h were checked
against pristine V7; only internal routine names and serialization wrappers differ.

## Planned Steps

The maintained plan is [next work](../status.md#next-work).

## Batch 12: V7 process and shared-text policy

Compared slp.c, sys1.c and text.c with the original sources function by function.
Exit/wait and ordinary sleep/run-queue policy were already closely reused;
setrun's channel-wide wakeup is original V7 behavior. Fork now restores V7's
per-effective-user count, MAXUPRC comparison and final-slot reservation for root.
The port retains its register return convention and recoverable allocation failure.

Moved victim selection from the MMU's first-eligible scan into shared swapvict,
using V7 sched's largest-sleeper/stopped preference and age-plus-nice ranking.
Current/system/locked/nonresident/locked-text processes cannot be selected.
The MMU excludes failed transfers during an allocation attempt and resets age
on successful swap transitions. Synchronous allocation still cannot wait for
the original background swapper's age gates; young negative-nice processes
remain eligible. Physical allocation and transfer mechanics stay in machine code.

Sticky text now retains its inode and immutable swap image after the last user
exits, releasing its physical memory. Original V7 text locks and xumount lookup
are reused; xrele corrects the original ITEXT precedence error. Failed cache
writes drop the unused entry, and incomplete loads or traced text are not cached.
Release rechecks an empty entry after taking its lock. The swap map is sized from
NPROC+NTEXT+2 (58 entries here) and guarded at compile time, allowing retained-text
allocation holes without overrunning V7's unchecked map insertion routine.

Tests add target-compiled fixtures using the actual shared text/fork/victim
routines, including failing swap writes and transfers, cached reload/release,
locked inodes/text, V7 admission boundaries and root reservation. Guest memory
tests add unprivileged exhaustion and sticky text with/without swap. Map fixtures
exercise the maximum configured fragmented map with an overrun guard.

Validation: all 28 memory scenarios, four tracing layouts/RAM combinations,
four exec combinations, raw-I/O/swapping, six core scenarios plus core policy,
service/accounting/profiling guests and process/text/victim fixtures pass.
Signal, preemption and syscall-ABI suites pass. The 694-file compiler ratchet
passes against reviewed baselines; all 34 kernel C files compile and assemble.
Kernel-only configuration and public-header consistency checks pass. Final kernel
text is 50,220 bytes including the 512-byte entry reserve, data 7,640 and BSS 9,674.
All ten native compiler cases pass, including both executable layouts, optimized
compilation, separate stages, object linking, the standalone optimizer and error
paths. No compiler implementation changes were required.

## Batch 13: separate V7 swapper and sleeping swap I/O

Process 0 now runs the adapted original sched loop. Restored runin/runout
wakeups, time-out/nice selection and the original three-second-out/two-second-
resident gates. swtch selects resident runnable processes only and performs no
allocation or disk transfer. Machine swap I/O owns a serialized bounce buffer
and sleeps at PSWP for interrupt-driven completion; resident processes can run.

Explicit replacement-extent requests go to corework in process 0. Their owners
remain pinned, and proc 0 reserves the actual extent before waking them; failed
transfers are skipped and allocation failure is reported without destroying the
old layout. This differs from PDP-11 expand's self-swap of a resized contiguous
image. Swap-out removes SLOAD before sleeping and restores it on failure.
Swap-in commits residency only after successful reads and releases provisional
frames, including unused newly loaded text, on error. Fork pins its source
through copying and reference updates. XLOCK protects shared text across I/O
and count changes. Locked incoming images receive a timed retry, avoiding a
wait for an unrelated arrival when their lock becomes available.

Testing exposed fast-controller thrashing: immediate completion and accelerated
clock aging allowed repeated swaps before useful resident work. SREADY protects
an image until first dispatch; a post-swap-in runin wait also allows resident IPC
and tracing handoffs to progress. Both safeguards are explicit port additions,
not claims of byte-identical V7 scheduling. The former alone did not resolve the
tracing contention case. Transfer and allocation mechanisms remain machine code.

CMAPSIZ now conservatively covers concurrent sleeping resizes: four committed
plus three provisional extents per process, all NTEXT entries and map headroom
(150 entries, versus 66 previously). The actual V7 map allocator is exercised
against this worst-case fragmented table with an adjacent overrun guard.

The emulator adds optional delayed swap completion IRQs and one-shot Nth read/
write failures. Memory probes use both layouts at 320 KiB and assert that user
mode is observed while a swap IRQ is pending, that the injected error occurred,
and that fork/data isolation and reaping still complete. Target fixtures execute
the actual sched loop through its wait/transfer choices, covering age gates,
locked arrivals, first-dispatch exclusion, failed output and reservation priority.
Exec argument staging is unchanged and remains the next separate batch.

Validation passes: all 32 memory scenarios (including four delayed/error cases),
tracing, exec, six core scenarios, raw I/O, accounting/profiling/lock guests and
all three target policy fixtures. The libc, signals, preemption, TTY, split I/D,
EPU, copy/fault, V7 interfaces, buffer-cache and ABI suites pass. All ten native
compiler cases pass, with optimized split I/D rechecked on the final kernel.
The 694-file compiler ratchet, public-header check and kernel-only build pass.
Final kernel: text 51,912 bytes including the 512-byte entry reserve, data 7,736,
BSS 10,010. No compiler implementation changes were needed.

The final native recheck exposed a host-build configuration issue: the unoptimized
emulator completed successfully in 69.836 seconds and 2,009,955,801 guest cycles,
exceeding the harness's 60-second wall limit. Reconfiguring that host build as
Release passed the unchanged test. Build instructions now specify Release;
no guest cycle limit or assertion was relaxed for this batch.


## 2026-10-06 — V7 exec argument swap staging (batch 14)

Replaced the 5,120-byte global argument buffer and exec lock with the original
V7 collection loop: reserve ten swap blocks, collect through getblk/bawrite,
copy back through bread and release buffers/reservation on every exit. The
register ABI selects u_arg rather than u_ap. V7 total-string/environment counts,
NCARGS-1 limit and null-argv behavior are retained. The existing CPU helper owns
the Z8000 stack address and staged SP, with checked stores and read errors.

Original V7 calls panic("Out of swap") when the fixed reservation cannot be
allocated, even with no arguments. This is retained without a no-swap fallback;
zero or undersized swap therefore prevents init's exec. Memory-pressure probes
use six KiB for the reservation, too little for their process/text images.
Their delayed output-error injection skips early argument writes.

Removing serialization required publishing new shared-text ownership before
corealloc sleeps. The swap map also includes concurrent argument reservations:
SMAPSIZ is now 2*NPROC+NTEXT+2 (74 entries), checked by the guarded fragmentation
fixture. No compiler implementation changes were needed.

Validation: both layouts at 320 KiB and 8 MiB pass credentials/startup and new
argument tests, including 5,119-byte high-bit strings, concurrent different-inode
execs, bad pointers, E2BIG and environment/null argv. The eight-buffer cache
spills the ten-block argument extent; observed swap reads/writes confirm real
backing-device use. Both layouts pass repeated failed exec and successful reuse
with room for only one reservation. Zero/undersized swap panics are verified.
All 32 memory scenarios, ptrace, core, services/policy, raw I/O, libc, signals,
preemption, TTY, split I/D, EPU, copy/fault, V7 interfaces, buffer cache and ABI
regressions pass. All ten native compiler cases, the 694-file compiler ratchet,
public-header check and kernel-only build pass. Final kernel: text 52,116 bytes
including the 512-byte entry reserve, data 2,624, BSS 10,074.


## Step 32: Essential V7 Userland

`tools/native-cc/userland.py` extends the native development environment with 26
unchanged original V7 commands: cat, echo, ls, pwd, mkdir, rmdir, ln, cp, mv, rm,
chmod, chown, chgrp, wc, grep, tail, sort, uniq, tee, cmp, date, sleep, sync, kill,
test and ed. Native make invokes native cc for every command. The image includes
source and a makefile under `/usr/src/cmd`, installs commands in `/bin`, and keeps
the compiler, portable-archive ar, make and yacc available for interactive work.
It uses the existing Step 30 native make/ar/yacc as bootstrap tools and rebuilds
portable ar from current sources. Its r-command now treats a nonexistent archive
as having no old members, restoring V7 creation behavior without weakening
malformed-header checks on existing archives.

The source audit covers every top-level unit in the original command tree:
158 units, 762 files, none missing and 728 byte-identical. The differences are
confined to portable ar/make archive handling, pstat, the shell and yacc's memory
configuration. Source preservation is not a claim that all original commands
build or run. The machine-readable inventory is `tests/build/userland/audit.json`.

Original mkdir and date required missing libc entry points for mknod and stime;
the shared kernel already supported these calls. Runtime sort then exposed a
libc bug: direct brk changed the kernel break but not sbrk's private cached value.
Subsequent stdio allocation could shrink sort's workspace. Both interfaces now
share the exact break initialized from the linker end symbol, following original
V7 sbrk.s. No command source was modified to fix this. Failed brk leaves that
value intact; sbrk(0) reads it without a syscall. The port's raw brk(0) query is
retained. mkdir, rmdir and mv install set-user-ID root, with original real-ID
access checks, for the V7 directory link/unlink operations.

After Steps 29–30, with the native compiler build artifacts available:

```
python3 tools/native-cc/userland.py --audit
python3 tools/native-cc/userland.py --setup
```

The script rebuilds the current kernel and host driver before preparing the disk.
The native compiler regression harness now does so too, avoiding stale kernel
images in its separate build directory. Running userland.py without options
resumes completed steps; `--limit N` limits additional steps. `--setup` starts a
fresh image. `--reuse-commands` recreates the test image while retaining the 26
compiled commands only when their sources, libc and startup object match; it
reruns installation and integration tests and discards other guest changes.
Artifacts are under `tests/build/userland`: the bootable `hd.img`,
staged source/makefiles, per-step logs, `results.json`, `audit.json` and the final
`summary.json` containing installed executable sizes and source hashes.

The integration suite checks file/directory operations, permissions, links,
listing, sorting/uniquing through a pipeline, tail/wc/grep/tee, scripted ed,
signals, date and deliberate command failures. A guest project builds C objects
with make, creates a portable archive with ar, links and runs the result,
generates and runs a yacc parser, then runs a no-change make. In both 0407 and
0411 layouts, its syscall probe alternates brk/sbrk, tests a failed break change, checks stime's high/low word order and denial for an ordinary user, and
checks device-node creation and denial. Ordinary-user mkdir/mv/rmdir and denial
in a protected parent test the installation modes and original access policy.

The background-command test exposed a missing `/dev/null`. The selected memory
driver now provides V7's minor-2 EOF/rathole behavior at character 4,2; other
memory minors return ENXIO. The basic and native disk builders install the node.
The syscall probe checks EOF, discarded writes and denial of physical-memory
minor 0. Shell tests account for V7's errexit and wait behavior instead of
assuming modern shell semantics.

This remains a development disk with the small console init. Original multiuser
init/getty/login, the remaining command set, and Z8000 object-inspection tools
such as nm/strip are not supplied by this batch. Ed encryption's external helper
is also outside the tested editor workflow.


Validation completes all 29 userland steps: 26 native command builds, installation
(including a fresh native ar), command integration, and the native project plus
both syscall-probe layouts. Portable archive tests pass fresh `ar r` creation,
malformed headers, host interoperability, odd/large members, mutation, linking
and make archive dependencies. Libc and ABI suites, all 32 memory scenarios,
services/policy and signal tests, and all ten native compiler cases pass. The
basic boot test also passes with the null-device node installed.

The 695-file compiler regression baseline check passes, with all 35 kernel C
files compiling and assembling. Public-header consistency and the kernel-only
build pass. Final kernel text/data/BSS: 52,228/2,640/10,074 bytes.
