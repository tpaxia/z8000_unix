# V7 Userland

The full inventory accounts for all **158 top-level command source units**.
The full native rebuild produces **161 command executables, seven games,
12 libraries and 12 nroff terminal tables** inside Unix, including the Bourne
shell. The command count includes support programs and tools that still
need machine integration; it is not a claim that every command has been tested
in use. The separate essential-userland workload builds 45 unchanged commands
inside Unix.

Run the [full native rebuild](../development/native-rebuild.md#full-native-userland)
to produce `tests/build/userland-native-sout/hd.img`. The default s.out rebuild
and runtime suite pass. `summary.json` records output hashes and segment sizes;
the guest makefiles list sources and native generation steps. The native
rebuild procedure records stage and output counts and the runtime proof.

The [cross-build comparison](../development/native-rebuild.md#complete-cross-built-userland)
produces `tests/build/userland-all/hd.img`, with 160 commands (excluding the
separately built shell). Its `report.json` records source lists and sizes;
`inventory.json` accounts for every source unit, replacement and unported program.

## Built command families

| Family | Coverage |
| --- | --- |
| Files and text | Original single-file C utilities, sed, diff helpers, find, tar, ed and awk |
| Languages and generators | bc, dc, lex, yacc, make, m4, ratfor, structure/beautify and both lint passes |
| Documents | nroff, troff, eqn, neqn, tbl, refer and its indexing/search helpers, spell and its hash-table generators |
| Numeric and plotting | graph, spline, units, Tektronix and TI plotting filters, vplot |
| Accounts and administration | init, getty, login, password/group tools, cron/at, accounting, filesystem creation/checking, dump/restoration tools |
| Communications | mail, encrypted mail, cu and the UUCP programs |
| Development inspection | nm, strip, size, file and prof |

Libraries are `libm`, `libmp`, `libln`, `libplot`, `libt300`, `libt300s`,
`libt4014`, `libt450`, `libvt0`, `libdbm`, `libF77` and `libI77`, built from original C sources. The plot
sources are extracted from the original PDP-11 source archives; output
libraries use the port's portable archive format.

The image includes original shell wrappers, manual pages, units data,
formatter macros/fonts and extracted learn lessons. Nroff terminal tables
and spelling hash tables are generated for the target. The spelling word
list comes from the original distribution.

The seven source-built games are arithmetic, backgammon, fish, fortune,
hangman, quiz and wump. Quiz and fortune data are installed with them.

## Format and runtime adaptations

`nm`, `size`, `strip` and libc `nlist` use the shared s.out reader; `nm` also
reads portable archives. `prof` uses the same reader, and `file` recognizes s.out
and portable archives. The shared reader rejects obsolete a.out objects; see
[object utilities](object-utilities.md).
Lint uses 16-bit alignment for long and floating types on Z8000.
Cpp selects V7's signed-character table layout for Z8000. It retains complete
macro names, supports `#error`, and corrects unary-expression and hexadecimal
evaluation; see the [source comparison](../development/v7-compatibility.md).
An original dc initialization loop now terminates its symbol free list at the
last entry instead of writing beyond the array.

Syscall wrappers occupy individual libc archive members. This allows commands
to define unrelated globals named `lock`, `acct` or `utime` without colliding
with unused wrappers. Startup exports assembler `start`, avoiding a collision
with learn's C `start()`. Libc supplies mount, umount and dup2 interfaces.

Compiler fixes cover returned-structure member access and arguments, union
initialization, V7 pointer/integer bitwise assignment, unsigned integer
typedefs, long compound operations and private frame-label names. These fixes
preserve the affected command sources. Structure returns use
[caller-owned result storage](structure-return-abi.md); affected callers and
callees require a matching rebuild.

The kernel has 64 in-core inode entries and 64 open-file entries so the original
spell pipeline can run. Process capacity remains 16. These are configuration
sizes; the V7 file and inode algorithms are unchanged.

Kernel inspection tools `ps`, `pstat`, `dmesg` and `iostat` build and install
natively. Their memory-device and live-kernel contracts are described in
[devices and I/O](../kernel/devices-and-io.md). `ps k` inspects saved kernel RAM/swap. Native [adb](adb.md) provides Z8000
tracing and core inspection; the port-specific savecore utility recovers
[kernel-written crash dumps](../development/crash-dumps.md).

## Remaining machine work

| Program | Remaining work |
| --- | --- |
| bas, roff, factor, primes | Original implementations are PDP-11 assembly. They require Z8000 implementations; nroff already provides the newer formatter. |
| f77 | Original backend emits PDP-11 code and depends on the Ritchie compiler's second pass. It needs a Z8000 backend and runtime integration. The F77/I77 runtime libraries build and have C-driven runtime checks. |
| chess | Move generation and control contain PDP-11 assembly and need porting. |
| init/getty/login | Original init is installed as `/etc/init.v7`; boot still uses console init. Multiuser startup and account/device configuration remain. |
| UUCP, tape/printer tools and device plotting | Built, but physical-device and site configuration have not been exercised. |

The original cc, ld and binary-archive converter are replaced by the native
Z8000 tools. PDP-11 ranlib indexes are not used: ldz8 reads unindexed portable
archives.

The distribution has no game sources for banner, bcd, bj, checkers, cubic,
maze, moo, ppt, reversi, ttt or words1. The ching and words scripts depend on
unavailable programs. These are not installed. The original PDP-11 libfpsim
is replaced by the software EPU layer.

## Runtime coverage

The combined-image checks exercise awk arithmetic, large bc/dc arithmetic,
sed/expr/egrep, diff, find, tar round trips, m4, nroff, eqn/neqn, tbl through
nroff, deroff, cb, lint, structure/beautify, spelling dictionaries and the
complete spell pipeline, graph/spline output, manuals, native compilation,
nm/archive inspection, strip/exec, nlist, dup2 and multiple-precision arithmetic.
Additional checks cover DBM store/fetch/delete, Fortran math/string operations
and internal formatted I/O, fortune data, and non-root directory/move and
syscall permissions. Filesystem checks create and inspect a disposable filesystem file with mkfs,
icheck, dcheck and ncheck. Physical-device operations and multiuser sessions
are not covered by these checks.
