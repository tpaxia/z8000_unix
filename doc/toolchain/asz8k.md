# Experimental Zilog Assembler Port

The `work/native-asz8k` branch contains a native V7 port of the CP/M-8000
`asz8k` assembler. It is an evaluation of the assembler needed for segmented
kernel components, not a replacement for the installed PCC toolchain.
[Build procedure](../development/native-rebuild.md#experimental-zilog-assembler).
[Source provenance and port changes](../../tools/asz8k/README.md).

## Verified behavior

Native PCC compiles all 16 C files inside Unix; native az8 and ldz8 assemble
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
controlled exhaustion of the 16-bit disk-backed symbol store. All 50 native
build/test steps pass, including the a.out interoperability checks below. This is not an exhaustive instruction-set validation.

The native executable has 43,340 bytes of text, 27,540 bytes of initialized
data and 1,072 bytes of BSS. During full FPE assembly to a.out, the observed
maximum break is 46,080 and minimum stack pointer is 64,988, leaving 18,908
bytes between them. These are workload measurements, not arbitrary-input
guarantees.

## Comparison with the installed assembler

| Capability | PCC az8 | Ported asz8k |
|---|---|---|
| Native V7 execution | Established system tool | Tested on this branch |
| Addressing | NONSEG | NONSEG and segmented |
| Object format | Port a.out, read by ldz8 | Unidot, or NONSEG port a.out with `-a` |
| Segmented relocation | No dedicated representation | Short address, long address, offset |
| EPA floating mnemonics | Not provided | Present and tested |
| Source facilities | Compiler-oriented directives | Macros, conditionals, repetition, sections, listings, cross-references |
| Storage | Heap-based symbol/branch records | Physical opcode tables and disk-backed symbol/macro store |

## NONSEG a.out backend

`asz8k -a` selects direct output in the port's existing a.out format. The
backend writes the 16-byte header, text/data, relocation records and 12-byte
symbols without producing or converting a Unidot file. Without `-a`, the
original Unidot backend remains available for oracle comparisons.

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
specifications are useful references for a future segmented backend.

## Remaining integration

The CP/M pipeline is `asz8k -> Unidot -> xcon -> x.out -> ld8k`. The checkout
contains C sources for asz8k and ld8k, but only an executable for xcon.
The Unix port bypasses that pipeline for NONSEG a.out output. Segmented
assembly still emits Unidot; the port does not invoke the CP/M converter or
claim to produce a linked kernel image.

The planned segmented backend uses ZEUS s.out, as described below. It is not
implemented. Existing ldz8 integer relocations alone do not establish general
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

Status: agreed implementation plan; not implemented or validated.

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
   Build and test the assembler on both host and V7. Review symbol length,
   source filename and predefinition-file constraints during syntax integration.
2. Extend ldz8 for segment placement, symbol resolution, segmented relocations,
   partial linking and range checks. Cover NONSEG combined/split I/D and the
   kernel's fixed segmented layout. Produce the image required by the existing
   boot path and compare loaded bytes and entry/layout with the current build.
3. Add s.out loading for the kernel's currently supported user-process layouts;
   update nm, size, strip and other object/debugging consumers. Rebuild libraries,
   userland and bootstrap seeds through the common host/native tools.
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
