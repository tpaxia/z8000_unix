# Kernel Bring-up: PSA Table, Trap Stubs, and Multi-Segment Support

## Overview

Unix V7 kernel bring-up on the Z8001. Implements the trap infrastructure needed for system calls: a PSA (Program Status Area) table, SYSCALL entry/exit stubs, ROM init code, syscall dispatch, and a minimal console TTY driver.

### Progress

#### Step 1: Trap Infrastructure (completed)

Proved the full SYSCALL trap round-trip: ROM init (segment 0) -> IRET to kernel (segment 1, NONSEG+SYS) -> `sc #0` -> trap handler (SEG+SYS) -> register save -> NONSEG+SYS -> set R0=7 -> SEG+SYS -> register restore -> IRET back -> halt with R0=7.

#### Step 2: Syscall Dispatch + Console TTY Driver (completed)

Added syscall number extraction from the SC tag word, argument passing via saved registers, return value plumbing, dispatch by syscall number, and `write(fd, buf, count)` as syscall #4.

The test proves the full write() syscall: `sc #4` with R1=1 (stdout), R2=msg, R3=23 -> trap handler extracts syscall #4 from tag word -> C dispatch calls `cons_write()` -> 23 bytes output to console port -> R0=23 returned to caller via saved register slot.

### Planned Steps

- **Syscall table**: V7-style `sysent[]` table (64 entries, indexed by syscall number)
- **exit() syscall**: process termination
- **Console input**: read from keyboard (requires emulator interrupt support)
- **Line discipline**: echo, erase, kill processing
- **clist buffering**: V7's character block allocator
- **File descriptor table / u-area**: proper fd validation
- **Separate user segments**: copyin/copyout for user memory access

## Architecture

### CPU Mode Transitions

The Z8001 has four mode combinations from two FCW bits:

| F_SEG (0x8000) | F_S_N (0x4000) | Mode | Stack pointer |
|----------------|----------------|------|---------------|
| 1 | 1 | SEG+SYS | RR14 (seg:off) |
| 0 | 1 | NONSEG+SYS | R15 (offset, segment from PC) |
| 1 | 0 | SEG+NORM | RR14 (user seg:off) |
| 0 | 0 | NONSEG+NORM | R15 (user offset) |

Key insight: `adjust_addr_for_nonseg_mode()` maps 16-bit addresses using the current PC's segment (`addr & 0xffff | m_pc & 0x7f0000`). So non-segmented C code running in segment 1 naturally accesses segment 1 memory. The kernel does NOT need to be in segment 0.

### Memory Layout

| Segment | Physical Address | Contents |
|---------|-----------------|----------|
| 0 | 0x000000-0x000FFF | ROM: reset vector + init code |
| 1 | 0x010000-0x01FFFF | Kernel: PSA table, trap stubs, C code, data, stacks |
| 2-8 | 0x020000-0x08FFFF | User processes (future) |

### PSA Table Layout (Z8001)

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

### Syscall Calling Convention

```
SC #N           — syscall number N (encoded in instruction tag word)
R1 = arg1       — e.g., fd for write
R2 = arg2       — e.g., buffer pointer for write
R3 = arg3       — e.g., byte count for write
R0 = return     — bytes written, or -1 on error
```

### SYSCALL Flow

1. User/kernel code executes `sc #N` (N encoded in tag word as `0x7F00 | N`)
2. CPU sets `CHANGE_FCW(old | F_S_N | F_SEG)` -> SEG+SYS mode
   - R14/R15 swapped with m_nspseg/m_nspoff as needed
3. CPU pushes PC(4) + FCW(2) + tag(2) = 8 bytes onto system stack via *RR14
4. CPU loads new FCW and PC from PSA[SYSCALL] -> jumps to trap stub
5. Trap stub: saves R0-R12, switches to NONSEG+SYS
6. Trap stub: extracts syscall number from tag word at SP+26, pushes
   syscall number and regs pointer as arguments, calls C handler at 0x0200
7. Trap stub: writes C handler return value into saved-R0 slot on stack
8. Trap stub: switches back to SEG+SYS, restores registers (R0 gets return value), IRET
9. IRET pops tag(2) + FCW(2) + PC(4), CHANGE_FCW restores original mode

### Stack Layout After Register Save

```
SP+0:  saved R0    <- regs[0] (return value written here by trap stub)
SP+2:  saved R1    <- regs[1] (arg1)
SP+4:  saved R2    <- regs[2] (arg2)
SP+6:  saved R3    <- regs[3] (arg3)
...
SP+24: saved R12   <- regs[12]
SP+26: tag word    <- 0x7F00 | syscall_number
SP+28: saved FCW
SP+30: saved PC low
SP+32: saved PC high
```

### CHANGE_FCW R14/R15 Swap Rules (Z8001)

| Transition | R15 swap? | R14 swap? |
|------------|-----------|-----------|
| SEG+SYS -> NONSEG+SYS | No | Yes (F_SEG changed within SYS) |
| NONSEG+SYS -> SEG+SYS | No | Yes (symmetric) |
| SEG+SYS -> NONSEG+NORM | Yes | Yes |
| NONSEG+NORM -> SEG+SYS | Yes | Yes |

### PSAPSEG Register Format

The PSAPSEG control register stores the segment in encoded format: `(seg_num << 8) | 0x8000`. For segment 1, this is `0x8100`. The emulator's `PSA_ADDR()` uses `segmented_addr((m_psapseg << 16) | m_psapoff)` which requires this encoding.

## Files

| File | Purpose |
|------|---------|
| `kernel/rom.s` | Reset vector + init code (segment 0, assembled z8001 mode) |
| `kernel/trap.s` | PSA table + syscall entry/exit stubs + test code (segment 1, assembled z8001 mode) |
| `kernel/krt.s` | Kernel runtime stub (ACK z8002): entry trampoline, `putc()`, `cons_write()` |
| `kernel/syscall.c` | Syscall dispatch: extracts args from saved regs, dispatches by number |
| `kernel/test_driver.cpp` | Standalone emulator driver, loads ROM + kernel + handler, verifies result |
| `kernel/Makefile` | Build rules for assembly, ACK C, driver, and test |

## Building and Testing

```sh
cd kernel
make test        # build all + run with instruction trace
make all         # build without running
make clean       # remove build artifacts
```

Build outputs:
- `rom.bin` — ROM image loaded at physical 0x000000
- `kernel.bin` — kernel image loaded at physical 0x010000
- `handler.bin` — ACK-compiled C handler loaded at physical 0x010200
- `rom.lst`, `trap.lst` — assembler listing files
- `test_driver` — test executable (links against `z8000_emu/build/libz8000.a`)

The test driver runs with a 10,000 cycle safety limit and verifies:
- R0 == 23 (bytes written by write() syscall)
- Console output == "Hello from Z8000 Unix!\n"

## Boot Sequence Detail

### ROM Init (segment 0, SEG+SYS)

1. Reset vector at 0x000000 sets FCW=0xC000 (SEG+SYS), PC=seg0:0x0010
2. Init code sets RR14 = seg1:0xFFF0 (system stack)
3. Sets PSAP to seg1:0x0000 (PSA table at start of kernel segment)
4. Sets NSP to seg2:0xFFF0 (normal/user stack, for future use)
5. Pushes fake IRET frame: tag=0, FCW=0x4000 (NONSEG+SYS), PC=seg1:0x0100
6. Executes IRET -> jumps to test code in segment 1 in NONSEG+SYS mode

### SYSCALL Handler (segment 1)

Entry in SEG+SYS mode (set by CPU hardware):
1. Saves R0-R12 via `push @rr14, rN` (26 bytes onto system stack)
2. `ldctl fcw, #0x4000` -> NONSEG+SYS (R14 swapped with m_nspseg)
3. Extracts syscall number: `ld r0, 26(r15)` then `and r0, #0xFF`
4. Saves regs pointer (`r1 = r15`), allocates stack space for 2 args
5. Calls C handler entry at 0x0200 (krt.s trampoline -> `_syscall_handler`)
6. Writes R0 (return value) into saved-R0 slot: `ld 0(r15), r0`
7. `ldctl fcw, #0xC000` -> SEG+SYS (R14 swapped back)
8. Restores R0-R12 via `pop rN, @rr14` (R0 gets return value from slot)
9. IRET -> pops tag, FCW, PC, restores caller mode

### C Syscall Dispatch (`syscall.c`)

```c
int syscall_handler(int num, int *regs)
```

Dispatches by syscall number via switch. Currently implements:
- **syscall 4 (write)**: `regs[1]`=fd, `regs[2]`=buf, `regs[3]`=count.
  Calls `cons_write(buf, count)` for fd 1 (stdout) or fd 2 (stderr).

All code runs in NONSEG+SYS mode within segment 1. Buffer pointers from
user registers are 16-bit offsets within segment 1, directly dereferenceable.
No copyin/copyout needed until separate user segments are implemented.

## ACK Compiler Workarounds

Two bugs in the ACK z8000 code generator required workarounds:

### 1. `char *` dereference generates segmented addressing

The ACK C compiler with `-z8002` generates segmented `@RR2` addressing for
byte pointer dereferences: it puts the address in R3 (offset) and sets R2=0
(segment), then executes `ldb rl0, @r2`. In z8002 NONSEG mode, `@r2` uses
only R2 (=0) as the address — R3 is ignored, causing reads from address 0.

**Workaround**: `cons_write()` is written in assembly (in `krt.s`) instead
of C, using direct `ldb rl1, 0(r2)` + `outb` to avoid the buggy code path.

### 2. `unsigned *` to `int` triggers broken `cii` conversion

Accessing `unsigned *regs` elements and assigning to `int` locals generates
calls to the ACK EM `cii` (convert integer to integer) runtime function.
The `cii` implementation consumes the value from the stack without pushing
a result back for same-size conversions, leaving the caller to pop garbage.

**Workaround**: `syscall_handler()` uses `int *regs` instead of
`unsigned *regs` to avoid the type conversion entirely.

## z8001/z8002 Instruction Encoding Mismatch

The trap stub (`trap.s`) is assembled in z8001 (segmented) mode but executes
part of its code in NONSEG+SYS mode after the FCW switch. Base-address (BA)
mode instructions like `ld r0, 26(r15)` have different encodings:

- **z8001 (segmented)**: 6 bytes — opcode(2) + segment(2) + offset(2)
- **z8002 (nonseg)**: 4 bytes — opcode(2) + displacement(2)

The z8001 assembler generates 6-byte X-mode (indexed) encodings. When the
CPU runs in NONSEG mode, it reads only 4 bytes, interpreting the segment
word as the displacement (always 0) and leaving the offset as a stray
instruction.

**Workaround**: BA-mode instructions in the NONSEG section are encoded
manually as `.word` directives with z8002-compatible 4-byte encodings:

```asm
.word   0x61F0, 0x001A  ! ld r0, 26(r15) — z8002 BA encoding
.word   0x6FF0, 0x0000  ! ld 0(r15), r0  — z8002 BA store encoding
```

Instructions using only immediate, register, or indirect-register addressing
modes encode identically in both modes and need no special handling.
