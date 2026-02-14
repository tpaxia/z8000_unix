# Kernel Bring-up: PSA Table, Trap Stubs, and Multi-Segment Support

## Overview

First step of Unix V7 kernel bring-up on the Z8001. Implements the trap infrastructure needed for system calls: a PSA (Program Status Area) table, SYSCALL entry/exit stubs, and ROM init code that boots into a multi-segment configuration.

The test proves the full SYSCALL trap round-trip: ROM init (segment 0) -> IRET to kernel (segment 1, NONSEG+SYS) -> `sc #0` -> trap handler (SEG+SYS) -> register save -> NONSEG+SYS -> set R0=7 -> SEG+SYS -> register restore -> IRET back -> halt with R0=7.

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

### SYSCALL Flow

1. User/kernel code executes `sc #N`
2. CPU sets `CHANGE_FCW(old | F_S_N | F_SEG)` -> SEG+SYS mode
   - R14/R15 swapped with m_nspseg/m_nspoff as needed
3. CPU pushes PC(4) + FCW(2) + tag(2) = 8 bytes onto system stack via *RR14
4. CPU loads new FCW and PC from PSA[SYSCALL] -> jumps to trap stub
5. Trap stub: saves R0-R12, switches to NONSEG+SYS, calls C handler
6. Trap stub: switches back to SEG+SYS, restores registers, IRET
7. IRET pops tag(2) + FCW(2) + PC(4), CHANGE_FCW restores original mode

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
| `kernel/trap.s` | PSA table + syscall handler + test code (segment 1, assembled z8001 mode) |
| `kernel/test_driver.cpp` | Standalone emulator driver, loads ROM + kernel, checks R0==7 |
| `kernel/Makefile` | Build rules for assembly (with listings), driver, and test |

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
- `rom.lst`, `trap.lst` — assembler listing files
- `test_driver` — test executable (links against `z8000_emu/build/libz8000.a`)

The test driver runs with a 10,000 cycle safety limit and verifies R0==7 after halt.

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
3. C handler code runs here (currently: `ld r0, #7`)
4. `ldctl fcw, #0xC000` -> SEG+SYS (R14 swapped back)
5. Restores R0-R12 via `pop rN, @rr14`
6. Sets R0 = return value
7. IRET -> pops tag, FCW, PC, restores caller mode
