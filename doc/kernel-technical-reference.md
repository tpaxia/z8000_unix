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
5. Trap stub: saves R0-R12, switches to NONSEG+SYS
6. Trap stub: extracts syscall number from tag word, calls C handler
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

## Mixed-Mode Assembly in trap.s

The trap stub is assembled in z8001 (segmented) mode because the trap handler executes in SEG+SYS mode and needs segmented register addressing (`@RR14`). However, the middle section runs in NONSEG+SYS mode after the FCW switch, where base-address (BA) mode instructions have different encodings:

- **z8001 (segmented)**: 6 bytes — opcode(2) + segment(2) + offset(2)
- **z8002 (nonseg)**: 4 bytes — opcode(2) + displacement(2)

The z8k-coff-as assembler provides `.unsegm` and `.segm` directives to switch encoding mode within a single file. The NONSEG section of trap.s uses `.unsegm` so that instructions like `ld r0, 26(r15)` get correct 4-byte z8002 encodings, then switches back to `.segm` before the SEG+SYS register restore and IRET.

Instructions using only immediate, register, or indirect-register addressing modes encode identically in both modes and need no special handling.

## Syscall Dispatch

```c
int syscall_handler(int num, unsigned *regs)
```

Dispatches by syscall number via V7-style `sysent[]` function-pointer table (64 entries). Each entry holds `{ sy_call, sy_narg }`. Bounds-checks the syscall number and NULL-checks the handler before calling.

### Syscall Calling Convention

```
sc #N           — syscall number N (encoded in instruction tag word)
R1 = arg1       — e.g., fd for write
R2 = arg2       — e.g., buffer pointer for write
R3 = arg3       — e.g., byte count for write
R0 = return     — bytes written, or -1 on error
```
