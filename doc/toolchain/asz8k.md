# Experimental Zilog Assembler Port

The `work/native-asz8k` branch contains a native V7 port of the CP/M-8000
`asz8k` assembler. It is an evaluation of the assembler needed for segmented
kernel components, not a replacement for the installed PCC toolchain.
[Build procedure](../development/native-rebuild.md#experimental-zilog-assembler).
[Source provenance and port changes](../../tools/asz8k/README.md).

## Verified behavior

Native PCC compiles the assembler C sources inside Unix; native az8 and ldz8 assemble
and link the resulting program. The program runs in NONSEG split I/D and can
assemble both NONSEG and segmented Z8001 source.

Five Unidot objects match the original CP/M distribution assembler byte for
byte, including symbols and relocations:

| Workload | Object bytes |
|---|---:|
| Supplied opcode corpus, with damaged tail repaired | 2,077 |
| Segmented indirect, short/long direct addresses, offset and pointer data | 90 |
| EPA floating-point instructions | 36 |
| Macro expansion, conditional assembly and repetition | 39 |
| Existing full FPE source with Z8002 definitions | 10,842 |

The trial also checks explicit output filenames, mode/argument errors and
controlled exhaustion of the 16-bit disk-backed symbol store. The trial retains
the a.out interoperability checks below and adds s.out layout and relocation
checks. This is not an exhaustive instruction-set validation.
The native trial also checks cross references and 32-bit expression boundaries;
the full host assembler independently repeats its assembler commands and compares
object files and listings byte for byte.
All 76 native build/test steps and 40 host assembler commands pass. The host
and native assemblers produce 30 byte-identical object and listing files.

The legacy-linked native executable has 52,000 bytes of text, 32,480 bytes of initialized
data and 1,190 bytes of BSS. During full FPE assembly to a.out, the observed
maximum break is 51,136 and minimum stack pointer is 64,860, leaving 13,724
bytes between them. These are workload measurements, not arbitrary-input
guarantees.

## Comparison with the installed assembler

| Capability | PCC az8 | Ported asz8k |
|---|---|---|
| Native V7 execution | Established system tool | Tested on this branch |
| Addressing | NONSEG | NONSEG and segmented |
| Object format | Port a.out, read by ldz8 | Unidot; NONSEG a.out with `-a`; NONSEG/SEG s.out with `-z` |
| Segmented relocation | No dedicated representation | Short address, long address, offset |
| EPA floating mnemonics | Not provided | Present and tested |
| Source facilities | Compiler-oriented directives | Macros, conditionals, repetition, sections, listings, cross-references |
| Storage | Heap-based symbol/branch records | Physical opcode tables and disk-backed symbol/macro store |

## Complete host and native builds

The host `tools/asz8k/Makefile` and native `src/makefile` compile the same 19 C
sources. The host uses `ASZ_HOST` for 16-bit relocation/virtual-store values,
32-bit expressions, full-width instruction-table pointers and pointer-aligned
input frames. A stable host arena preserves contiguous instruction-format
arrays; native V7 retains its existing `sbrk` allocator. Internal functions
whose names collide with modern libc are renamed, and historical initializers
use syntax accepted by both compilers. Host cross-reference formatting preserves
V7's lowercase long-hex output. Outside PCC mode instruction selection is
unchanged; PCC mode adds the branch relaxation described above.

`host.py` runs the same assembly commands against source files, independently
of native instruction bytes or emission events. It compares complete objects,
including symbols and relocations, and listings including cross references.
Coverage includes the original CP/M fixtures, full FPE, SEG/NONSEG s.out,
transitional a.out, explicit output filenames and error cases. A separate
expected-byte check covers signed 32-bit boundaries, wrapping arithmetic,
shifts and byte/word values. The comparison rejects stale native source trees.

The intended system format remains s.out. The a.out writer is temporary support
for the currently installed linker/loader; Unidot supplies the original
assembler oracle. Neither is a second permanent format migration target.

## NONSEG a.out backend

`asz8k -a` selects direct output in the port's existing a.out format. The
backend writes the 16-byte header, text/data, relocation records and 12-byte
symbols without producing or converting a Unidot file. Without `-a`, the
original Unidot backend remains the default for oracle comparisons.

The initial backend maps `__text`, `__data` and `__bss` to a.out sections. It
supports local and external byte/word/long relocations and adjusts local data
and BSS references to a.out's combined object address convention. Sections are
padded to four bytes, as in az8. A single relocatable object must fit the
16-bit combined address convention; final split-I/D linking still uses
separate code and data address spaces. Absolute local constants too wide for
the 16-bit symbol table are omitted after their expressions have been resolved;
wide absolute globals are rejected.

Native tests link the emitted objects with PCC-compiled C and libc, run both
0407 combined and 0411 split-I/D executables, and compare the stripped results
byte-for-byte with az8-built equivalents. A partial `ldz8 -r` link produces the
same final executable. The tests exercise external calls and absolute symbols,
local data/BSS references, function/data pointers and byte/word/long relocation.
An independent decoder compares the FPE object's section bytes and relocation
targets with the original-assembler Unidot output. The direct FPE a.out object
contains 4,284 bytes of text, 712 bytes of data and no BSS.

Segmented output, other named sections, common/fixed sections, transfer
addresses and alignment above four bytes are rejected explicitly. This is a
NONSEG backend, not a new segmented object format.

ZEUS uses a different format again: its recovered `a.out(5)` describes **s.out**,
with a 24-byte descriptor and segment table. ZEUS `as` accepts PLZ/ASM while
`cas` accepts a separate Z8000 assembly syntax; the `cas(1)` manual explicitly
says it does not accept PLZ/ASM. Those tools and object files are not drop-in
substitutes for this assembler or ldz8. The recovered segment and relocation
specifications define the new segmented backend below.

## Initial s.out backend

`asz8k -z` writes a NONSEG relocatable object; `asz8k -z -s` writes a segmented
object. The default suffix is `.so`; `-o` overrides it. `-a` and `-z` are
mutually exclusive. This writes s.out directly, without an intermediate Unidot
file. The shared K&R C serializer explicitly encodes big-endian disk fields:
a 24-byte descriptor, 16-byte segment entries, image bytes, one relocation word
per image word, and 14-byte symbol entries. Relocation information is retained.

The initial section contract is `__text`, `__data` and `__bss`, aligned to two
bytes. NONSEG objects use one segment and combined section-relative addresses.
SEG objects give each declared section its own unbound logical segment; segment
placement and merging belong to the [s.out linker](ldz8.md). These object sections
are word-padded, not padded to the executable's 256-byte loading boundaries.
Recovered ZEUS `scrt0.o` and `crt0.o` confirm this distinction.

Supported relocations are word offsets, short segmented addresses, and long
segmented addresses. A long address receives separate segment and offset tags;
both local and external targets are supported. SEG symbol values are offsets
with a separate segment-table index, as in recovered ZEUS `scrt0.o`. Symbols
retain the imported assembler's eight-character limit. Absolute 32-bit constants
are supported; relocatable bytes and plain 32-bit integer relocations are
rejected because Z8000 s.out has no equivalent relocation tags. Common/fixed
sections, other section names, transfer addresses, alignment above two bytes,
overlapping output and section/combined-address overflow are rejected. Known
short-address offsets above 255 are rejected before encoding; final link-time
short-address range checks still belong to the linker. NONSEG word relocation
uses the same modulo-65536 addend arithmetic as the existing a.out backend,
including negative symbol addends.

Tests compile the serializer with native PCC and host C and compare both
outputs with independently packed expected bytes. Native SEG/NONSEG assembly
fixtures cover local/external long, short and offset references, addends and
BSS. An independent decoder checks section sizes, symbol indices, relocation
tags and patched image bytes. The complete host assembler independently
assembles the same sources and must emit identical s.out files. The opt-in
s.out linker reads these objects; the kernel loads linked NONSEG executables.
The standard images still use az8/a.out. The s.out C trial uses the PCC syntax
mode below.

## PCC source compatibility

`-zc` accepts NONSEG PCC/az8 assembly directly and emits s.out. It accepts
`.az8` filenames, `.text`, `.data`, `.bss`, `.globl`, `.comm`, `.even`,
`.zerow`, frame-size assignments and forward location-counter reservations.
Numeric syntax includes C hexadecimal/octal constants; `!` starts a comment.
`sp` names r15, and byte operand positions accept PCC's r0–r7 spelling for
rl0–rl7. Floating condition aliases are ordinary PCC runtime symbol names in
this mode. Undefined identifiers become externals; common storage declarations
are resolved by the [linker](ldz8.md).

Repeated layout passes promote out-of-range `jr` instructions to `jp` until
the layout stabilizes. Decisions use a completed layout before applying any
promotion. Instruction encoding still uses the imported assembler tables,
including compact byte-immediate loads. Assembly files need no external syntax
rewriter. The mode requires `-z` and does not support SEG compiler output.
Predefinitions are read from the current directory, falling back to
`/usr/lib/asz8k.pd` in PCC mode. Output paths can be up to 127 characters;
source basenames retain the 14-character V7 limit.

## Remaining integration

The CP/M pipeline is `asz8k -> Unidot -> xcon -> x.out -> ld8k`. The checkout
contains C sources for asz8k and ld8k, but only an executable for xcon.
The Unix port bypasses that pipeline for a.out and s.out output. Segmented
assembly emits Unidot by default or s.out with `-z`; it does not invoke the
CP/M converter or claim to produce a linked kernel image.

The initial segmented writer uses ZEUS s.out. Existing ldz8 integer
relocations alone do not establish general
segmented linking.

The kernel's GNU-syntax machine assembly also needs adaptation. Asz8k retains
eight-character significant symbols, requires `.8kn`/`.8ks` main filenames,
and reads its predefinition file from the current directory. These constraints
need review when converting existing labels and build recipes.

The result supports reusing this assembler rather than implementing its
segmented parser and instruction support afresh. PCC assembly-syntax
integration, segmented linking, kernel artifact reproduction and boot/regression
validation remain separate work.

## Planned common host and native format

Status: shared serializer, native object writer and complete host assembler
implemented; initial linker and NONSEG executable loading implemented.
Whole-system migration remains pending; see [the linker](ldz8.md).

Use the documented ZEUS **s.out** format for both relocatable objects and
executables, covering NONSEG and segmented programs with combined or separate
I/D. The intended final system uses this common format family across addressing
modes. Portable archives retain their container format; their object members
change. Adopting s.out does not adopt PLZ/ASM source syntax or the ZEUS calling
convention.

Build the same asz8k and ldz8 sources for the host and for V7:

```text
Host compiler -> host-built asz8k -> host-built ldz8
Native PCC    -> native asz8k     -> native ldz8
```

Both paths must serialize the same target bytes explicitly, independent of
host integer widths, byte order and structure padding. For identical assembly
input and link settings, host and native objects and linked outputs must match
byte for byte. Compiler assembly syntax must be aligned with asz8k, and the
kernel's existing GNU assembly path must migrate too. Changing only native
output would leave the bootstrap and native build paths inconsistent.

The recovered reference files are in the Zilog_S8000 checkout:

- `tapes/extracted/install-3.21/files/file-007/include/s.out.h`
- `tapes/extracted/install-3.21/files/file-008/man/man5/a.out.5`

They describe the descriptor, segment table, segment-associated symbols and
relocations for offsets, segment values, short segmented addresses and relative
references. Audit documentation/header differences and historical table limits
against recovered objects before claiming format compatibility. The format's
word-for-word relocation information increases relocatable object size. No
reusable ZEUS assembler/linker source has been established; this plan implements
the documented format in our tools.

### Implementation sequence

1. Add shared explicit format readers/writers and direct asz8k s.out emission.
   The initial writer and complete host/native assembler checks are implemented.
   Extend sections/relocations as needed for the kernel migration.
   Review symbol length, source filename and predefinition-file constraints
   during further syntax integration. NONSEG PCC/az8 syntax is now accepted
   directly by `-zc`.
2. Extend ldz8 for segment placement, symbol resolution, segmented relocations,
   partial linking and range checks. The initial backend covers asz8k objects
   and NONSEG combined/split I/D. Integrate the
   kernel's fixed segmented layout. Produce the image required by the existing
   boot path and compare loaded bytes and entry/layout with the current build.
3. Add s.out loading for the kernel's currently supported user-process layouts;
   this loader is implemented. Update nm, size, strip and other object/debugging
   consumers. Startup/libc and the compiler driver have an opt-in native s.out
   rebuild trial. Migrate standard-image libraries, userland and bootstrap
   seeds through the common host/native tools.
4. Validate boot, native compilation, full native rebuilding and existing
   regressions before retiring legacy a.out production and transitional support.

Keep existing a.out support during migration so each stage has a working
bootstrap and comparison baseline. Unidot remains useful as the original
assembler oracle. Any conversion bridge is transitional, not the final build
pipeline. These steps do not implement arbitrary multi-segment user processes:
that requires separate executable-loading, memory-management and ABI work.

### Acceptance checks

- Host/native byte equality for identical assembly and link inputs, including
  symbols and relocations; compiler output equality is a separate check.
- Relocation fixtures for local/external targets, segment and offset parts,
  short/long addresses, relative references, partial links and invalid ranges.
- Existing NONSEG interoperability and original Unidot oracle coverage retained
  through migration, with equivalent s.out execution tests.
- Kernel image reproduction and boot/regression tests, followed by native
  rebuilding using the migrated tools and libraries.

The assembler writer is only part of this work. Linker semantics and validation
are the largest tasks; loader and utility migration make a unified format a
broader change than the completed native assembler port.
