# Kernel Technical Reference

Detailed technical notes for the Z8001 kernel trap infrastructure.

## PSA Table Layout (Z8001)

Each entry is 8 bytes: reserved(2) + FCW(2) + segmented_PC(4).

The PSAP register points to the PSA base. Vector addresses are `PSA_ADDR() + m_vector_mult * offset` where `m_vector_mult=2` for Z8001:

| PSA Offset | Vector | Purpose |
|------------|--------|---------|
| 0x00 | RST | Reset (unused after boot) |
| 0x08 | EPU | Extended processor unit trap |
| 0x10 | TRAP | Privilege violation |
| 0x18 | SYSCALL | System call (`sc` instruction) |
| 0x20 | SEGTRAP | Segment trap |
| 0x28 | NMI | Non-maskable interrupt |
| 0x30 | NVI | Non-vectored interrupt |
| 0x38 | VI | Vectored interrupt |

### PSAPSEG Register Format

The PSAPSEG control register stores the segment in encoded format: `(seg_num << 8) | 0x8000`. For segment 1, this is `0x8100`. The emulator's `PSA_ADDR()` uses `segmented_addr((m_psapseg << 16) | m_psapoff)` which requires this encoding.

## CPU Mode Transitions

The Z8001 has four mode combinations from two FCW bits:

| F_SEG (0x8000) | F_S_N (0x4000) | Mode | Stack pointer |
|----------------|----------------|------|---------------|
| 1 | 1 | SEG+SYS | RR14 (seg:off) |
| 0 | 1 | NONSEG+SYS | R15 (offset, segment from PC) |
| 1 | 0 | SEG+NORM | RR14 (user seg:off) |
| 0 | 0 | NONSEG+NORM | R15 (user offset) |

Key insight: in NONSEG mode, the CPU maps 16-bit addresses using the current PC's segment (`addr & 0xffff | m_pc & 0x7f0000`). Non-segmented C code running in any segment naturally accesses that segment's memory. The kernel does NOT need to be in segment 0.

### CHANGE_FCW R14/R15 Swap Rules

| Transition | R15 swap? | R14 swap? |
|------------|-----------|-----------|
| SEG+SYS -> NONSEG+SYS | No | Yes (F_SEG changed within SYS) |
| NONSEG+SYS -> SEG+SYS | No | Yes (symmetric) |
| SEG+SYS -> NONSEG+NORM | Yes | Yes |
| NONSEG+NORM -> SEG+SYS | Yes | Yes |

When F_SEG changes within system mode, the CPU swaps R14 with the saved system stack segment register. R15 (stack offset) is unchanged since F_S_N stays set.

## SYSCALL Flow

1. Code executes `sc #N` (N encoded in tag word as `0x7F00 | N`)
2. CPU sets `CHANGE_FCW(old | F_S_N | F_SEG)` -> SEG+SYS mode
3. CPU pushes PC(4) + FCW(2) + tag(2) = 8 bytes onto system stack via *RR14
4. CPU loads new FCW and PC from PSA[SYSCALL] -> jumps to trap stub
5. Trap stub: saves R0-R12, switches to NONSEG+SYS with VIE and NVIE enabled
6. Trap stub: extracts syscall number from tag word, calls C handler
7. C handler writes results into saved R0/R1 and handles signals/rescheduling
8. Trap stub: switches back to SEG+SYS, restores registers (R0 gets return value), IRET
9. IRET pops tag(2) + FCW(2) + PC(4), CHANGE_FCW restores original mode

### Stack Layout After Register Save

```
SP+0:  saved R0    <- regs[0] (return value written here by C handler)
SP+2:  saved R1    <- regs[1] (arg1)
SP+4:  saved R2    <- regs[2] (arg2)
SP+6:  saved R3    <- regs[3] (arg3)
...
SP+24: saved R12   <- regs[12]
SP+26: tag word    <- 0x7F00 | syscall_number
SP+28: saved FCW
SP+30: saved PC high
SP+32: saved PC low
```

## Mixed-Mode Assembly in trap.s

The trap stub is assembled in z8001 (segmented) mode because the trap handler executes in SEG+SYS mode and needs segmented register addressing (`@RR14`). However, the middle section runs in NONSEG+SYS mode after the FCW switch, where base-address (BA) mode instructions have different encodings:

- **z8001 (segmented)**: 6 bytes — opcode(2) + segment(2) + offset(2)
- **z8002 (nonseg)**: 4 bytes — opcode(2) + displacement(2)

The z8k-coff-as assembler provides `.unsegm` and `.segm` directives to switch encoding mode within a single file. The NONSEG section of trap.s uses `.unsegm` so that instructions like `ld r0, 26(r15)` get correct 4-byte z8002 encodings, then switches back to `.segm` before the SEG+SYS register restore and IRET.

Instructions using only immediate, register, or indirect-register addressing modes encode identically in both modes and need no special handling.

## Syscall Dispatch

```c
trap(num, regs)
int num;
unsigned *regs;
```

`trap()` in `sys/trap.c` is reached from the SYSCALL stub in `trap.s` through the jump table at the start of `krt.s` (see Entry Points below). It copies the arguments from the saved registers into `u.u_arg[0..4]`, sets `u.u_dirp` to the first one, and dispatches through the V7-style `sysent[]` table (64 entries). A number out of range or with no handler gives `ENOSYS`.

### Syscall Calling Convention

```
sc #N           — syscall number N (encoded in instruction tag word)
R1..R5          — up to five arguments (e.g. fd, buffer, count for write)
R0 = return     — first result (u.u_r.r_val1), or -1 on error
R1 = return     — second result (u.u_r.r_val2), or errno on error
```

`fork` uses the second result: the kernel returns R1 = 1 in the child and 0 in the parent. The user-space stubs in `tools/libc/syscalls.az8` store R1 into `errno` when R0 is -1.

## Entry Points

`trap.s` (assembled with `z8k-coff-as`) holds the PSA and the stubs that the CPU enters in SEG+SYS mode. Each stub saves R0–R12, switches to NONSEG+SYS and calls a fixed address in the jump table at the start of `krt.s`, which is linked at 0x0200:

| Address | Label | Reached from | Calls |
|---------|-------|--------------|-------|
| 0x0200 | `syscall_dispatch` | `syscall_entry` | `_trap` |
| 0x0202 | `boot_entry` | boot code at 0x01F0 | `_main` |
| 0x0204 | `nvi_dispatch` | `nvi_entry` | `_clock` |
| 0x0206 | `vi_dispatch` | `vi_entry` | `_hdintr`, then `_consrint` |
| 0x0208 | `epu_dispatch` | segment 127 EPU entry (SEG call) | `_fptrap` |

All devices share VI vector 0, so `vi_dispatch` calls every device handler and each one checks whether it has work.

## Software EPU Service

PSA offset 0x08 enters segment 127 offset 0 in SEG+SYS mode. EPA remains
disabled in user FCW, so extended instructions trap for software execution.
The arithmetic/decoder is the preserved `fpe/fpe.z8k` from CP/M-8000;
`tools/fpe/translate.py` translates assembler syntax and replaces only the
CP/M entry adapter. The build uses GNU Z8000 binutils, without requiring a
CP/M installation or prebuilt arithmetic objects. `fpe/unix.s` provides Unix
entry/return and instruction/data memory access helpers.

The machine loads `fpe.bin` at physical 0x7f0000. Segment 127 and its physical
frames are reserved. UPAGE maps pages 30/31 of both segment 1 and segment 127
to the current process's u-area/kernel stack. The same stack is therefore
accessible from either nonsegmented PC segment. SEG transfers use the system
stack in segment 1. Arithmetic runs with VI/NVI enabled; interrupt return from
system mode does not schedule. Scheduling/signals occur at the ordinary
`userret` boundary after the engine has completed an instruction.

The EPU entry saves all 16 user registers plus the four-word hardware frame.
The kernel adapter validates instruction formats and passes this frame and
`u.u_fpe`, a 208-byte per-process workspace, to segment 127 offset 0x80.
The original engine receives its state through R9 and workspace through R13.
Instruction fetch uses the process's I backing segment; operand accesses use
its D segment. User memory transfers cannot wrap across the 64 KB boundary.
Invalid instructions signal SIGILL, invalid memory signals SIGSEGV, and enabled
arithmetic exceptions signal SIGFPE. No arithmetic is performed by host code.

Fork copies the workspace with the u-area. Exec clears it; first use selects
affine infinity and round-to-nearest/even. Signal frames preserve the first
96 bytes (eight 80-bit registers and control state). The updated libc
trampoline restores them through syscall 52, then restores only unprivileged
CPU flags and PC. This changes the signal-frame ABI: existing programs that
use caught signals must be relinked with the updated libc. All repository
test images and native compiler binaries are rebuilt with it.

The exposed subset includes the arithmetic, comparisons and transfers needed
by PCC, square root, absolute value/negation, and flags/user control transfers.
The gate rejects reserved instructions and unsupported operations, including
the upstream defective FINT, BCD and partial-remainder operations. The original
engine's numerical verification and remaining IEEE differences are recorded
in `fpe/VERIFICATION.md`: subnormal double rounding and exception-flag behavior
are not claimed to be strictly IEEE compliant. PCC glue preserves the prior
NaN conventions for addition/subtraction, multiplication/division, and format
conversion, without implementing finite arithmetic itself.

`tools/fpe/glue.c` and generated `epu.az8` replace the private C arithmetic
engine in Unix `libv7.a`; PCC's existing `float.az8` calling convention remains.
The standalone compiler CPU tests retain `PCC-z8000/z8000/lib/softfp.c`, because
their machine has no Unix service. Kernel C now uses the existing `c2z8.py`
compaction pass and shared csv/cret. `bout2bin.py` rejects a kernel whose
text/data/BSS reaches the MMU copy window at 0xe000.

Run `cmake --build build --target test-fpe` to test arithmetic vectors,
integer/format conversions, I/D memory operands, fork inheritance, exec reset,
concurrent arithmetic, signal preservation, and invalid-instruction/memory
and arithmetic-exception delivery in both 0407 and 0411 programs.

## Interrupt Levels

The Z8000 has two interrupt enables in the FCW where the PDP-11 has priority levels: VIE (0x1000) for devices and NVIE (0x0800) for the clock.

| Routine | PDP-11 meaning | Here |
|---------|----------------|------|
| `spl0`, `spl1` | everything allowed | set VIE and NVIE |
| `spl4`, `spl5` | devices blocked, clock allowed | clear VIE, set NVIE |
| `spl6`, `spl7` | everything blocked | clear VIE and NVIE |
| `splx(s)` | restore | copy VIE and NVIE from `s` |

All return the previous FCW for `splx`. `spl5` allows clock interrupts even
inside device handlers, but keeps VIE clear to prevent device reentry.
NVI enters C with both enables clear; VI enters C with NVIE set. Clock
callouts use `spl5()` to permit nested ticks. `BASEPRI()` tests the saved
FCW and defers nested callouts whenever either enable was clear.

Syscalls enter C with both enables set. User-memory helpers preserve the
caller's enables, and fork restores the MMU copy window between short,
masked chunks. User mode starts with both enables set.

`userret()` handles signals and scheduling for syscall exits and interrupts
returning to user mode. It preserves NSPOFF on the process's kernel stack
across a switch, rechecks pending work, and masks the final return through
IRET. Interrupts of kernel code never schedule directly. `trap()` saves
`u_qsav` so signals can unwind interruptible sleeps. See
[interrupt-masking.md](interrupt-masking.md) for tests and measurements.

`resume()` also masks both from the moment it remaps the u-area until it has restored SP: in between, the stack pages already belong to the new process while SP is still the old one.

Each process has a normal/user stack and a system/kernel stack. The latter
preserves suspended kernel calls when the process sleeps. An interrupt from
kernel mode uses that current system stack; there is no independent timer
stack selected automatically. Even a counter-only handler therefore needs
the stack-switch interval masked, because CPU entry saves its frame before
executing the handler.

Masking delays a pending timer request; it does not itself lose a tick.
Loss occurs when another pulse arrives while the request latch is already
set. Keeping fully masked regions brief prevents accumulation in the tested
workloads. Ten nominal timer hours each of idle and two busy workloads showed
zero post-boot merges; see the measurement method and limits in
[interrupt-masking.md](interrupt-masking.md#measuring-sustained-clock-delivery).

## Boot Flow (V7 Kernel)

```
ROM reset → seg0:0x0010 (init)
  → set PSAP to seg1:0x0000, system stack RR14 = seg1:0xFFF0
  → set NSP = 0xFFF0
  → IRET to seg1:0x01F0 (NONSEG+SYS)

seg1:0x01F0 (trap.s boot entry):
  → call 0x0202

seg1:0x0202 (krt.s boot_entry):
  → zero BSS (_edata.._end)
  → ld sp, #0xFFFE    (kernel stack at top of u-area page)
  → FCW = 0x5000      (NONSEG+SYS, devices enabled, clock not yet)
  → calr _main

main() (sys/main.c):
  → proc[0] setup: p_stat=SRUN, p_flag=SLOAD|SSYS, p_addr=62
  → u.u_procp = &proc[0], u.u_error = 0
  → rootdev = makedev(1, 0)   — the IDE hard disk; also pipedev, swapdev
  → printf("boot\n")
  → clkstart()      — enable the clock
  → cinit()         — clist free list
  → binit()         — init 8-buffer cache, count block devices
  → iinit()         — open block device, bread superblock, mount root
  → iget(ROOTINO)   — load root inode
  → namei("/dev/console") — walk root→dev→console via bread/bmap/iget
  → open1()         — falloc(), openi(), cdevsw[0].d_open()
  → dup fd 0 → fd 1, fd 2
  → printf("Z8000 Unix\n")
  → newproc()       — fork process 1
    → child: copyout(icode) → return → krt.s → retu() → user mode
    → parent: swtch() → resumes child → child runs icode
  → icode: exec("/etc/init") → init execs /bin/sh
```

## Paged MMU

The emulated MMU provides 128 segments x 32 pages x 2KB pages, identity-mapped on construction. UPAGE and WPAGE remap page pairs within segment 1; IMAP selects a separate instruction bank for each logical segment:

### MMU Control Ports

| Port | Width | Name | Function |
|------|-------|------|----------|
| 0x00B0 | word | UPAGE | KDSA6 equivalent: sets seg1 pages 30-31 to frame pair (value, value+1) |
| 0x00B4 | word | WPAGE | Copy window: sets seg1 pages 28-29 to frame pair (value, value+1) |
| 0x00B8 | word | IMAP | High byte: logical segment; low byte: instruction backing segment (7 bits each) |

### Address Translation

```
segment = (addr >> 16) & 0x7F
if program_access: segment = instruction_bank[segment]
page    = (addr & 0xFFFF) >> 11    // 5 bits → 32 pages
pg_off  = addr & 0x7FF             // 11 bits → 2048 bytes per page
frame   = pages[segment][page]
physical = (frame << 11) | pg_off
```

### u-area Mapping

The u-area occupies virtual 0xF000-0xFFFF (4KB = pages 30-31 of segment 1). The kernel stack grows down from 0xFFFE within these pages. `resume()` writes UPAGE to remap these two pages to the target process's physical frames, swapping the entire u-area + kernel stack with a single I/O port write.

Process 0's u-area is at frame 62 (identity-mapped: seg1 page 30 = frame 62). Forked processes get frames from `frame_alloc()` starting at frame `(2*NPROC+1)*32`, which is 544 with `NPROC` = 8: everything below is reserved for the kernel, user data banks, and user instruction banks.

### Separate Instruction and Data Spaces

`ldz8 -i` emits V7 magic 0411: text starts at instruction address zero and
initialized data starts at data address zero. Ordinary 0407 programs retain a
combined layout. Both use 16-bit pointers and NONSEG execution. Each space has
64 KB of addresses; the a.out header limits an individual section to 65,535
bytes, and executable text must have even length. Data, BSS, heap, arguments,
and stack share the data space. The exec loader reserves at least 256 bytes
beyond the initial arguments for stack growth; this is not a guarantee that a
program's eventual heap and stack will fit.

For process slot `i`, the logical/data segment is `S=i+1`. A split process uses
backing segment `S+NPROC` for instructions. `u.u_sep` records the layout;
`sureg()` selects the instruction bank and sets `useg`/`iseg` for kernel copies.
The emulator routes instruction fetches and PC-relative program accesses
through this selection, while data and stack accesses retain their original
mapping. The kernel stays combined. Fork copies both banks for a split process;
exec can change between layouts. Text is neither shared nor write-protected.
This mapping is implemented in the emulated machine; FPGA hardware still needs
an equivalent instruction/data bus mapping.

`copyiin`/`copyiout` and `fuibyte`/`suibyte` access instruction backing memory.
The loader uses the instruction copy path for text and the data path for data
and BSS. It checks header, entry point, file length, and layout before replacing
the old image. PCC places dense switch tables in data space because generated
indirect loads use the data bus. The linker also rejects overflowing layouts
before truncating header fields or symbol values.

`test-split` runs a program with more than 64 KB of total static storage,
including a function and PC-relative constant above address 0x8000. It covers
BSS, initialized data, switch tables, file I/O, signals, fork isolation, failed
exec, and transitions between combined and split programs. It also runs the
libc and signal suites as 0411 binaries and checks linker overflow rejection.

## RAM Disk DMA

The kernel runs in NONSEG mode with 16-bit pointers (64KB address space). The disk image cannot live in this space alongside the kernel. Instead, the RAM disk driver (`dev/md.c`) uses I/O port-based DMA: it writes a block number and kernel buffer address to I/O ports, and the emulator performs the memory transfer.

### DMA Controller Ports

| Port | R/W | Description |
|------|-----|-------------|
| 0xE0 | W | Block number high byte |
| 0xE1 | W | Block number low byte |
| 0xE2 | W | DMA address high byte (kernel buffer offset) |
| 0xE3 | W | DMA address low byte |
| 0xE4 | W | Command: 1=read block→mem, 2=write mem→block |
| 0xE5 | R | Status: 0=ok, 0xFF=error |

On command write, the emulator immediately copies 512 bytes between the disk image and the memory region at `0x010000 + dma_addr` (segment 1). Reads beyond the end of the disk image return zero-filled blocks.

### Device Switch Tables

```
bdevsw[0] = { mdopen, mdclose, mdstrategy, &mdtab }   — RAM disk
bdevsw[1] = { hdopen, hdclose, hdstrategy, &hdtab }   — IDE hard disk (the root device)
cdevsw[0] = { consopen, consclose, consread, conswrite } — console
cdevsw[1] = the same entry, spare
cdevsw[2] = { consopen, consclose, consread, conswrite } — /dev/tty alias
```

## Caught Signals

The libc `signal()` wrapper keeps a 17-entry table of C handler addresses.
For a caught disposition it registers a shared libc trampoline with syscall
48; default and odd/ignored dispositions are passed through. It translates
the previous kernel disposition back to the previous C handler on return,
and rolls back its table update if registration fails. Fork copies the table
and kernel dispositions; exec resets caught dispositions and preserves ignores.

At return to user mode, `psig(usp)` constructs this eight-byte user frame and
redirects the saved PC to the registered trampoline:

| Offset from new user SP | Value |
|-------------------------|-------|
| 0 | Signal number |
| 2 | Interrupted FCW |
| 4 | Interrupted R0 |
| 6 | Interrupted PC offset |

The returned SP remains on the process's kernel stack through scheduling;
`userret()` installs it in NSPOFF only at final return. Delivery rejects an
odd SP or insufficient space above the data area for the frame and trampoline
entry, terminating with SIGSEGV rather than wrapping the user stack.

The trampoline saves R1-R14, calls the C handler with the signal number,
restores the registers, and uses unprivileged `LDCTLB FLAGS,rl0` to restore
condition flags. It then pops R0 and returns to the interrupted PC, restoring
the original SP. No privileged FCW bits are loaded from user memory, and no
signal-return syscall is needed. A handler may instead use `longjmp`.

As in V7, caught dispositions reset before delivery except SIGILL and
SIGTRAP; SIGKILL cannot be caught or ignored. A caught signal interrupting a
blocking syscall unwinds through `u_qsav`, and the syscall returns EINTR after
the handler returns. This implements neither modern signal masks/alternate
stacks nor ptrace/core dumps, and does not add hardware-exception routing.

`test-signal` covers asynchronous register/flag restoration, handler syscalls,
an interrupted pipe read, nested delivery, one-shot and persistent dispositions,
ignore/error cases, longjmp, fork inheritance, exec reset, and invalid stack
rejection. Its alarm tests use two seconds because V7's next-second rounding
can make a one-second alarm fire before the blocking call starts.

## User Program Startup

`execve()` builds a user stack containing `argc`, the `argv` pointers and
their null terminator, then the environment pointers and their null
terminator. PCC startup in `tools/libc/crt0.az8` clears BSS, sets its
`_environ` global to `&argv[argc+1]`, and calls `main(argc, argv, envp)`.
Returning from main calls C `exit()`, which flushes stdio and calls `_exit()`.
The syscall error handler in `syscalls.az8` owns the common `_errno` symbol;
there is no separate errno archive member or startup initialization helper
in the active libraries.

## PCC Calling Convention (Z8002)

| Aspect | Convention |
|--------|-----------|
| Stack pointer | R15 |
| Frame pointer | R13 |
| Return value | R0 (int/pointer), RR0 (long) |
| Arguments | Pushed right-to-left onto R15 stack |
| Callee-saved | R4-R7, R10-R12, R14 |
| C symbol names | Leading underscore, eight characters in all (`main` → `_main`), as on the PDP-11. Runtime support routines (`lmul`, `ldiv`, `fadd`, ...) have no underscore |
| Function prologue | `push @sp, r13; ld r13, sp; sub sp, #N` |
| Function epilogue | `ld sp, r13; pop r13, @sp; ret` |

Assembly functions called from C are defined with the underscore (`_save`, `_resume`, `_spl0`, ...) and must return values in R0. The `save()`/`resume()` functions in `krt.s` preserve all callee-saved registers, the caller's R13 (FP), and the return address in `label_t`.

Note: Steps 1-10 used ACK which has the same R13 frame pointer convention. PCC was changed from R14 to R13 for Z8001 segmented mode compatibility (RR14 is the system stack pointer in SEG mode).

## Terminal control

The libc stubs pass `ioctl(fd, command, address)` in R1–R3. Kernel `ioctl`
handles `FIOCLEX`/`FIONCLEX` for any valid descriptor, rejects ordinary files
with `ENOTTY` for device commands, and calls the character driver's `d_ioctl`.
The two-argument `stty` and `gtty` calls translate to `TIOCSETP` and `TIOCGETP`.

The console's `consioctl` delegates to `ttioccomm`: settings (`GETP`, `SETP`,
`SETN`), special characters (`GETC`, `SETC`), flush, and tty state flags.
`SETP` drains output and flushes input; `SETN` changes settings without that
flush. Raw output sends all eight bits, including values otherwise used as
output-delay markers. Baud values are stored but do not configure hardware on
the host console. Alternate disciplines and modem commands return `ENOTTY`.
The existing exclusive-open and hangup state flags do not implement physical
modem behavior or enforce exclusive console opens.

Run `test-tty` for settings and interactive mode regressions; see Step 21 in
[implementation-steps.md](implementation-steps.md).
