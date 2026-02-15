# ACK Compiler for Z8000

The project uses the Amsterdam Compiler Kit (ACK) as the C compiler. ACK was chosen because it already has a Z8000 code generator backend and its ANSI C frontend compiles V7 K&R C source unchanged — proven by Robert Nordier's V7/x86 port, which compiled the entire V7 kernel with zero syntax modifications.

The ACK submodule (`ack/`) is a fork of [davidgiven/ack](https://github.com/davidgiven/ack) on the `z8000unix` branch.

## Building

```sh
cd ack
gmake HOSTCC=cc CC=cc -j8 NINJA='ninja -k0'
```

The default build produces Z8002 (non-segmented) runtime libraries (`ACKCFLAGS = -O -z8002`). Both code generators (`cg` for Z8001 and `cg_z8002`) are always built. The runtime libraries (boot.o, libem.a, libsys.a, libend.a) are assembled for one mode at a time — currently Z8002.

## Compilation Pipeline

```
C source -> cem (frontend) -> EM intermediate -> cg (code generator) -> as (assembler) -> led (linker)
```

The `aslod` tool extracts a flat binary from the ACK output format.

## Existing Z8000 Backend

The Z8000 code generator was written by Jan Voors at the Vrije Universiteit Amsterdam (Tanenbaum's group) in 1983. It originally targeted only the Z8001 (segmented) mode.

**Code generator table** (`mach/z8000/cg/table`): ~1,900 lines covering all 15 EM operation groups with extensive peephole optimizations (~60 patterns for arithmetic alone, 36 for fused compare+test+logical).

**Assembler** (`mach/z8000/as/`): covers essentially the entire Z8000 instruction set including segmented addressing modes.

**Runtime library** (`mach/z8000/libem/`): 34 assembly files implementing EM operations too complex for inline code (array ops, block move, integer conversions, case dispatch, etc.).

## Extending ACK for Z8002 (Non-Segmented) Mode

The original backend only supported Z8001 segmented mode. We extended it to support Z8002 non-segmented mode, which the kernel and user processes use.

### Platform and Build System

Created the `plat/z8000` platform definition with build system integration (`build.py` files for code generator, assembler, libem, libend). Added `-z8001`/`-z8002` mapflags in the platform descriptor so the flag flows through the ACK driver to the assembler and code generator.

### Assembler: `*SP` Mnemonic and `-n` Flag

Added a `*SP` assembler mnemonic that resolves based on the current mode:
- **Segmented** (default): `*SP` -> `*RR14` (register pair for segmented indirect addressing)
- **Non-segmented** (`-n` flag): `*SP` -> `@R15` (single register for flat addressing)

This allows all runtime libraries to be assembled for either mode from the same source. Added `.segm`/`.unsegm` directives and fixed `emit_ad()` to emit 1-word addresses in non-segmented mode.

### Code Generator: Separate `cg_z8002`

Built a separate code generator (`cg_z8002`) compiled with `-DZ8002=1`. The key difference is `EM_BSIZE=4` (2-byte local base + 2-byte return address) vs `EM_BSIZE=6` for Z8001 (2-byte LB + 4-byte segmented return address). This affects all parameter offsets in generated code.

### Code Generator: IR-Mode Indirect Addressing

During kernel bringup, pointer dereferences in Z8002 mode were producing wrong results. The code generator table's IR-mode token definitions and all LOI/STI/LIL/SIL rules used register pairs (LWXREG) for indirect addressing, putting the address in `%[a.2]` (the odd register — the offset half in Z8001 segmented mode) and zeroing `%[a.1]` (the even register — the segment half).

In Z8002 mode, `*RR2` uses only R2 (the even register) as the 16-bit address, ignoring R3. Added `#ifdef Z8002` blocks at 27 sites to put the address in `%[a.1]` instead. The ACK assembler only accepts register-pair syntax (`*RR2`), so LWXREG tokens are kept — only which half receives the address changes.

This fixed both `char *` and computed pointer dereferences, allowing `cons_write()` to be written in C instead of assembly.

### Code Generator: Same-Size Type Conversions

Assigning `unsigned *` elements to `int` locals generated calls to the EM runtime's `cii`/`cuu` functions, which consumed the value from the stack without pushing a result for same-size conversions (e.g., `unsigned` to `int` when both are 16-bit). This required the kernel's `syscall_handler()` to use `int *regs` as a workaround. Added no-op code generator patterns so same-size conversions are eliminated at compile time, allowing `unsigned *regs` to be used directly:

```
loc loc cii $1==$2   | | | | |
loc loc cuu $1==$2   | | | | |
loc loc cui $1==$2   | | | | |
loc loc ciu $1==$2   | | | | |
```

### Runtime Libraries: `*SP` Migration

Replaced `*RR14` with `*SP` across libem (30 files), libmon (2 files), and boot.s, so the same source assembles correctly for both modes.

## Known Limitations

- **No floating point**: All FP operations trap with EILLINS. The Z8000 has no FPU and no software FP library was implemented. Not needed for the kernel.
- **Non-reentrant runtime**: A shared global `saveret`/`savereg` area is used by `blm`, `dvu2/4`, `rmu2/4`, `sar`, `lar`, `cii`, `cms`, `dup`. These routines are not interrupt-safe. Must be fixed for a real OS kernel.
- **Empty sigtrp.s**: Signal trap support is unimplemented.
