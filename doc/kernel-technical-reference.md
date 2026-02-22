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

## Boot Flow (V7 Kernel)

```
ROM reset → seg0:0x0010 (init)
  → set PSAP to seg1:0x0000, system stack RR14 = seg1:0xFFF0
  → set NSP = 0xFFF0
  → IRET to seg1:0x0100 (NONSEG+SYS)

seg1:0x0100 (trap.s boot entry):
  → call 0x0202

seg1:0x0202 (krt.s boot_entry):
  → zero BSS (begbss..endbss)
  → ld R15, $0xFFFE   (kernel stack at top of u-area page)
  → calr _main

main() (sys/main.c):
  → proc[0] setup: p_stat=SRUN, p_flag=SLOAD|SSYS, p_addr=62
  → u.u_procp = &proc[0], u.u_error = 0
  → rootdev = makedev(0, 0)
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
  → icode: write(1, "hello from process 1\n", 21) → exit(0)
```

## Paged MMU

The emulated MMU provides 128 segments x 32 pages x 2KB pages, identity-mapped on construction. Two I/O ports remap specific pages within segment 1:

### MMU Control Ports

| Port | Width | Name | Function |
|------|-------|------|----------|
| 0x00B0 | word | UPAGE | KDSA6 equivalent: sets seg1 pages 30-31 to frame pair (value, value+1) |
| 0x00B4 | word | WPAGE | Copy window: sets seg1 pages 28-29 to frame pair (value, value+1) |

### Address Translation

```
segment = (addr >> 16) & 0x7F
page    = (addr & 0xFFFF) >> 11    // 5 bits → 32 pages
pg_off  = addr & 0x7FF             // 11 bits → 2048 bytes per page
frame   = pages[segment][page]
physical = (frame << 11) | pg_off
```

### u-area Mapping

The u-area occupies virtual 0xF000-0xFFFF (4KB = pages 30-31 of segment 1). The kernel stack grows down from 0xFFFE within these pages. `resume()` writes UPAGE to remap these two pages to the target process's physical frames, swapping the entire u-area + kernel stack with a single I/O port write.

Process 0's u-area is at frame 62 (identity-mapped: seg1 page 30 = frame 62). Forked processes get frames from `frame_alloc()` starting at frame 96.

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
cdevsw[0] = { consopen, consclose, consread, conswrite } — console
cdevsw[2] = { consopen, consclose, consread, conswrite } — /dev/tty alias
```

## PCC Calling Convention (Z8002)

| Aspect | Convention |
|--------|-----------|
| Stack pointer | R15 |
| Frame pointer | R14 |
| Return value | R0 (int/pointer), RR0 (long) |
| Arguments | Pushed right-to-left onto R15 stack |
| Callee-saved | R4-R14 |
| Function prologue | `push @sp, r14; ld r14, sp; sub sp, #N` |
| Function epilogue | `ld sp, r14; pop r14, @sp; ret` |

Assembly functions called from C must return values in R0. The `save()`/`resume()` functions in `krt.s` preserve R4-R14 and the return address in `label_t`.

Note: Steps 1-10 used ACK which has a similar convention but with R13 as frame pointer.
