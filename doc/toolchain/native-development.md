# Native Development Tools

The native environment supplies `cc`, `cpp`, two-pass PCC (`front` and `back`),
`oz8` assembly optimization, `az8`, `ldz8`, make, portable ar, yacc, installed V7
headers, libc and startup code. See [ABI and formats](abi.md) for the binary
contract and [native rebuild](../development/native-rebuild.md) for reproduction.

`cc -i` produces separate-I/D executables; `cc -O` invokes native assembly
optimization. Both combined 0407 and split 0411 programs retain 16-bit pointers.
The separate software EPU keeps its arithmetic engine out of each executable.

## Archives

Native ar, make and ldz8 share portable ASCII archives. See the
[archive contract](abi.md#library-archives). The ar/make implementation supports
short names within V7's 14-character filename limit, even-byte member padding
and unindexed libraries. GNU/BSD long-name and index extensions are outside
that implementation. Archive framing does not determine CPU addressing mode;
full segmented compilation/linking remains unsupported.

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

## Source preparation and limits

The host stages two-pass compiler headers/glue and EPU wrapper assembly. Native
make/yacc/compiler operations execute inside Unix. Successful convergence and
parser generation cover the documented workloads, not every source or grammar;
code and data/heap/stack address spaces each remain bounded by the NONSEG model.
See [historical measurements](../history/implementation-steps.md#step-30-native-development-environment).
