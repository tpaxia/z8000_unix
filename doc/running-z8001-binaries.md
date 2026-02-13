# Running ACK-compiled C on the Z8001 Emulator

## Overview

The toolchain compiles C source to Z8000 machine code using ACK (Amsterdam Compiler Kit), then runs the resulting binary on the Z8001 software emulator in segmented mode.

Pipeline: `C source → ACK (cem → cg → as → led) → aslod → reset vector prepend → z8000emu`

## Z8001 Reset Vector

The Z8001 CPU reads its initial state from a reset vector at address 0x0000. The reset vector is 8 bytes:

| Offset | Size | Contents | Value |
|--------|------|----------|-------|
| 0x0000 | 2 | Reserved | 0x0000 |
| 0x0002 | 2 | FCW (Flags/Control Word) | 0xC000 |
| 0x0004 | 2 | Segment word | 0x8000 |
| 0x0006 | 2 | Offset word | 0x0008 |

### FCW bits

- Bit 15: Segmented mode (1 = segmented addressing)
- Bit 14: System/Normal mode (1 = system mode, privileged)

FCW = 0xC000 means segmented + system mode.

### PC encoding

The Z8001 always uses segmented format for the reset PC, even when running non-segmented code:

- **Segment word**: `(segment << 8) | 0x8000` — the 0x8000 bit indicates "long format" (16-bit offset follows)
- **Offset word**: 16-bit offset within the segment

For segment 0, offset 0x0008: segment word = 0x8000, offset word = 0x0008.

The entry point is 0x0008 because the first 8 bytes are occupied by the reset vector itself. ACK's linker is configured with `-b0:0x0008` in the platform descriptor to link code starting at this address.

## ACK Z8000 Code Generation

ACK's Z8000 backend generates **Z8001 segmented** code, not Z8002. All direct address references use the segmented long format (`0x8000 | seg<<8, offset`). This is hardcoded in the assembler's `emit_ad()` function.

The code runs in segment 0 with all code, data, and BSS in the same 64KB segment.

## Binary Layout

```
0x0000  Reset vector (8 bytes)
0x0008  .text (boot.s entry point, then C runtime, then user code)
        .rom  (read-only data, immediately after text)
        .data (initialized data — hol0, trppc, trpim, reghp)
        .bss  (uninitialized data — errno, user globals)
```

The stack starts at 0x0000 and grows downward (wraps to 0xFFFE). Boot code sets RR14 = 0 so the stack pointer (R15) decrements from the top of the 64KB segment.

## Running a Test

```sh
cd tests
./run_test.sh test_add.c -t        # instruction trace
./run_test.sh test_add.c -t -r     # instruction + register trace
./run_test.sh test_add.c            # no trace, just final state
```

The script:
1. Compiles the C source with ACK (`ack -mz8000unix`)
2. Extracts a flat binary with `aslod`
3. Prepends the 8-byte Z8001 reset vector
4. Runs the result on `z8000emu -s` (Z8001 segmented mode)

## Exit Behavior

`_exit` currently executes `halt` (opcode 0x7A00), which stops the emulator. The return value from `main()` is in R0 at halt time.
