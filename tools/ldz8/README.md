# Shared linker

`make -C tools/ldz8` builds `tests/build/ldz8-host/ldz8`.
`-z` selects the ZEUS s.out backend. Without it, `dispatch.c` uses the
unchanged linker source in `PCC-z8000/z8000/ldz8.c` for the transitional
PCC a.out pipeline. `soutfmt.c` is shared with asz8k, without copied codecs.

`tools/native-binutils/build.py` builds the same sources for V7, and
`tools/native-cc/environment.py` stages them for native make/cc rebuilding.
`python3 tools/ldz8/test.py` rebuilds inside V7, compares host/native link
outputs, executes NONSEG s.out programs and probes the kernel loader.
Unchanged source objects can be reused from its preceding trial disk.

See [the linker reference](../../doc/toolchain/ldz8.md) for supported options,
format constraints and remaining migration work, and
[native rebuild](../../doc/development/native-rebuild.md#sout-linking-and-execution)
for prerequisites and artifacts. Python stages test files and disks; C tools
perform assembly and linking, and the V7 kernel loads the executables.
