# Traps and Interrupts

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

The PSAPSEG control register stores the segment in encoded format: `(seg_num << 8) | 0x8000`. The current ROM installs `PSAPSEG=0x8000`, `PSAPOFF=0x1000`: vectors are at data address `0:1000`. The emulator's `PSA_ADDR()` uses `segmented_addr((m_psapseg << 16) | m_psapoff)` which requires this encoding.

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

`trap()` in `machine/trap.c` is reached from the SYSCALL stub in `machine/trap.s` through the jump table at the start of `machine/krt.s` (see Entry Points below). It copies the arguments from the saved registers into `u.u_arg[0..4]`, sets `u.u_dirp` to the first one, and dispatches through the V7-style `sysent[]` table (64 entries). A number out of range or with no handler gives `ENOSYS`.

### Syscall Calling Convention

```
sc #N           — syscall number N (encoded in instruction tag word)
R1..R5          — up to five arguments (e.g. fd, buffer, count for write)
R0 = return     — first result (u.u_r.r_val1), or -1 on error
R1 = return     — second result (u.u_r.r_val2), or errno on error
```

`fork` uses the second result: the kernel returns R1 = 1 in the child and 0 in the parent. The user-space stubs in `tools/libc/syscalls.az8` store R1 into `errno` when R0 is -1.

## Entry Points

`machine/trap.s` (assembled with `z8k-coff-as`) holds the PSA and the stubs that the CPU enters in SEG+SYS mode. Each stub saves R0–R12, switches to NONSEG+SYS and calls a fixed address in the jump table at the start of `machine/krt.s`, which is linked at 0x0200:

| Address | Label | Reached from | Calls |
|---------|-------|--------------|-------|
| 0x0200 | `syscall_dispatch` | `syscall_entry` | `_trap` |
| 0x0202 | `boot_entry` | boot code at 0x01F0 | `_main` |
| 0x0204 | `nvi_dispatch` | `nvi_entry` | `_clock` |
| 0x0206 | `vi_dispatch` | `vi_entry` | configuration `_devintr(vector)` |
| 0x0208 | `epu_dispatch` | segment 127 EPU entry (SEG call) | `_fptrap` |

The emulated configuration shares VI vector 0. Its `devintr()` in
`conf/emulated.c` calls `hdintr()` and `consrint()`; CPU entry code no longer
names individual device handlers.

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
trampoline restores them through syscall 62, then restores only unprivileged
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
their machine has no Unix service. Kernel C now uses the native C `oz8`
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
[interrupt-masking.md](../history/interrupt-masking.md) for tests and measurements.

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
[interrupt-masking.md](../history/interrupt-masking.md#measuring-sustained-clock-delivery).
