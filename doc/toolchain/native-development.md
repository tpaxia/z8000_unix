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

The userland image adds these 26 commands from unchanged original V7 sources:

```text
cat echo ls pwd mkdir rmdir ln cp mv rm chmod chown chgrp
wc grep tail sort uniq tee cmp date sleep sync kill test ed
```

The installation also rebuilds the ported portable-archive ar. Original mkdir,
rmdir and mv run set-user-ID root and retain their real-ID permission checks
for directory link/unlink operations. `/dev/null` supplies the EOF/rathole
semantics needed by shell background commands.

Command integration includes pipelines and scripted ed. Ed's external encryption
helper is not installed/tested. Original multiuser startup and the full V7
command set are not supplied. Object tools such as nm/strip remain future work.

## Source preparation and limits

The host stages two-pass compiler headers/glue and EPU wrapper assembly. Native
make/yacc/compiler operations execute inside Unix. Successful convergence and
parser generation cover the documented workloads, not every source or grammar;
code and data/heap/stack address spaces each remain bounded by the NONSEG model.
See [historical measurements](../history/implementation-steps.md#step-30-native-development-environment).
