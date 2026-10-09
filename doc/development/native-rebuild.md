# Native rebuild

Complete [bootstrap](bootstrap.md) first. Commands below run from the repository
root on the host; the runners stage files and execute the actual compiler,
assembler, linker, make, ar, yacc and lex inside Unix. `--setup` recreates the
corresponding disk and resets its results.

The default path uses s.out throughout. Existing `--sout` spellings remain
accepted for command compatibility; they no longer select a second profile.
Output directories retain their `-sout` suffix.

## Supporting tools and libc

```sh
python3 tools/native-cc/environment.py --setup
python3 tools/native-cc/environment.py --summary
```

The environment starts from the direct s.out bootstrap. It rebuilds make, ar,
yacc, the compiler driver, assembler/linker, object utilities, preprocessor,
compiler passes, optimizer, libc and startup under Unix. Both compiler passes
are built before either is activated. The guest `/usr/src/makefile` provides
`make all`.

Outputs and logs are under `tests/build/native-environment-sout/`; exported
programs, startup and libc are under its `native/` directory. The default s.out
workload passes all 127 stages, validates all 17 development executables and
147 libc members, and passes the final 39-check libc test. A subsequent source
refresh rebuilt the current assembler under Unix and reproduced dc's object
byte for byte; `current-assembler.json` records that check.
The compiler source refresh also passes address-to-byte conversions in both
executable layouts; `current-compiler.json` records the native rebuild and
runtime checks. PCC relocates the word address before taking its low byte,
which allows unchanged V7 sources such as refer's deliv to build as s.out.
The linker also preserves V7's distinction between commons and library text
definitions. The common-array and nlist-without-direct-stdio checks pass
in both layouts; `current-link-contracts.json` records those runs. The
libc archive keeps its nlist adapter/reader before stdio and its dummy cleanup
after stdio. `current-libc-archive.log` records its native rebuild;
`current-native-libctest.log` records the subsequent 39-check libc test.

Running without options resumes. `--limit N` limits additional steps;
`--refresh` restages sources while retaining outputs; `--from-step N` restarts
at a zero-based step. Changed ABI or incompatible library changes require a
fresh setup. `--refresh --reset-compiler` restores bootstrap compiler passes,
discards retained compiler objects and resumes at their rebuild.

## Compiler convergence

The native environment rebuilds the compiler and libc through its guest
makefiles. Earlier two-generation convergence measurements are recorded in
[implementation history](../history/implementation-steps.md). The older
standalone `selfhost.py` harness still needs its legacy fixture links migrated;
it is not part of the default s.out bootstrap procedure.

## Essential userland

```sh
python3 tools/native-cc/userland.py --audit
python3 tools/native-cc/userland.py --setup
```

This builds 45 unchanged original V7 command sources with native make and cc,
rebuilds portable ar, installs the programs and tests commands, archives, yacc,
make dependencies and both executable layouts. The 49-stage s.out workload has
passed. Outputs are under `tests/build/userland-sout/`: `hd.img`, per-step logs,
`results.json`, `audit.json` and `summary.json`.

Running without options resumes; `--limit N` limits additional steps.
`--reuse-commands` recreates the disk while retaining a verified prefix of
command outputs when their source, libc and startup match. It reruns installation
and integration checks and discards other guest changes.

The guest command makefile is under `/usr/src/cmd`; the integration project is
under `/usr/src/demo`.

## Full native userland

To run the entire bootstrap, environment and userland sequence:

```sh
python3 tools/userland/run.py
```

To rebuild userland from the completed native environment and essential disk:

```sh
python3 tools/userland/native.py --setup
```

The checked-in recipes cover 161 commands, seven games, 12 libraries and 12
terminal tables. The s.out rebuild passes all 194 build/install stages,
validates all 192 built and installed outputs, and passes the full runtime
smoke suite. The smoke run reports zero absent or unmapped RAM accesses,
protection faults and stack warnings. Its log is `smoke.log`.

The seed uses the preceding native environment and essential commands. The
host runner stages sources and data, saves the disk between packages and
monitors native execution. It does not compile userland on the host. Original
plot source archives are unpacked as source staging.

Inside Unix:

```sh
cd /usr/src/build
make all
make install
```

Native yacc/lex generate parsers and scanners; awk's procedure-table generator
is built and run natively. Libraries use native portable ar. Installation uses
temporary files and rename when replacing executing tools, and preserves extra
links while their text inodes are busy. Parser packages may rebuild after yacc
is replaced.

Outputs are under `tests/build/userland-native-sout/`. `results.json` records
completed packages; `summary.json` and `smoke.log` record inventory validation
and runtime checks after installation.
Running without options resumes. `--limit N` limits additional packages;
`--refresh` restages recipes while retaining outputs. After rebuilding the
native environment, `--refresh --update-toolchain` installs its exported tools
on the retained disk. Changed flags or ABI need appropriate object rebuilding.

Terminal-table conversion preserves nroff's original data-resource layout;
these resources are neither executables nor linker inputs.

## Machine assembly and raw images

```sh
python3 tools/kernel-asm/test.py
```

The trial cross-builds target tools from the shared C sources, then runs them
inside Unix. It assembles ROM, traps, the Unix FPU adapter and preserved Zilog
arithmetic core, board firmware, the disk block and independent addressing
fixtures. PCC fixtures cover long local labels, external calls and I/O operands.
The test also rejects legacy output/input, invalid origins, memory limits,
partial raw links, split raw links and unresolved symbols.

Host/native objects, raw images and the PCC executable are compared byte for
byte. Four unchanged firmware/FPU hashes retain the pre-migration GNU build as
an independent comparison. The disk block changed to consume s.out. Artifacts
and logs are under `tests/build/kernel-asm/`; `results.json` records exact
command and comparison counts. `--kernel-build <directory>` selects the kernel
and driver; `--host-only` omits native execution.

This test cross-builds its target tools; the environment procedure above tests
native rebuilding of their C sources. Kernel C links as `handler.sout`;
`sout2bin.py` extracts its instruction/data images. `/boot` and `/unix` also use
s.out. Complete native kernel and standalone rebuilding remains future work.

## s.out linking and execution

See [linker options](../toolchain/ldz8.md), [exec loading](../kernel/processes-and-exec.md#sout-loading)
and the machine-image trial above. SEG object inspection and linking are
supported; SEG user execution is not.

## s.out native C pipeline

```sh
python3 tools/native-cc/build.py
python3 tools/native-cc/test.py
```

The seed is `tests/build/native-cc-sout/hd.img`. The bootstrap directly builds
s.out compiler passes, tools, startup, libc, shell and init. It does not first
assemble or link legacy objects. All ten native pipeline workloads pass,
including both layouts, floating point, optimizer use, separate compilation,
compiler controls and failure cleanup.

## s.out object utilities

```sh
python3 tools/sout-utils/test.py
```

The shared reader still accepts legacy a.out for transitional inspection.
Its historical trial includes legacy fixtures that require migration before
reader retirement; see [object utilities](../toolchain/object-utilities.md).
The default native environment already installs the s.out utilities and nlist
adapter.

## s.out bootstrap and native environment

Use the default bootstrap and environment commands above. No format-selection
option is required. The remaining phaseout work is tracked in
[ABI and formats](../toolchain/abi.md).

## Experimental Zilog assembler

The assembler is now the installed production assembler. Its separate historical
host oracle can be built with `make -C tools/asz8k oracle`; that executable is
not installed in Unix. The older native multi-format trial is historical and
needs migration before it can test the current production defaults.

## Complete cross-built userland

The older cross-built package runner remains a legacy regression producer; the
native procedure above is the production path. Package source coverage is
listed in [userland](../toolchain/userland.md).

## Scope

Host Python remains responsible for source preparation, disk construction and
emulator automation. It does not replace the native compile pipeline. Multiuser
startup, native kernel/boot rebuilding and full SEG user execution remain
separate work.
