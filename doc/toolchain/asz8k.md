# Zilog assembler

`tools/asz8k` ports the CP/M-8000 Zilog assembler to V7. Host and native builds
compile the same K&R C sources. The installed assembler produces s.out by
default; it rejects `-a`. s.out is the only object writer in both builds.

[Source provenance](../../tools/asz8k/README.md).
[Build and validation](../development/native-rebuild.md#machine-assembly-and-raw-images).

## Source modes

| Invocation | Source |
|---|---|
| `asz8k file.8kn` | Original NONSEG Zilog syntax |
| `asz8k -s file.8ks` | Original SEG Zilog syntax |
| `asz8k -c file.az8` | NONSEG PCC assembly |
| `asz8k -gs file.s` | SEG machine assembly, permitting `.segm`/`.unsegm` |

`-z` remains an accepted compatibility spelling for s.out. `-o` selects an
output name; the default suffix is `.so`. `-l` produces a listing; `-x` adds
cross references. Listings, macros, conditionals, repetition and the original
instruction format tables are retained.

## s.out objects

The shared serializer writes big-endian disk fields explicitly: a 24-byte
header, 16-byte segment descriptors, initialized bytes, one relocation word per
image word and 14-byte symbol records. It does not write host C structures.

Sections are `__text`, `__data` and `__bss`, with word alignment. NONSEG objects
use one segment and combined section-relative addresses. SEG objects give each
present section an unbound logical segment. Final placement belongs to
[ldz8](ldz8.md); executable sections acquire 256-byte loading alignment there.

Word-offset, short-segmented and long-segmented relocations support local and
external targets. Long addresses carry separate segment and offset tags.
External names on disk retain eight-character significance. PCC/machine source
labels may contain up to 32 characters internally, so kernel labels such as
`.Lufault0` and `.Lufault1` remain distinct. Long names consume extra symbol
storage only when needed.

Absolute 32-bit constants are supported. Relocatable bytes and plain 32-bit
integer relocation have no supported s.out encoding and are rejected. Other
section names, fixed/common sections, transfer addresses, alignment above two
bytes, overlapping output and address-space overflow are rejected. C `.comm`
declarations produce undefined symbols carrying the requested common size.

## PCC assembly

PCC mode accepts `.text`, `.data`, `.bss`, `.globl`, `.comm`, `.even`, `.zerow`
and location-counter assignments. It supplies `sp` and PCC's byte-register
aliases. Indirect and literal-port output instructions accept PCC's source-first
operand order.

PCC mode relaxes out-of-range or external `jr` to `jp`, and `calr` to `call`.
Decisions use a completed layout and repeat until the layout converges. Short
local calls and jumps retain their original encodings. Relative relocations
are consequently not required for calls between kernel objects.

## Machine assembly

Machine mode accepts column-one instructions/directives, `!` comments,
`.global`, hexadecimal literals, forward `.org`, `.space` and literal I/O ports.
`.segm` and `.unsegm` change instruction encoding without changing the SEG
object format selected on the command line. Unlike PCC mode, machine mode
retains fixed branch widths; oversized relative transfers fail assembly.

Kernel ROM/traps, the Zilog FPU engine and disk primary loader use this mode.
Native assembler invocations use private, unlinked virtual-storage files, so
concurrent users and parallel host builds cannot overwrite one another's
symbol store.

## Validation

The machine-image trial assembles and links the same fixtures on the host and
inside Unix. It compares complete s.out objects and raw images, covers PCC
long local labels, external calls and both I/O forms, and rejects legacy output
and linker input. Four unchanged firmware/FPU hashes retain the previous GNU
build as a separate comparison. The primary loader changed to read s.out.

Native source rebuilding belongs to the
[native environment procedure](../development/native-rebuild.md#supporting-tools-and-libc).
