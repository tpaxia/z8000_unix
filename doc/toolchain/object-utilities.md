# Object utilities

On `work/native-asz8k`, `tools/sout-utils` provides shared host/native `nm`,
`size` and `strip`, and a libc `nlist` adapter. A machine-dependent reader
decodes on-disk fields explicitly; it never reads target C structures into
host structures. The original V7 command and libc sources remain intact.
These replacements are installed in an isolated test disk. Standard bootstrap
and userland images still use the original a.out utilities.

## Formats and presentation

The reader accepts the port's big-endian 16-byte a.out header and 12-byte
symbols, or the current assembler/linker's s.out contract: 24-byte header,
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

`nm` reads portable ASCII archives, including mixed a.out/s.out members,
skipping ordinary non-object members and optional symbol indexes. Member
names are limited to fourteen characters. Long-name archive extensions,
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
It resolves a.out and NONSEG s.out names without changing callers. It clears
requested results before lookup; absent names retain zero type/value.
SEG files, malformed input and values outside the 16-bit interface return
`-1` with cleared results. A future SEG program ABI needs a distinct interface
for full addresses; this adapter does not truncate them.

The utility trial replaces `nlist` and adds its reader/codec objects to a copy
of libc with native `ar`, then tests extraction through native `cc`/`ldz8`.
The regular libc build has not yet switched to the adapter.

Reproduction is in [native rebuild](../development/native-rebuild.md#sout-object-utilities).

## Validation

The native trial passes 174 build/check commands, 113 exact host/native output
comparisons and nine byte-identical stripped files. Coverage includes both
NONSEG layouts, bound and unbound SEG records, reversed physical segment
placement, mixed portable archives, 32-bit absolute symbols, malformed input,
repeated stripping, and execution of stripped combined/split executables.
The `nlist` checks pass both when linked directly and when extracted from the
trial libc by native cc/ldz8. Emulator logs report zero absent-RAM accesses and
zero kernel stack warnings.

| Native s.out utility | Code bytes | Data bytes | BSS bytes |
|---|---:|---:|---:|
| nm | 22528 | 1536 | 1280 |
| size | 18176 | 1280 | 1280 |
| strip | 19456 | 1536 | 1280 |
