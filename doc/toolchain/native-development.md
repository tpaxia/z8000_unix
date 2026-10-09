# Native Development Tools

The native environment supplies `cc`, `cpp`, two-pass PCC (`front` and `back`),
`oz8` assembly optimization, `az8`, `ldz8`, make, portable ar, yacc, installed V7
headers, libc and startup code. See [ABI and formats](abi.md) for the binary
contract and [native rebuild](../development/native-rebuild.md) for reproduction.

`cc -i` produces separate-I/D executables; `cc -O` invokes native assembly
optimization. Both combined 0407 and split 0411 programs retain 16-bit pointers.
The separate software EPU keeps its arithmetic engine out of each executable.

On `work/native-asz8k`, the [s.out C trial](../development/native-rebuild.md#sout-native-c-pipeline)
installs a PCC driver built with `SOUT`: it invokes `asz8k -zc` and `ldz8 -z`
with s.out startup/libc. The same driver accepts `-z` when built without that
default. Compiler passes and `oz8` retain their existing assembly syntax and
calling convention. `.b` object filenames and the default executable name
`a.out` are retained; their contents use the [s.out contract](asz8k.md).
`-i` selects separate I/D. Nonzero legacy `-R` relocation bases are rejected.
The standard bootstrap/userland images retain the legacy pipeline until their
build rules and object-file consumers are migrated.

The s.out trial passes 56 native build/test stages. All 146 startup/libc
assembly inputs produce byte-identical host/native objects. Native make
rebuilds every libc member from its C or assembly source; the 39-check libc
test passes with both the seed and rebuilt library, and again after asz8k,
ldz8 and cc rebuild themselves. The self-rebuilt tools also pass compiler
controls, JR boundary checks and host/native SEG object/link comparisons.
Sources on the completed disk match the checkout. The native compiler passes,
cpp and oz8 used in this trial are the previously built legacy executables.

| Self-rebuilt s.out tool | Code bytes | Data bytes | BSS bytes |
|---|---:|---:|---:|
| asz8k | 51,968 | 33,280 | 1,280 |
| ldz8 | 38,144 | 5,120 | 17,664 |
| cc driver | 17,152 | 1,792 | 3,328 |

## Archives

Native ar, make and ldz8 share portable ASCII archives. See the
[archive contract](abi.md#library-archives). The ar/make implementation supports
short names within V7's 14-character filename limit, even-byte member padding
and unindexed libraries. GNU/BSD long-name and index extensions are outside
that implementation. Archive framing does not determine CPU addressing mode;
the experimental [s.out linker](ldz8.md) can link initial SEG objects, while
full segmented compilation and execution remain unfinished.

## Installed essential commands

The userland image adds these 45 commands from unchanged original V7 sources:

```text
cat echo ls pwd mkdir rmdir ln cp mv rm chmod chown chgrp
wc grep tail sort uniq tee cmp date sleep sync kill test ed
basename comm tr rev split join dd du pr od sum touch nice time
yes cal look tsort fgrep
```

The installation also rebuilds the ported portable-archive ar. Original mkdir,
rmdir and mv run set-user-ID root and retain their real-ID permission checks
for directory link/unlink operations. `/dev/null` supplies the EOF/rathole
semantics needed by shell background commands.

Command integration checks pipelines, scripted ed, joins and comparisons,
translation, split/reassembly, byte swapping, octal dumps, checksums, pagination,
hard-link accounting, command execution and fixed-string matching.
Original od, touch, look and tsort do not explicitly return success from main;
their tests check normal termination and output/filesystem effects separately.
Their exit status remains unspecified, as in their source.

The separate [full userland image](userland.md) rebuilds the remaining portable C
commands and support libraries, including nm/strip and the encryption programs.
These builds run inside Unix, including parser and scanner generation.
The image also supplies the original formatting and language packages. Multiuser
startup and several machine-dependent ports remain unfinished.

## Object utilities on the assembler branch

The [shared object utilities](object-utilities.md) read both port a.out and
s.out. `nm` displays SEG addresses without truncation; `size` totals segment
sections; `strip` removes symbols and relocation while preserving file mode.
The libc `nlist` adapter keeps the V7 16-bit interface and rejects SEG addresses.
The replacements are compiled inside V7 and tested against the host builds in
an isolated disk. The explicit `--sout` profile installs them and the nlist
adapter through the native environment recipes; default a.out builds retain
their original utilities. See
[native utility reproduction](../development/native-rebuild.md#sout-object-utilities).

The existing bootstrap, native environment and userland image builders accept
`--sout`, using separate output directories. Bootstrap pipeline tests pass;
native cpp uses the original signed-character configuration and passes its
macro-expansion runtime check. The complete 127-stage environment rebuild passes
and exports all 17 development executables in s.out, with 147 libc members and
the final 39-check libc test validated. Essential/full userland profile rebuild
validation remains pending. Native
terminal tables preserve the original nroff data-resource format through a
converter, with an independent host/native and unchanged-reader test. See
[profile reproduction](../development/native-rebuild.md#sout-bootstrap-and-native-environment).

## Source preparation and limits

The host stages two-pass compiler headers/glue and EPU wrapper assembly. Native
make/yacc/compiler operations execute inside Unix. Successful convergence and
parser generation cover the documented workloads, not every source or grammar;
code and data/heap/stack address spaces each remain bounded by the NONSEG model.
See [historical measurements](../history/implementation-steps.md#step-30-native-development-environment).
