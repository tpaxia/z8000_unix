# Experimental Unix asz8k port

The original CP/M-8000 assembler is built and exercised inside V7 Unix.
It emits Unidot relocatable objects, including Z8001 segmented relocations.
The new `-a` backend emits NONSEG a.out objects directly for ldz8.
It is not installed as the system assembler; PCC syntax integration is still
separate work.

See [the assessment](../../doc/toolchain/asz8k.md) for results and remaining
integration work, and [native rebuild](../../doc/development/native-rebuild.md#experimental-zilog-assembler)
for the build procedure.

## Source provenance

`src/` comes from `CPM8000/src/asm8k` at revision
`2d4ac7da46e2e3741fc2832a28b8cce0fb7b9f1d` of
<https://github.com/tpaxia/CPM8000>. `upstream.json` records the original file
hashes. That checkout traces its sources to the surviving `8k0583.zip`
collection and includes its maintained EPA instruction fixes. Existing source
notices are retained. CRLF and terminal CP/M padding were normalized.

`src/aout.c` is new code for the port's existing object format. The imported
assembler calls it at the pass boundary, byte emission and finalization; its
instruction selection is unchanged.

The CP/M stdio, BDOS and portability headers and `chain.c` are not imported.
The Unix port uses V7 headers and libc. Its changes are:

- Standard Unix file operations replace CP/M ASCII/binary variants.
- Successful assembly closes the output and returns instead of chaining to
  the unavailable native `xcon`; `-o` names the selected output format.
- Static stdio buffers avoid interference with the assembler's direct `sbrk`
  allocator. Listing files are closed only when actually opened.
- Missing output arguments and overlong basenames are rejected.
- Physical and disk-backed storage allocation checks reject 16-bit address
  wraparound. The disk cache does not provide unlimited virtual storage.

`tests/opcodes.8kn` preserves the supplied opcode test up to its damaged final
section declaration. The corrupt tail was replaced by `__data .sect`, the
missing `foo` definition and `.end`. `tests/biosdefs.z8k` is the minimal Z8002
FPE definitions file from the same CPM8000 checkout. The FPE source itself is
staged from the existing kernel tree without modification.

`tests/expected.json` records SHA-256 values and exact byte lengths from the
original distribution CP/M assembler, with only padding after the Unidot
object-end record removed. `oracle.py` independently regenerates and checks
those objects; it never rewrites the recorded expectations.
