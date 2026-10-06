# Kernel Technical Reference

Detailed technical notes for the Z8001 kernel trap infrastructure.

Machine selection and the CPU/MMU interface are described in
[kernel configuration](../v7z8000/usr/sys/conf/README.md).

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

The emulated MMU provides 128 segments x 32 pages x 2KB pages, with only ROM, kernel and EPU banks identity-mapped on construction. User banks start unmapped. UPAGE and WPAGE remap page pairs within segment 1; IMAP selects a separate instruction bank for each logical segment:

### MMU Control Ports

| Port | Width | Name | Function |
|------|-------|------|----------|
| 0x00B0 | word | UPAGE | KDSA6 equivalent: sets seg1 pages 30-31 to frame pair (value, value+1) |
| 0x00B2 | word, read-only | SWAPSIZE | Dedicated swap unit size in 512-byte blocks |
| 0x00B4 | word | WPAGE | Copy window: sets seg1 pages 28-29 to frame pair (value, value+1) |
| 0x00B8 | word | IMAP | High byte: logical segment; low byte: instruction backing segment (7 bits each) |
| 0x00BA | word, read-only | RAMSIZE | Installed low RAM in 2 KB frames (at most 4096) |
| 0x00BC | word | PAGESEL | Select page-table entry: `(segment << 5) | page` |
| 0x00BE | word | PAGEFRAME | Set physical frame plus RO/SYS flags; `0xffff` unmaps it |

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

Process 0's u-area is at frame 62 (identity-mapped). Other processes allocate
u-areas and user banks from the resource map starting at physical frame 96.

### Physical memory sizing and resource maps

`mmuinit()` reads RAMSIZE and seeds V7's `coremap` with frames from 96 up to
`min(physmem, 4064)`, exclusive. Only the first 192 KiB (ROM, kernel data, kernel instructions) and
the dedicated EPU service bank are reserved. The emulator retains the EPU
bank when less low RAM is installed; absent physical accesses raise SEGTRAP.

`sys/malloc.c` retains V7 first-fit allocation and adjacent-range coalescing;
only its unit comment differs. `h/map.h` is unchanged. Core-map units are
2 KB frames; process accounting uses 64-byte clicks. `USIZE=64` represents
the 4 KB u-area/system stack. `p_addr` names its two-frame allocation;
`p_size` includes that allocation and private data/stack pages; shared text is
accounted separately. For a nonresident process, `p_addr` is its swap block.
`swapmap` is seeded from the dedicated swap device, excluding block zero; `maxmem=MAXMEM` is the I+D bank limit in accounting clicks.

Machine-layer `newmem()` allocates a u-area and copies the parent's section
sizes. User text, data and stack are separate contiguous physical extents,
rounded to 2 KiB pages. Logical segment numbers remain tied to process slots;
PAGESEL/PAGEFRAME map data from address zero and stack at the top of data space.
Split text gets a read-only mapping shared by processes executing the same inode. Unused pages, including the heap/stack gap,
are unmapped. Fork copies private sections and takes a shared-text reference. Failure rolls back all
provisional storage before publishing the child; exit releases every extent.

`estabur(nt, nd, ns, sep, xrw)` validates 64-byte click sizes and page-rounded
space limits, then acquires all replacement extents before committing mappings
and u-area accounting. Exec uses it before destroying the old image, so ENOMEM
preserves that image. `expand(total_clicks)` resizes data with text, stack and
u-area sizes fixed; `sbreak()` uses it for real heap allocation and release.
`brk(0)` retains the port's click-rounded query interface. New pages are cleared;
regrowth also clears the newly exposed portion of a retained partial page.
`p_size` accounts for the u-area and actual allocated pages.

Growth relocates a section to a larger contiguous extent while preserving its
contents. Shrink releases a data/text suffix or a stack prefix. This is V7-style
contiguous-section allocation at page granularity, not arbitrary scattered
physical-page allocation. Fragmentation or the temporary replacement allocation
can cause ENOMEM even when the final image alone would fit. CMAPSIZ covers four
extents per user process, three provisional replacements and the map terminator.
Only process context performs allocation and remapping.

The initial stack reserves at least `SSIZE=64` clicks (4 KiB), including startup
arguments. Exec enlarges this reservation if arguments plus 256 bytes of spare
stack require more, then rounds it to pages. Stack write warnings extend it
before overflow; supported failed stores can also trigger growth with software
backout. Other user gaps raise SIGSEGV; covered kernel user-copy faults return
EFAULT. `useracc()` checks every data-space page in the requested range.
Private combined-space text/data remain writable, as required for 0407.

The Bourne shell now explicitly reserves heap workspace before stores. Its
original SIGSEGV-driven break extension required restarting a failed store.
Shell word construction, expansions, environment construction and here-documents
use bounds checks; failed break requests preserve its previous break pointer.
Other user programs must request heap memory before using it.

`test-memory` checks the target allocator and sizing helpers, host MMU bounds,
partial fork and resize rollback, low-RAM exhaustion/reaping/reuse, failed exec,
layout transitions, heap zeroing/isolation/reclamation, gap EFAULT/SIGSEGV and
large shell workspace. The low-RAM workloads run at 256, 258 and 320 KiB in both
layouts. The driver reports unmapped accesses separately from accesses to absent
physical RAM; valid mappings must never reach absent RAM.

### Separate Instruction and Data Spaces

`ldz8 -i` emits V7 magic 0411: text starts at instruction address zero and
initialized data starts at data address zero. Ordinary 0407 programs retain a
combined layout. Both use 16-bit pointers and NONSEG execution. Each space has
64 KB of addresses; the a.out header limits an individual section to 65,535
bytes, and executable text must have even length. Data, BSS, heap, arguments,
and stack share the data space. The exec loader reserves a mapped stack of at least 4 KiB, enlarged when
startup arguments plus 256 bytes require more. Heap and stack pages must not
overlap; the stack does not grow automatically.

For process slot `i`, the logical/data segment is `S=i+1`. A split process uses
backing segment `S+NPROC` for instructions. `u.u_sep` records the layout;
`sureg()` selects the instruction bank and sets `useg`/`iseg` for kernel copies.
The emulator routes instruction fetches and PC-relative program accesses
through this selection, while data and stack accesses retain their original
mapping. The kernel stays combined. Fork copies the mapped text, data and stack sections;
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

### Shared user-copy policy and machine-helper contract

`passc()`, `cpass()` and `iomove()` now follow V7 transfer policy. `u_segflg`
selects user data (0), kernel memory (1), or user instructions (2). Aligned,
even-length user transfers use the corresponding bulk helper; other transfers
use the byte routines, including kernel-memory transfers. `iomove(..., 0, ...)`
has no effect. Callers supply a nonnegative length no greater than `u_count`;
`passc()` requires a nonzero remaining count.

The replaceable machine layer must implement these return conventions:

| Helpers | Success | Failure |
|---|---|---|
| `fubyte`, `fuibyte` | Unsigned byte, 0–255 | Negative value |
| `subyte`, `suibyte` | Zero | Negative value |
| `copyin`, `copyout`, `copyiin`, `copyiout` | Zero after the whole transfer | Nonzero |

A failed byte access sets `EFAULT` without advancing `u_base`, `u_count` or
`u_offset` for that byte. Earlier successful bytes remain accounted. A failed
bulk operation sets `EFAULT` without advancing any of those fields for that
operation. Its destination may already contain a copied prefix: the V7 bulk
interface has no residual count and does not promise rollback. Successful
operations update the three fields exactly once. `passc()` also returns -1
when the last requested byte succeeds; `u_error` distinguishes failure.

`machine/krt.s` now rejects bulk ranges crossing the 64 KB boundary and odd
word addresses (including a word at `0xffff`). Zero-length bulk copies do not
access memory; a final byte at `0xffff` remains valid. `rdwr()` calls the selected
MMU's `useracc()` before starting a read/write, preventing a long request from
wrapping across multiple buffer-cache blocks or byte transfers.

The current MMU maps private data/stack read/write and shared text read-only and leaves unused pages
unmapped. `useracc()` checks address wrap and every covered data-space page,
including a request spanning the heap/stack gap. A null pointer is not inherently
unmapped when data starts at zero. Future protected MMUs must also check access
permissions.

The SEGTRAP vector now enters the runtime at `0x020a`, saves the same registers
as syscall entry, and calls `segtrap()`. For a kernel fault, `ufixups` recognizes
only the saved PCs immediately following the ten user-access instructions.
Z8001 SEGT is accepted after the instruction, as specified in the CPU manual
(section 7.3.4 and the interrupt transaction description). The handler redirects
IRET to the matching recovery label. That label restores the helper caller's
FCW, including interrupt enables, and unwinds the helper frame with return -1.
No global recovery pointer or shared continuation is needed. Kernel faults
outside those sites panic; user faults use the stack-growth checks above or normal SIGSEGV delivery.

Exec argument-vector faults now return `EFAULT`; a fault after exec has replaced
the old image takes the existing fatal-image path. Signal-frame store failures
terminate with SIGSEGV instead of returning into an incomplete frame. EPU-state
restoration copies into a temporary buffer first, so a failed copy cannot leave
partially restored state.

This mechanism requires a functioning system stack and MMU hardware that
suppresses invalid accesses and raises SEGT. It does not recover arbitrary
kernel bugs, supply demand paging, or provide general instruction restart.

`test-copy` compiles the actual three shared functions with the real target
headers and user structure, relocating `u` into the test program and replacing
only machine helpers. It runs 655 cases in each executable layout: all three
spaces, both directions, odd/even addresses and lengths, zero count, high-bit
bytes, 32-bit offset carry, injected failures and an address-space boundary.
Failures are synthetic; existing boot, libc, TTY and split-I/D suites exercise
the kernel with the actual machine helpers.

`test-fault` adds 30 guest scenarios across combined and split I/D layouts.
They test range rejection, zero-length and last-byte accesses, actual bus faults
in byte/bulk copies, mid-transfer accounting, pathnames, exec vectors, EPU
restoration, signal stacks, split-text loading and direct user accesses.
Each recovery case checks subsequent syscalls and clock progress. The emulator
option `-F r:hex`, `-F w:hex` or `-F u:hex` denies an access at that offset after
the `-w` marker: r/w target kernel segmented user accesses; u targets user-mode
accesses. The bus suppresses the access and requests the CPU's real SEGTRAP
path. This is test-only fault injection, not a new production MMU permission
register. Ordinary runs have no injected denied addresses.

### Library Archives

`ldz8`, native `ar` and native `make` use portable ASCII archives with the
eight-byte `!<arch>\n` signature and 60-byte member headers. Native libraries
use unindexed members with names of at most 14 characters. The archive
container does not determine CPU addressing mode: object headers and
relocations, followed by the linker and loader, determine that. Current
executable support is NONSEG 0407 combined space and 0411 separate I/D;
full segmented executables require further toolchain and loader work.

### Kernel split I/D layout

The kernel is also linked as 0411, with 16-bit pointers. Its instruction space
uses logical segment 1, backing map 126 and physical bank 2 (frames 64–95).
Data starts at logical `1:0000`, backed by bank 1; the copy window and u-area
remain at `e000` and `f000`. ROM programs the I map before entering the kernel.
`krt.s` reserves the first 512 instruction bytes for the separately assembled
trap stubs; `handler.bin` begins at physical `2:0200`, and `handler-data.bin`
is loaded at physical `1:0000`. The PSA copy lives in ROM at `0:1000` because
vector fetches use the data bus. Kernel instruction storage counts against
installed RAM. Images and the emulator must be rebuilt together.

### Stack faults and protection

`machine/mmu.h` defines the board contract. PAGEFRAME bits 15 and 14 mean
read-only and system-only; `ffff` remains unmapped. Read-only applies to system
writes too. Kernel/EPU maps are system-only; shared 0411 text is read-only.
Normal processes remain NONSEG, so their ordinary accesses select their own
logical segment. This does not add a segmented user ABI.

The MMU observes first-word instruction fetch (external status 1101) and latches
that logical PC, the fault segment, low/high access offsets and reasons. Ports
`c0/c2/c4/c6/c8/ca` expose those six words; `cc` acknowledges them. Reason bits
are VALID=1, WRITE=2, READ=4, FETCH=8, UNMAP=16, PROT=32, WARN=64, MIXED=128.
`d0/d2` select a logical segment and its stack-warning base; `ffff` disables it.
A normal-mode store into the lowest 256 bytes of the mapped stack succeeds and
raises a warning. Growth is attempted when SP approaches that boundary; a warning
never turns an otherwise valid store into SIGSEGV. Invalid or protected accesses are suppressed and raise SEGT.
Different instructions/segments in one unacknowledged report set MIXED.

Z8001 SEGT occurs after instruction completion. It is **not** Z8003/4 ABORT.
`stackfault()` accepts warnings without replay, or grows and retries a small
whitelist of nonsegmented instructions: LD/LDB/LDL stores, LDM stores, CALL/CALR,
and register/immediate PUSH through R15. PUSH sources containing R15 are rejected.
CALL/PUSH restore the implicit SP decrement before replay. Failed reads, fetches,
read-modify-write operations, mixed reports, protection faults and unsupported
instructions receive SIGSEGV. Growth also checks that the fault lies at or above
the actual SP in the stack gap; arbitrary heap faults do not allocate memory.
Signal delivery reserves stack space before building its frame. This is safe
software backout for supported cases, not general demand paging or CPU rollback.

### Shared text and swapping

`sys/text.c` owns inode-backed text references, resident counts and immutable
swap copies. The original V7 `struct text` is retained, including 64-byte click
units for `x_size`; physical allocation rounds it to 2 KiB pages. Exec prepares the shared 0411 text before replacing the old layout;
fork shares it; exit/exec drop references. ITEXT and inode references prevent
writes while executable text is in use, including swapped-out users. Executing
an inode already open for writing returns ETXTBSY. The last reference releases
RAM, swap and the inode; there is no idle/sticky text cache. Exec serializes its
shared argument buffer across sleeping disk reads.

The board supplies a separate ATA unit (major 1, minor 1) for swap; root remains
unit 0. Port `b2` reports its size in 512-byte blocks. `corealloc()` evicts other
unlocked resident processes on allocation pressure. Private u-area/data/stack
images are written completely before their frames are released. The last
resident text reference writes its immutable swap copy and releases text RAM.
The scheduler loads runnable nonresident processes before resuming their saved
continuations; it rechecks the run queue after device interrupts during I/O.
If a second fork image cannot fit, the saved child continuation and private
sections are written directly to swap. Exhausted swap causes allocation failure,
not a root-disk write or a kernel panic.

Physical transfer uses a private 512-byte bounce buffer and the configured block
driver. Copy-window masking is bounded to 16-byte transfers. Swap waits enable
interrupts without sleeping on process 0's scheduler stack. This is whole-process
swapping, not demand paging; allocation and victim selection are synchronous.
Contiguous-section fragmentation and temporary resize reservations can still
cause ENOMEM. Raw `physio`, bus-map ownership, core dumps and ptrace remain absent.

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

At return to user mode, `psig(usp)` constructs this 104-byte user frame and
redirects the saved PC to the registered trampoline:

| Offset from new user SP | Value |
|-------------------------|-------|
| 0 | Signal number |
| 2 | Interrupted FCW |
| 4 | Interrupted R0 |
| 6 | Interrupted PC offset |
| 8–103 | Saved EPU registers and control state (96 bytes) |

The returned SP remains on the process's kernel stack through scheduling;
`userret()` installs it in NSPOFF only at final return. Delivery rejects an
odd SP or insufficient space above the data area for the frame and trampoline
entry, terminating with SIGSEGV rather than wrapping the user stack.

The trampoline saves R1-R14, calls the C handler with the signal number,
restores EPU state through syscall 62, restores the registers, and uses unprivileged `LDCTLB FLAGS,rl0` to restore
condition flags. It then pops R0 and returns to the interrupted PC, restoring
the original SP. No privileged FCW bits are loaded from user memory, and no
CPU-context signal-return syscall is needed; syscall 62 restores only EPU
state. A handler may instead use `longjmp`.

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

The syscall table preserves V7 numbering for these interfaces:

| Number | Interface | Arguments |
|---:|---|---|
| 11 | `exec` | pathname, argv; always an empty environment |
| 52 | `sysphys` | unimplemented (`ENOSYS`); no longer EPU restore |
| 59 | `exece` / libc `execve` | pathname, argv, envp |
| 60 | `umask` | creation mask |
| 61 | `chroot` | pathname |
| 62 | Z8000 EPU restore extension | saved 96-byte EPU state |

`execv()` and `execl()` call `execve()` with `environ`. Both successful exec
syscalls update the saved user stack pointer before trap return. The boot
icode keeps using two-argument syscall 11. Sysent argument counts describe
16-bit words in registers here; the dispatcher copies R1–R5 directly rather
than decoding PDP-11 inline arguments.

This migration breaks compatibility with earlier port binaries using the old
execve, umask, chroot or caught-signal trampoline slots. Rebuild both libc
archives, relink programs, and regenerate boot/test/native disks with the
matching kernel. There are no legacy slot aliases: the old numbers conflict
with the restored interfaces. Rebuild the native environment from the repository
root (with the cross-toolchain available):

```sh
python3 tools/native-cc/build.py
cmake -S v7z8000/usr/sys -B v7z8000/usr/sys/build
cmake --build v7z8000/usr/sys/build --target kernel test_driver
cmake -S v7z8000/usr/sys -B tests/build/selfhost/host -DCMAKE_BUILD_TYPE=Release
cmake --build tests/build/selfhost/host --target test_driver
python3 tools/native-cc/selfhost.py --setup
python3 tools/native-cc/environment.py --setup
```

Rebuild the separate host driver as well: the native scripts prefer it when
present, and its profiler must recognize syscall 59. Refreshing an old disk is
insufficient. `test-abi` verifies the raw slots and libc interfaces in combined
and split I/D executables, including inherited/empty environments, file modes,
child-only root changes and the now-reserved slot 52. It also verifies that
process-table exhaustion returns EAGAIN and that slots can be reused afterward.
The configured limit is 16 processes: eight cannot accommodate recursive make,
its command shells and the compiler passes. This adds 224 bytes to kernel BSS.
The emulator profiler observes both exec syscall numbers.

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

Assembly functions called from C are defined with the underscore (`_save`, `_resume`, `_spl0`, ...) and must return values in R0. `save()` in `machine/krt.s` and `resume()` in `machine/pagert.s` preserve all callee-saved registers, the caller's R13 (FP), and the return address in `label_t`.

Note: Steps 1-10 used ACK which has the same R13 frame pointer convention. PCC was changed from R14 to R13 for Z8001 segmented mode compatibility (RR14 is the system stack pointer in SEG mode).

PCC now emits `ld r8,#frame_size; call csv` and `jp cret` directly. The shared
helpers in `PCC-z8000/z8000/lib/csv.az8` preserve the existing frame layout and
R4–R7, R10–R12, R14 and R13, leaving R0–R3 return values untouched. R8/R9 are
call-clobbered scratch registers. Unix libc archives include the helpers;
the kernel links its own copy. No assembly postprocessor is required for
these entry/return sequences.

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

## Shared TTY ioctl contract

`ttioccomm()` implements the V7 handled/unhandled interface: return 1 for a
recognized request (with `u_error` if it failed), or return 0 so the driver can
try a device-specific command. `consioctl()` sets `ENOTTY` for this latter case.
The configured ordinary line discipline is zero; GETD/SETD are supported,
unconfigured disciplines return `ENXIO`, and DIOCGETP/DIOCSETP route to the
discipline's ioctl callback (the ordinary discipline rejects them with ENODEV).
Parameter updates validate the user copy before flushing or changing state.
Both parameter and special-character commits use interrupt masking.

`test-v7-interfaces` tests the shared contract on the target ABI, including
failure preservation and alternate callback routing through substitute test
disciplines. `test-tty` exercises the real terminal syscall path and input modes.
The same interface suite tests restored filesystem call sites, including
lookup and creation beyond 64 KB directory offsets and disabled multiplexor
syscall behavior. The real multiplexor remains unconfigured.

## Buffer cache and asynchronous disk requests

`sys/bio.c` again uses V7's ordinary cache implementation, word-based `clrbuf`
and `DISKMON` accounting. `io_info.nbuf` is initialized to NBUF; `nread`,
`nreada` and `nwrite` count submitted operations, `ncache` counts `bread()`
cache hits, and `bufcount[]` records the free-list position of reused buffers.
These are diagnostic counters, not completion/durability statistics.

The ordinary cache still excludes `physio()` and `B_MAP` release.
Whole-process swap uses its own machine-layer buffer and block-driver transfers. Current drivers do not create physical or bus-mapped requests. Those
facilities require a machine/device transfer contract; no no-op `mapfree()`
is substituted for real mapping ownership.

The HD driver now queues busy buffers through `av_forw`, headed by
`hdtab.b_actf/b_actl`, with one controller request active at a time. This is
required by V7 `bflush()`: it can submit multiple asynchronous writes while
interrupts are masked. A single active pointer previously let later requests
overwrite earlier ones. Completion removes the head before `iodone()` can
release/reuse its list link, then starts the next request. Failed reads do not
copy controller data into the buffer, and errors do not strand later requests.
Controller-busy and read-not-ready interrupts leave the request pending.

`test-bio` compiles the actual cache, HD driver and `binit()` for the target ABI
with a deferred-completion controller and injectable read/write errors. It
checks cache hits/counters, delayed writes, dirty eviction, read-ahead reuse,
queue order, completion/free-list integrity, buffer clearing, retries and
specific/default error propagation. A separate real-kernel test writes 24
blocks with partial-block updates, calls sync, saves the settled disk image,
and verifies every byte after a fresh boot. As in V7, sync queues delayed writes;
the reboot test waits for completion and does not claim power-loss durability
at the instant sync returns.

### Panic-time flushing remains separate

The port still prints the panic and idles without calling `update()`. Source
review shows that `update()` can call `getblk()`/`bwrite()` and sleep on a busy
buffer or I/O. If the panicking path owns that buffer, completion of a flush
cannot be guaranteed; device/cache corruption is another possible panic cause.
Restoring V7's unconditional `update()` here would risk hiding the panic behind
a deadlock. A future best-effort panic flush needs a separate bounded protocol
that avoids owned buffers and does not depend on normal interrupt completion.
