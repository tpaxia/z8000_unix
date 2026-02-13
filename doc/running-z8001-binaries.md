# Running ACK-compiled C on the Z8000 Emulator

## Overview

The toolchain compiles C source to Z8000 machine code using ACK (Amsterdam Compiler Kit), then runs the resulting binary on the Z8000 software emulator. Both Z8001 (segmented) and Z8002 (non-segmented) modes are supported.

Pipeline: `C source -> ACK (cem -> cg -> as -> led) -> aslod -> reset vector prepend -> z8000emu`

## Z8001 vs Z8002

| | Z8001 (segmented) | Z8002 (non-segmented) |
|---|---|---|
| Compiler flag | `ack -mz8000` (default) | `ack -mz8000 -z8002` |
| Address format | 2 words (segment + offset) | 1 word (16-bit flat) |
| `call` pushes | 4 bytes (seg:offset PC) | 2 bytes (16-bit PC) |
| FCW | 0xC000 (segmented + system) | 0x4000 (non-segmented + system) |
| Emulator flag | `z8000emu -s` | `z8000emu` (no flag) |
| CG EM_BSIZE | 6 (2 LB + 4 ret addr) | 4 (2 LB + 2 ret addr) |

The `-z8002` flag flows through the ACK driver to affect three things:
1. **Assembler** (`-n` flag): switches `emit_ad()` to 1-word addresses and `*SP` to `@R15`
2. **Code generator** (selects `cg_z8002`): uses `EM_BSIZE=4` for correct parameter offsets
3. **Linker**: unchanged (always patches 16-bit relocations)

## Reset Vectors

Both modes use 8 bytes at address 0x0000. Code starts at 0x0008.

### Z8001 (segmented)

| Offset | Contents | Value |
|--------|----------|-------|
| 0x0000 | Reserved | 0x0000 |
| 0x0002 | FCW | 0xC000 |
| 0x0004 | Segment word | 0x8000 (segment 0, long format) |
| 0x0006 | Offset word | 0x0008 |

### Z8002 (non-segmented)

| Offset | Contents | Value |
|--------|----------|-------|
| 0x0000 | Reserved | 0x0000 |
| 0x0002 | FCW | 0x4000 |
| 0x0004 | PC | 0x0008 |
| 0x0006 | Padding | 0x0000 |

## The `*SP` Mnemonic

The assembler provides a `*SP` mnemonic that resolves based on the current mode:
- **Segmented** (default): `*SP` -> register 14 (indirect via RR14, the segmented stack pointer pair)
- **Non-segmented** (`-n`): `*SP` -> register 15 (indirect via R15, the flat stack pointer)

In both modes, R15 is the actual stack pointer. The difference is the push/pop encoding: Z8001 needs the register pair (RR14) for segmented indirect addressing, while Z8002 uses R15 directly.

All runtime libraries (libem, libsys, boot.s) use `*SP` so they can be assembled for either mode.

## ACK Code Generation

The code runs in segment 0 with all code, data, and BSS in the same 64KB segment. The CG backend generates Z8000 code from EM intermediate code:

- **Z8001 mode** (`cg`): `EM_BSIZE=6`, parameters at R13+6. Return sequences include `ldk R14, $0` to restore the segment register before `ld R15, R13`.
- **Z8002 mode** (`cg_z8002`): `EM_BSIZE=4`, parameters at R13+4. The `ldk R14, $0` in return sequences is harmless (zeroing an unused register).

## Binary Layout

```
0x0000  Reset vector (8 bytes)
0x0008  .text (boot.s entry point, then C runtime, then user code)
        .rom  (read-only data, immediately after text)
        .data (initialized data -- hol0, trppc, trpim, reghp)
        .bss  (uninitialized data -- errno, user globals)
```

The stack starts at 0x0000 and grows downward (wraps to 0xFFFE). Boot code sets `ldl RR14, $0` so the stack pointer (R15) decrements from the top of the 64KB segment.

## Running Tests

```sh
cd tests
./run_test.sh test_add.c                  # Z8001 (default), final state
./run_test.sh test_add.c -t               # Z8001, instruction trace
./run_test.sh test_add.c -p z8002         # Z8002 non-segmented
./run_test.sh test_add.c -p z8002 -t      # Z8002, instruction trace
./run_test.sh test_add.c -p z8001 -t -r   # Z8001, instruction + register trace
```

The script:
1. Compiles the C source with ACK (`ack -mz8000` or `ack -mz8000 -z8002`)
2. Extracts a flat binary with `aslod`
3. Prepends the appropriate 8-byte reset vector
4. Runs the result on `z8000emu` (`-s` for Z8001, no flag for Z8002)

## Building ACK

```sh
cd ack
# Default build (z8001 libraries):
gmake HOSTCC=cc CC=cc -j8 NINJA='ninja -k0'

# Build with z8002 libraries (for z8002 testing):
gmake HOSTCC=cc CC=cc "ACKCFLAGS=-O -z8002" -j8 NINJA='ninja -k0'
```

Note: the pre-built runtime libraries (boot.o, libem.a, libsys.a, libend.a) are assembled for one mode at a time. The CG binaries for both modes (`cg` and `cg_z8002`) are always built.

## Exit Behavior

`_exit` currently executes `halt` (opcode 0x7A00), which stops the emulator. The return value from `main()` is in R0 at halt time.
