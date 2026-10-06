# Step 8: Fork, Paged MMU, and V7-Style Context Switching

> **Snapshot.** This note records the project as it was at this step. Paths (`kernel/...`), the compiler (ACK) and some details have changed since; `implementation-steps.md` and `kernel-technical-reference.md` describe the current state.

Added process management (fork/exit/wait), a paged MMU emulation, and V7-style context switching using a KDSA6-equivalent I/O port. Process 0 forks process 1, which writes a message via syscall and exits.

## Paged MMU

The Z8001 has no hardware MMU. We emulate one in the test driver: 128 segments x 32 pages x 2KB pages, identity-mapped on construction. Two I/O ports control page remapping:

| Port | Width | Name | Function |
|------|-------|------|----------|
| 0x00B0 | word | UPAGE | KDSA6 equivalent: remaps seg1 pages 30-31 (virtual 0xF000-0xFFFF) |
| 0x00B4 | word | WPAGE | Copy window: remaps seg1 pages 28-29 (virtual 0xE000-0xEFFF) |

A single `out 0x00B0, R0` swaps the u-area and kernel stack, exactly like `mov r0, KDSA6` on the PDP-11. The copy window lets `newproc()` write to the child's physical frames through a virtual address.

### Address Translation

```
segment = (addr >> 16) & 0x7F
page    = (addr & 0xFFFF) >> 11    // upper 5 bits of offset
pg_off  = addr & 0x7FF             // lower 11 bits
frame   = m_pages[segment][page]
physical = (frame << 11) | pg_off
```

## u-area at Fixed Virtual Address

The u-area occupies virtual 0xF000-0xFFFF (4KB = seg1 pages 30-31). The kernel stack grows down from 0xFFFE within the same pages.

```c
/* h/user.h */
#define u (*(struct user *)0xF000)
```

No `struct user u` variable in BSS. The MMU's UPAGE port remaps these two virtual pages to different physical frames per process.

## save() / resume() — V7 Style

### label_t[12]

The PDP-11 uses `label_t[6]` (R2-R5, SP, return address). Our Z8000 uses `label_t[12]`:

| Index | Contents |
|-------|----------|
| 0-8 | R4-R12 (callee-saved registers) |
| 9 | Caller's R13 (frame pointer) |
| 10 | Return address |
| 11 | Caller's SP |

(Since Step 12, when the frame pointer moved to R13 and R14 became callee-saved, the layout is: 0–3 R4–R7, 4–6 R10–R12, 7 R14, 8 caller's R13, 9 return address, 10 caller's SP, 11 unused.)

The label_t stores the return address and SP explicitly because `resume()` cannot depend on stack contents — `newproc()` calls `bcopy()` after `save()` returns, which overwrites save's deallocated stack frame before the u-area snapshot is taken. The PDP-11 avoids this because `copyseg()` copies physical memory click-by-click without touching the virtual stack.

### save(label)

Stores R4-R12, caller's R13 (from stack), return address (from stack), and caller's SP into label_t. Returns 0.

### resume(p_addr, label)

No prologue — reads arguments from R15 before the stack is remapped. Writes KDSA6 (`out 0x00B0, R0`) to remap the u-area, then restores all state from label_t (registers, R13, SP). Pushes the return address onto the restored stack and returns 1.

## Context Switching (swtch)

Identical to V7 PDP-11 `swtch()`:

```c
swtch() {
    if (u.u_procp != &proc[0]) {
        if (save(u.u_rsav)) { sureg(); return; }
        resume(proc[0].p_addr, u.u_qsav);
    }
    if (save(u.u_qsav)==0 && save(u.u_rsav))
        return;
    /* search runq for highest priority, idle if empty */
    resume(p->p_addr, n? u.u_ssav: u.u_rsav);
}
```

No bcopy of u-areas. No per-process kernel stacks. `resume()` handles everything via the KDSA6 port.

## Process Creation (newproc)

V7-style fork:

1. Find a free proc slot, generate a unique PID
2. Copy parent's proc entry fields (uid, pgrp, nice, etc.)
3. `frame_alloc()` — allocate a 2-frame (4KB) pair for the child's u-area
4. Bump reference counts on open files, cdir, rdir
5. `save(u.u_ssav)` — when the child is later resumed, it returns here with value 1
6. Copy the parent's u-area to the child's physical frames via the MMU copy window:
   ```c
   outw(0x00B4, rpp->p_addr);        /* map window to child's frames */
   bcopy(0xF000, 0xE000, 4096);      /* copy u-area + kernel stack */
   outw(0x00B4, WPAGE_IDENTITY);     /* restore window identity map */
   ```
7. Put the child on the run queue with `SSWAP` flag

## User-Mode Entry (retu)

After `main()` returns in the child process, `krt.s` calls `retu()` which:

1. Sets up NSPSEG/NSPOFF for the user stack
2. Switches to SEG+SYS mode (R14 swapped with NSPSEG)
3. Builds an IRET frame on the system stack: user segment:0x0000, FCW=0x0000 (NONSEG+NORM)
4. Clears all user registers
5. Executes IRET to enter user mode at the start of the user segment

The user segment contains `icode[]` — a small Z8002 program that calls `write(1, "hello from process 1\n", 21)` then `exit(0)`.

## Cross-Segment Memory Access

`fubyte`/`subyte`/`fuword`/`suword`/`copyin`/`copyout` in `krt.s` temporarily switch to SEG+SYS mode for segmented addressing. The `useg` variable (set by `sureg()` during context switch) provides the user segment encoding. Only register-indirect instructions are used in SEG mode — BA/DA instructions would be decoded with 6-byte segmented format.

## Memory Allocation

### Frame allocator (machdep.c)

Physical frame allocator for u-area pages. Each u-area is 4KB = 2 frames (2KB pages). Frames 0-95 are reserved for identity-mapped segments 0-2.

```c
frame_alloc()   /* returns base frame for a 2-frame pair */
frame_free(f)   /* frees a 2-frame pair */
```

### Segment allocator (machdep.c)

Each user process gets a dedicated 64KB segment for its user-space code/data. Segment numbers start from 2 (0=ROM, 1=kernel).

## Bugs Found and Fixed

### ACK code generator byte zero-extension bug

The comparison loop in `namei()` failed because ACK's MOVES rule for byte-to-word zero-extension clobbers the index register:

```asm
ldk R1, $0      /* clears R1 — but R1 is the index! */
ldb LR1, addr(R1)   /* reads wrong address */
```

The code generator table (`mach/z8000/cg/table` line 1945) cleared the destination register before loading the byte. If the source operand used the destination as an index register, the clear destroyed the index.

Fixed in ACK: reversed the order — load the byte first, then clear the high byte:
```asm
ldb LR1, addr(R1)   /* load byte (source addressing intact) */
clrb HR1             /* clear high byte (zero-extend) */
```
Also added `HR0`-`HR7` assembler aliases (for `RH0`-`RH7`) so the `H%` prefix in code generator rules produces valid register names. The `nami.c` comparison loop now uses the original V7 code without any workaround.

### Stack clobber in newproc()

After `save(u.u_ssav)` returns 0, its stack frame is deallocated. Subsequent function calls (outw, bcopy) push frames to the same stack locations, overwriting the saved R13 and return address. `bcopy()` then snapshots the corrupted stack into the child's u-area.

Fix: expanded `label_t` from 10 to 12 words so `save()` captures the return address and SP into the label_t itself. `resume()` restores everything from the label_t without reading the stack.

## Files Changed

| File | Change |
|------|--------|
| `test_driver.cpp` | Added MMU class, UPAGE/WPAGE I/O ports |
| `h/param.h` | `label_t[12]` |
| `h/user.h` | `#define u (*(struct user *)0xF000)` |
| `krt.s` | save/resume (stack-independent), outw, boot SP at 0xFFFE, retu, copyin/copyout/fubyte/subyte/fuword/suword |
| `sys/slp.c` | sleep/wakeup, swtch, newproc, setrq/setrun/setpri, qswtch |
| `sys/main.c` | V7-style `if(newproc())` fork pattern |
| `sys/sys1.c` | fork, exit, wait, write syscalls |
| `sys/machdep.c` | frame_alloc/free, seg_alloc/free, sureg, icode[] |
| `sys/nami.c` | Original V7 code (ACK bug fixed in compiler) |

## Test

CPU halted, console output = "boot\nZ8000 Unix\nhello from process 1\n", no panics. PASS.
