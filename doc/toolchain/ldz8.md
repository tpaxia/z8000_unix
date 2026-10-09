# s.out linker

`tools/ldz8` builds the same s.out linker sources on the host and inside V7.
The installed linker accepts only s.out input; the old PCC a.out dispatcher has
been removed. `-z` remains an accepted compatibility spelling, with no format
switch. It uses the byte serializer shared with [asz8k](asz8k.md).

## Supported layouts and options

The backend reads unbound code, data and BSS sections, resolves eight-character
external names and coalesces sections by kind. It streams image and relocation
words instead of allocating a complete image in the native linker's 16-bit data
space. Only external symbol records remain in RAM; local records are validated
and read on demand for relocations. Executable sections are padded to 256-byte cliques, as specified by
ZEUS; partial links retain word-aligned sizes and relocation.

| Option | Effect |
|---|---|
| `-z` | Accepted compatibility spelling; s.out is always selected |
| `-o file` | Output filename; default `a.out` |
| `-i` | Separate instruction/data layout |
| `-r` | Partial link; retain unresolved externals and relocation |
| `-s` | Omit executable symbols |
| `-x` | Accepted; local symbols are already omitted |
| `-e symbol` | Entry at a defined code symbol; default offset zero |
| `-u symbol` | Introduce an unresolved external before archive scanning |
| `-lfoo` | Select needed members of portable `libfoo.a` |
| `-L prefix` | Override the library filename prefix, default `/lib/lib` |
| `-C number`, `-D number` | SEG code/data physical segment numbers, decimal; defaults 0/1 |
| `-b` | Raw machine image: contiguous code/data in the segment selected by `-C`; omit headers, symbols, relocation and BSS bytes |
| `-T number` | Raw-image text offset, decimal; default zero, must be even |
| `-M number` | Raw-image end-address limit, decimal; default 65536, includes BSS |

Raw output retains word alignment without executable clique padding. `-T`
changes relocation addresses but does not prepend bytes to the file: a boot
block linked at offset 65024 is still a 512-byte image loaded at that offset.
`-b` rejects partial and split-I/D output. Its data follows text in the same
segment; `-D` has no separate placement effect. Kernel C links as e711 s.out.

The kernel and disk-boot builders use raw linking for ROM, PSA/trap stubs,
the FPU service, board firmware and the primary disk bootstrap. FPU linking
checks the 61440-byte boundary below the stack/u-area; trap linking checks the
512-byte boundary before the C runtime entry table. See
[machine-image validation](../development/native-rebuild.md#machine-assembly-and-raw-images).

NONSEG output has one segment table entry and either e707 combined or e711
separate-I/D magic. SEG executables have bound code and data segment entries;
BSS follows initialized data in the data segment. SEG partial output keeps
three unbound logical segments, one per section kind. Output symbols refer to
segment-table indices, while instructions and entry addresses use assigned
CPU segment numbers.

Local and external offset, long segmented and short segmented relocations are
supported. Short offsets must fit 0–255 after placement. Portable archives are
rescanned until no additional member satisfies an unresolved external; rejected
members cannot modify global symbols or section placement. Undefined symbols
with a nonzero value declare common storage: final links allocate the largest
declaration in word-aligned BSS, while a data/BSS definition takes precedence.
As in original V7 `ld`, a text definition cannot replace a common variable or
cause its library member to be selected. This preserves nroff's common
`nlist` array despite libc also exporting the `nlist()` function.
Partial links retain common sizes. Referenced `_etext`, `_edata` and `_end`
symbols are supplied at the padded section boundaries. Undefined symbols
are allowed only with `-r`. Duplicate definitions, malformed/truncated inputs,
invalid indices, address-space overflow and unsupported relocations fail the
link and remove a newly created output.

## Current limits

Inputs must be s.out relocatable objects from the current assembler section
contract. The backend does not read a.out members, Unidot, bound/offset input
sections, long/debug symbols or line tables. Each SEG input
segment must contain only one section kind. Section coalescing is limited to
one code segment and one data/BSS segment; it is not a general multisegment
placement script. Relative relocation actions fail explicitly: asz8k resolves
its relative branches during assembly and does not emit those actions yet.
`-r -i` and `-r -s` are rejected.

The kernel accepts the resulting NONSEG layouts; see the authoritative
[exec loader contract](../kernel/processes-and-exec.md#sout-loading).
SEG user processes remain unsupported. Kernel C, standalone `/boot` and fixed
raw machine images use the shared tools. Default bootstrap and native rebuild
commands use s.out. [Object utilities](object-utilities.md) inspect the same
contract; their legacy reader remains temporary compatibility support.

Reproduction and validation commands are in
[native rebuild](../development/native-rebuild.md#sout-linking-and-execution).
