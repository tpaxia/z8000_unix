# Structure-return ABI: PCC, the Z8000 port, and ZEUS

Status: proposed ABI change; not implemented. See [current ABI](abi.md).

Date: 2026-10-05

## Finding

The original Motorola 68000 PCC backend, our Z8000 PCC port, and the inspected
ZEUS 3.21 compiler use statically allocated aggregate-return storage. A function
returning a structure or union copies its result into a labelled buffer and
returns that buffer's address. Invocations of the same function share the
buffer.

This can corrupt a return value if a signal or interrupt re-enters that same
function while the first invocation copies its result, or before its caller
finishes consuming the result. Ordinary recursive calls do not by themselves
establish this failure; they require separate tests for result lifetimes and
expression evaluation.

The limitation was inherited by our port. The recent aggregate-copy repairs
fixed Z8000 instruction generation and copy lengths, but did not change the
static-buffer calling convention.

## PCC source evidence

In `PCC-z8000/68000/c68/code.c`, `efcode()` allocates a labelled `.bss` area,
copies the aggregate into it, and returns its address in D0. In
`PCC-z8000/z8000/cz8/code.c`, `efcode()` performs the corresponding operation
using R0 for the returned address. The inspected working compiler repository
was `/Users/paxia/Projects/PCC`, revision `ff47624`; the Unix project's separate
compiler checkout should be checked when integrating any ABI change.

Ritchie's November 1978 [Recent Changes to C](https://cm-bell-labs.github.io/who/dmr/cchanges.pdf)
already describes structure-return corruption under interrupt re-entry in the
PDP-11 implementation. This is historical evidence for the limitation, not
proof of direct source ancestry between that implementation and this 68000
backend.

## ZEUS binaries inspected

Source directory:

```
/Users/paxia/Projects/Zilog_S8000/tapes/extracted/upgrade-3.21/files/file-000/3.21.update/
```

| Binary | SHA256 |
| --- | --- |
| `cparse` | `2cec3fbf9275e385b27fcf7262337b59aea8fc8937a4a72c3cfd171ae4e867bd` |
| `codgen` | `ed236affb42a4c09da292417d28518b5cb0225f6d101207095a39c2cae0858e3` |
| `scodgen` | `0fb0ee8e3e3fd56b6a4ba5d84758c98e266b819578f170835e511b8470a17f70` |

The code generators were inspected using binary strings and disassembly through
`tools/zdis/zdis.py` in the S8000 project, backed by MAME `unidasm`.
**No compilation probes or interrupt-re-entry tests were executed.** These
findings establish the inspected code-emission paths; they are not a complete
ZEUS ABI specification or a statement about every ZEUS compiler release.

### Nonsegmented aggregate return

In `codgen`, the emitter at code address `0x8b48` conditionally emits these
assembly templates:

| File offset | Template |
| --- | --- |
| `0x12cde` | `ld r4,#L%d` |
| `0x12cec` | `ld r1,#%ld` |
| `0x12cfa` | `ldir @r4,@%s,r1` |
| `0x12d0c` | `ld %s,#L%d` |

The emitter calls register selector `0x851c` with a null operand. That selector
chooses register index 2; the register-name table at data address `0x3ac6`
maps index 2 to `r2`. The return sequence is therefore equivalent to:

```asm
ld    r4,#L_result
ld    r1,#word_count
ldir  @r4,@r2,r1
ld    r2,#L_result
```

This selects a fixed label as the destination and returns that label's address,
rather than receiving caller-owned return storage.

The parser contains the labelled allocation template `L%d:\t.=.+%ld\n` at
file offset `0x12fc6`, together with `.bss`/`.data` section switches. Its reference
at code address `0x84f0` was checked in raw bytes. `cparse` is a segmented
executable; the stock `zdis.py` invocation does not correctly decode its
segmented address forms, so its automatic instruction annotations were not
used as evidence for that reference.

### Segmented aggregate return

The `scodgen` return emitter at code address `0x8eda` contains corresponding
register-pair templates:

| File offset | Template |
| --- | --- |
| `0x13850` | `ldl rr4,#L%d` |
| `0x13860` | `ld r1,#%ld` |
| `0x1386e` | `ldir @rr4,@r%s,r1` |
| `0x13882` | `ldl r%s,#L%d` |

It has the same fixed-label result strategy, adapted to segmented addresses.
A complete segmented argument/register convention was not reconstructed.

### Other observed conventions

| Item | ZEUS nonsegmented evidence | Our Z8000 PCC port |
| --- | --- | --- |
| Word/pointer result | R2 | R0 |
| Leading word arguments | Register arguments R7, R6, etc. in inspected compiler-executable functions | Stack arguments |
| Stack/frame base | R15; code-emission aliases `sp := r15`, `fp := r15` | R15 stack pointer, R13 frame pointer |
| Preserved registers | R8–R14 saved/restored in inspected functions | R4–R7, R10–R12 and R14, with R13 frame pointer saved separately |
| Aggregate return | Address of fixed labelled storage | Address of static result buffer |

The register-argument observations come from the compiler executable's own
machine code. Exhaustive argument allocation, stack overflow arguments,
variadic calls, floating arguments, and segmented conventions remain to be
verified with generated-code probes.

## Proposed repair for our PCC port

Use a hidden pointer to caller-owned aggregate-return storage:

1. The caller reserves an aligned structure/union-sized temporary in its frame.
2. It pushes ordinary arguments right-to-left, then the result pointer as a
   hidden first argument.
3. After the callee saves R13, the hidden pointer is at `4(r13)` and ordinary
   arguments begin at `6(r13)`.
4. The callee copies the returned aggregate to that destination and returns
   its address in R0.
5. The caller removes all arguments, including the hidden pointer. Its result
   temporary remains valid for the surrounding expression.

Implementation points are `genscall()`/`gencall()` in `local2.c`, parameter
allocation in `bfcode()`, and aggregate return copying in `efcode()` in `code.c`.
Direct and indirect calls must agree. Stack accounting and temporary lifetimes
must also cover nested calls and aggregate arguments.

Initially use a distinct temporary for each simultaneously live result.
Passing the final assignment destination directly, or forwarding an incoming
return buffer to another function, can be later optimizations after aliasing and
evaluation-order checks.

This is an **aggregate-return ABI change**. Affected callers and callees,
including relevant V7 library functions and hand-written assembly, must be
rebuilt or adapted together. Scalar-returning functions retain their convention.

## Cost and verification

The conservative implementation adds a two-byte hidden argument, argument
setup/cleanup instructions, and stack storage for live aggregate results. It
retains the callee's aggregate copy. Safe destination forwarding can later
remove redundant copies. Cycle costs have not been benchmarked.

Before integration, test small and large structures, unions, recursion,
multiple aggregate calls in an expression, aggregate arguments, indirect calls,
register pressure, and deliberate re-entry during the return copy. Run the
existing compiler suites and rebuild affected V7 components. Native V7 signal
behavior needs verification in addition to emulator tests.

The recommended timing is before broad userland rebuilding, so the ABI changes
once while its users can be rebuilt consistently. ZEUS compatibility, if needed,
would require explicit adapters or retention of its old aggregate-return ABI;
its historical convention is not re-entrant either.
