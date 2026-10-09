# Structure-return ABI

Current Z8000 PCC uses caller-owned aggregate-return storage. See [current ABI](abi.md).

## Calling convention

A structure/union caller reserves a word-aligned temporary in its frame, pushes
ordinary arguments right-to-left, then pushes the temporary's address as a hidden
first argument. The callee finds that address at `4(r13)` and its ordinary
arguments at `6(r13)` onward. It copies the result there and returns the address
in R0. Argument cleanup includes the hidden pointer. Scalar calls are unchanged.

Each live result has distinct frame storage, so another invocation, including
a signal handler, cannot overwrite it. `gencall()` allocates through PCC's
`freetemp()`; `bfcode()` shifts aggregate-function parameters and `efcode()`
copies into the supplied destination. The hidden-pointer push preserves R8
with `ex r8,@sp`, including indirect callees held in that register.
Temporary allocation returns `OFFSZ` and uses long arithmetic for bit offsets;
a native 16-bit `int` cannot represent offsets beyond 4 KiB.

This changes the aggregate-return ABI. Rebuild affected callers and callees,
including libraries, with matching compiler passes. s.out does not encode the
calling-convention revision and cannot detect mixed old/new objects. Historical
ZEUS aggregate callees require explicit adapters; their convention is different.

## Historical comparison

The following observations describe the compiler implementations inspected on
2026-10-05, before the current Z8000 aggregate-return change.

### Static result storage

The inspected Motorola 68000 PCC backend, earlier Z8000 backend, and
ZEUS 3.21 compiler used statically allocated aggregate-return storage. A function
returning a structure or union copies its result into a labelled buffer and
returns that buffer's address. Invocations of the same function share the
buffer.

This can corrupt a return value if a signal or interrupt re-enters that same
function while the first invocation copies its result, or before its caller
finishes consuming the result. Ordinary recursive calls do not by themselves
establish this failure; they require separate tests for result lifetimes and
expression evaluation.

The port previously inherited this limitation. Its current caller-owned
storage replaces the static-buffer convention.

## PCC source evidence

In `PCC-z8000/68000/c68/code.c`, `efcode()` allocates a labelled `.bss` area,
copies the aggregate into it, and returns its address in D0. The earlier Z8000 backend performed the corresponding operation using R0
for the returned address. The separate working compiler repository inspected
at that time was `/Users/paxia/Projects/PCC`, revision `ff47624`. Current source
in the Unix project's PCC submodule implements the convention above.

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

| Item | ZEUS nonsegmented evidence | Z8000 PCC before this ABI change |
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

## Cost and verification

Calls add a two-byte hidden argument, pointer setup/cleanup instructions, and
frame storage for live aggregate results. The callee still copies the aggregate.
Destination forwarding remains a possible optimization; it is not implemented.

The compiler regression covers small and large structures, unions, recursion,
multiple live results, nested aggregate arguments and indirect calls. A 5 KiB
return probe checks every word after direct and indirect calls, including native
compilation and execution. Native
`aggregate-signal.c` re-enters the same aggregate-returning function from signal
handlers while its caller checks every returned word, in combined and split I/D.
The test compiles it inside Unix, then uses native sed to split its 512-byte
return copy into two halves with a self-signal hook between them. Nested hooks
are suppressed. This forces re-entry during the copy, rather than relying on
alarm timing; it does not alter production compiler output. In this probe the prior static-buffer
compiler produced 8,192 corrupted words across 66 signal re-entries; the current
compiler produced zero in both layouts.

```sh
python3 PCC-z8000/z8000/test/regress/run.py --strict
python3 PCC-z8000/z8000/test/regress/run.py --strict --compact
python3 tools/native-cc/build.py
python3 tools/test-kernel-gaps.py
python3 tools/native-cc/selfhost.py --setup
```

The native test requires the kernel/test driver and shared object utilities to
be built; see [testing](../development/testing.md). The self-host runner rebuilds
two compiler generations and compares every object and executable.
