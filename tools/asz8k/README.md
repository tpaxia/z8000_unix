# Experimental Unix asz8k port

The original CP/M-8000 assembler builds from the same C sources on the host
and inside V7 Unix. Both independently assemble the same test sources.
It emits Unidot relocatable objects, including Z8001 segmented relocations.
The new `-a` backend emits NONSEG a.out objects directly for ldz8.
The `-z` backend writes ZEUS s.out relocatable objects for NONSEG and SEG.
The `-zc` mode reads PCC/az8 assembly directly. The s.out C trial installs it
in the compiler pipeline; the standard bootstrap images retain az8 for now.
Machine sources use `-zg` (add `-s` for SEG objects), including mixed
`.segm`/`.unsegm` encoding. Kernel and disk-boot machine images now use these
shared sources on the host and raw linking in ldz8. See
[machine assembly](../../doc/toolchain/asz8k.md#machine-assembly).

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

`src/aout.c`, `src/sout.c`, `src/soutfmt.c` and `src/pcc.c` are new port code. The imported
assembler calls the selected backend at the pass boundary, byte emission and
finalization. Outside PCC mode instruction selection is unchanged; PCC mode
also relaxes out-of-range JR branches to JP. s.out short-address output
additionally rejects offsets above 255 before the encoder truncates them.
The host `Makefile` and native `src/makefile` build the complete assembler.
`host.py` compares independent host/native assembly, including object files,
listings, cross references and error cases. `soutcheck.py` separately checks
the format serializer and independently decodes s.out layouts and relocations.
The [native C pipeline procedure](../../doc/development/native-rebuild.md#sout-native-c-pipeline)
rebuilds libc and the tools using this assembler inside V7.

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
- Host builds select `ASZ_HOST`: target values retain their 16/32-bit widths,
  instruction-table pointers use `uintptr_t`, input frames have pointer
  alignment, and a stable arena replaces native `sbrk` allocation. The native
  allocator and target ABI are retained. The internal `getline`/`valloc` names
  become `asline`/`vmalloc` to avoid modern libc collisions. Historical array
  initializers use syntax accepted by both compilers.

`tests/opcodes.8kn` preserves the supplied opcode test up to its damaged final
section declaration. The corrupt tail was replaced by `__data .sect`, the
missing `foo` definition and `.end`. `tests/biosdefs.z8k` is the minimal Z8002
FPE definitions file from the same CPM8000 checkout. The FPE source itself is
staged from the existing kernel tree without modification.

`tests/expected.json` records SHA-256 values and exact byte lengths from the
original distribution CP/M assembler, with only padding after the Unidot
object-end record removed. `oracle.py` independently regenerates and checks
those objects; it never rewrites the recorded expectations.
