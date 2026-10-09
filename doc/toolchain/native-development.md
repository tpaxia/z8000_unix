# Native Development Tools

The default native environment supplies `cc`, `cpp`, two-pass PCC (`front` and
`back`), `oz8`, `asz8k`, `ldz8`, make, portable ar, yacc, headers, libc and startup
code in s.out. See [ABI and formats](abi.md) and
[native rebuild](../development/native-rebuild.md).

`cc -i` selects e711 split I/D; ordinary output is e707 combined space. Both
retain 16-bit pointers. `cc -O` invokes native assembly optimization. The PCC
driver invokes `asz8k -c` and `ldz8`; compiler passes retain their existing
assembly syntax and calling convention. The separate software EPU keeps its
arithmetic engine out of each executable.

The host bootstrap builds these tools directly from source through the shared
assembler/linker, including the compiler passes, shell and init. It does not
build legacy objects first. The assembler's default output is s.out and the
installed linker has no a.out backend. Existing `--sout` command spellings remain
accepted, but there is no alternative production build profile.

The fresh bootstrap passes all ten compiler-driver workloads. The 45 unchanged
V7 essential commands have built and passed native command, make/archive/yacc
and syscall integration checks. The complete environment passes all 127 stages,
validates 17 development executables and 147 libc members, and passes all 39
final libc checks. The full-userland rebuild and runtime suite also pass;
see the reproduction procedure for inventory counts and current logs.

## Archives

Native make's `.az8.b` rule invokes `asz8k -c`; its C and yacc rules use the
same s.out compiler pipeline. Native ar, make and ldz8 share portable ASCII archives. See the
[archive contract](abi.md#library-archives). The ar/make implementation supports
short names within V7's 14-character filename limit, even-byte member padding
and unindexed libraries. GNU/BSD long-name and index extensions are outside
that implementation. Archive framing does not determine CPU addressing mode;
the [s.out linker](ldz8.md) can link initial SEG objects, while
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

## Object utilities

The default environment installs [shared nm/size/strip](object-utilities.md)
and the NONSEG libc nlist adapter. Native make and prof use the same object
reader; file recognizes s.out magic. The shared reader rejects obsolete a.out
objects. Positive regression fixtures are produced as s.out.

Native terminal tables preserve nroff's original data-resource layout. Their
16-byte prefix is part of that resource contract; they are not executables or
linker inputs. The table converter accepts a resolved data-only s.out input.

## Source preparation and limits

The host stages two-pass compiler headers/glue and EPU wrapper assembly. Native
make/yacc/compiler operations execute inside Unix. Successful convergence and
parser generation cover the documented workloads, not every source or grammar;
code and data/heap/stack address spaces each remain bounded by the NONSEG model.
See [historical measurements](../history/implementation-steps.md#step-30-native-development-environment).
