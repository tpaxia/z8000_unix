# Step 3: Trap Infrastructure

> **Snapshot.** This note records the project as it was at this step. Paths (`kernel/...`), the compiler (ACK) and some details have changed since; [current status](../status.md) and [kernel overview](../kernel/overview.md) describe the current implementation.

Implemented the SYSCALL trap round-trip — the foundation for all system calls.

## What Was Built

- ROM init code (`kernel/rom.s`, segment 0): sets up the system stack (RR14 = seg1:0xFFF0), configures PSAP to point to the PSA table (seg1:0x0000), sets the normal-mode stack pointer, and uses a fake IRET frame to jump from SEG+SYS mode to NONSEG+SYS mode in the kernel segment.
- PSA table (`kernel/trap.s`): 8 interrupt/trap vector entries, each pointing to handlers in the kernel segment.
- SYSCALL entry/exit stub: saves R0-R12, transitions SEG+SYS -> NONSEG+SYS, calls the C handler, writes the return value into the saved-R0 slot, transitions back to SEG+SYS, restores registers, and executes IRET.
- Test driver (`kernel/test_driver.cpp`): loads ROM + kernel into the emulator's 8MB Z8001 address space and verifies the result.

## Simplifications

- No bootloader — the emulator front end loads ROM, kernel, and C handler binaries directly at their target physical addresses. This avoids building a bootloader or file system before the kernel itself works.
- Kernel hardcoded to segment 1, no MMU, identity-mapped.
- All code runs in the same segment — no separate user segments, no copyin/copyout.
- System stack at a fixed address (seg1:0xFFF0).
- Test code embedded in the kernel binary.

## Key Challenges Solved

- Z8001 PSA table entry format (8 bytes with segmented PC encoding).
- CHANGE_FCW R14/R15 swap semantics — R14 swaps with the saved stack segment register when F_SEG changes within system mode, R15 stays.
- PSAPSEG register encoding: `(seg << 8) | 0x8000`.
- The trap stub is assembled in z8001 mode (for `@RR14`) but must emit z8002 encodings for the NONSEG section. The z8k-coff-as `.unsegm`/`.segm` directives switch encoding mode within the file.

See [trap reference](../kernel/traps-and-interrupts.md) for full architecture details (PSA table layout, CPU mode transitions, SYSCALL flow, stack layout, mixed-mode assembly).

## Test

`sc #0` -> trap handler sets R0=7 -> IRET -> halt with R0=7. PASS.
