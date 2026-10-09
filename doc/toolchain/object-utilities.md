# Object utilities

On `work/native-asz8k`, `tools/sout-utils` provides shared host/native `nm`,
`size` and `strip`, and a libc `nlist` adapter. A machine-dependent reader
decodes on-disk fields explicitly; it never reads target C structures into
host structures. The original V7 nm/size/strip and nlist sources remain intact.
The default native environment installs these replacements and the nlist
adapter. The shared reader accepts only s.out; see [formats](abi.md).

## Formats and presentation

The reader accepts the current assembler/linker's s.out contract: 24-byte header,
16-byte segment records and 14-byte symbols. It accepts NONSEG combined/split
I/D, SEG objects and bound SEG executables. See [the linker](ldz8.md) for
placement and relocation semantics. SEG execution remains a separate kernel
limitation; inspecting or stripping a SEG executable does not execute it.

`nm` retains V7's `-goprun` selection, ordering and archive presentation.
NONSEG values use the original six-digit octal presentation. SEG text/data/BSS
symbols use `segment:offset`, in octal: the segment is a physical number for
bound output, or a logical number for relocatable input. Absolute symbols
retain their full 32-bit value. Numeric sorting compares segment then offset.
`-p` streams records; sorted output allocates a bounded table and reports an
error if it cannot fit the native heap.

`nm` reads portable ASCII archives containing NONSEG and SEG s.out members,
skipping ordinary non-object members and optional symbol indexes. Obsolete
0407/0410/0411/0405 objects are errors, including when encountered in archives.
Member names are limited to fourteen characters. Long-name archive extensions,
long/debug symbols and line records are unsupported.

`size` sums text, data and BSS across the segment table, preserving V7's
decimal and octal total presentation. It operates on individual object files.

`strip` validates the header, table, file length and supported symbol records
before rewriting. It copies the header, segment table and initialized image
through a temporary file, removes symbols and relocation, and preserves the
existing file's permissions. For s.out it sets the relocation-stripped flag.
As with original V7 strip, stripping a relocatable object removes information
needed for subsequent linking. Repeated stripping leaves identical bytes.
It operates on individual files, including SEG files, rather than archives.

## libc nlist

The adapter retains the public V7 `struct nlist` layout and return convention.
It resolves NONSEG s.out names without changing callers. It clears
requested results before lookup; absent names retain zero type/value.
Obsolete a.out images, SEG files, malformed input and values outside the
16-bit interface return `-1` with cleared results. A future SEG program ABI needs a distinct interface
for full addresses; this adapter does not truncate them.

The utility trial replaces `nlist` and adds its reader/codec objects to a copy
of libc with native `ar`, then tests extraction through native `cc`/`ldz8`.
The default native environment builds and installs the adapter through its
libc makefile. The adapter and its reader/codec precede the original V7
stdio members in libc. Their stdio dependencies must be resolved before
the dummy cleanup in `fakcu` is considered; the original V7 members retain
their relative order.

## Other format consumers

The `SOUT` builds of V7 make and prof use the same reader for archive-symbol
dependencies and NONSEG profiling symbols. Their existing timestamp selection
and histogram/report policy remain unchanged. Make supports symbol lookup in
mixed portable archives, including SEG objects. Prof rejects SEG programs;
its histogram and the process ABI remain 16-bit. V7 file gains conditional
s.out magic recognition. Without `SOUT`, these commands retain their existing
a.out behavior. These are machine-format adaptations, not compiler workarounds.

Native regression checks exercise make's `archive((symbol))` dependencies
with NONSEG and SEG s.out, classify s.out with file, and produce a prof report
from a s.out executable and V7 histogram. They require rejection of obsolete
archive symbols by make and obsolete profile images by prof. Original V7 prof
reports bad format with exit status zero; the test checks its diagnostic and
empty report. Standalone disk boot uses s.out.

Reproduction is in [native rebuild](../development/native-rebuild.md#sout-object-utilities).

## Validation

The native utility trial passes 187 build/check commands, 105 exact host/native
output comparisons and eight byte-identical stripped files. It also checks
24 obsolete-format utility rejections: four magics as direct files and archive
members, each through nm, size and strip. Rejected files retain their bytes and
mode 0751; nlist rejects all eight fixtures and clears previously filled results.
Coverage includes both
NONSEG layouts, bound and unbound SEG records, reversed physical segment
placement, mixed portable archives, 32-bit absolute symbols, malformed input,
repeated stripping, and execution of stripped combined/split executables.
The `nlist` checks pass both when linked directly and when extracted from the
trial libc by native cc/ldz8. Emulator logs report zero absent-RAM accesses and
zero user-stack growth warnings.

These sizes are the trial builds. The native environment records its installed
sizes in `summary.json`.

| Trial s.out utility | Code bytes | Data bytes | BSS bytes |
|---|---:|---:|---:|
| nm | 22784 | 1536 | 1280 |
| size | 18432 | 1280 | 1280 |
| strip | 19712 | 1536 | 1280 |
