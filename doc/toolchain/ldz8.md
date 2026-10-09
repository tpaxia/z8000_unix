# s.out linker

On `work/native-asz8k`, `tools/ldz8` builds a shared host/native linker.
`dispatch.c` includes the existing PCC linker unchanged for the transitional
a.out path. An explicit `-z` selects the s.out backend in `ldso.c`, using the
same byte serializer as [asz8k](asz8k.md). The installed compiler driver still
uses az8 and a.out in the standard images. The s.out C trial installs a driver
that invokes asz8k and this backend; see [native development](native-development.md).

## Supported layouts and options

The backend reads unbound code, data and BSS sections, resolves eight-character
external names and coalesces sections by kind. It streams image and relocation
words instead of allocating a complete image in the native linker's 16-bit data
space. Only external symbol records remain in RAM; local records are validated
and read on demand for relocations. Executable sections are padded to 256-byte cliques, as specified by
ZEUS; partial links retain word-aligned sizes and relocation.

| Option | Effect |
|---|---|
| `-z` | Select s.out input/output |
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
declaration in word-aligned BSS, while a strong definition takes precedence.
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
SEG user processes remain unsupported. Kernel assembly syntax, fixed-layout
kernel images and machine boot loaders still need integration before the
current a.out build can be retired. Shared host/native
[nm/size/strip and a nlist adapter](object-utilities.md) are available on this
branch; deployment to the standard bootstrap/userland images remains pending.

Reproduction and validation commands are in
[native rebuild](../development/native-rebuild.md#sout-linking-and-execution).
