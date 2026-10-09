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
standalone harness now uses the same s.out assembler/linker and seed library:

```sh
python3 tools/native-cc/selfhost.py --setup
```

The s.out harness has passed two-generation convergence with caller-owned
aggregate returns. Every compiler object and executable matches between
generations. The native front end uses 63,232 bytes of text; the back end uses
44,032 bytes.

Running without options resumes an interrupted trial. After a completed trial,
`--refresh-back` restages current back-end sources and repeats both back-end
generations and runtime probes while retaining the unchanged front end and
optimizer. Rebuild the bootstrap tools first with `tools/native-cc/build.py`.
Front-end, optimizer or runtime changes require a fresh `--setup` trial.

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

The checked-in recipes cover 164 commands, seven games, 12 libraries and 12
terminal tables. The full plan has 197 build/install stages and validates
195 built and installed outputs. The runtime smoke suite exercises the combined
image; `smoke.log` records its result and memory-access counters.

The full smoke suite passes, including factor/primes, archive and native
compilation probes, syscall permissions, libraries, spelling and filesystem
checks, with zero absent/unmapped accesses, protection faults or stack warnings.
The permissions probe checks unsupported memory minor 3; minors 0 and 1 are
the implemented root-only physical and kernel memory devices.

To restage current test inputs and rerun the suite on an existing native disk:

```sh
python3 tools/userland/native.py --refresh --limit 0
python3 tools/userland/test.py --native
```

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
links while their text inodes are busy. Repeated installation removes a stale
backup name before creating the new link, since process IDs repeat across
emulator boots. Parser packages may rebuild after yacc is replaced.

Outputs are under `tests/build/userland-native-sout/`. `results.json` records
completed packages; `summary.json` and `smoke.log` record inventory validation
and runtime checks after installation.
Running without options resumes. `--limit N` limits additional packages;
`--names NAME ...` restricts pending work to those packages. Completion is tracked
by stage name, so catalog additions do not skip new packages or repeat completed ones.
`--refresh` restages recipes while retaining outputs. After rebuilding the
native environment, `--refresh --update-toolchain` refreshes compiler tools,
startup and libc while retaining completed full-userland programs. Changed
packages, flags or ABI need appropriate object rebuilding. Native make's
Z8000 `.az8.b` rule now invokes `asz8k -c`; the portable-archive trial checks
C, yacc and assembly built-in rules together.

Terminal-table conversion preserves nroff's original data-resource layout;
these resources are neither executables nor linker inputs.

### Factor and primes

With a completed native userland disk, build the new packages and run their
dedicated installation and runtime checks:

```sh
python3 tools/userland/native.py --refresh --names factor primes
python3 tools/userland/test-numeric.py
```

Compilation and assembly run inside Unix through native make, cc and asz8k.
The test installs both programs into `/bin`, boots that disk again, and compares
guest output against independent integer calculations. It covers repeated
factors, `65537` squared, `2^55`, `2^56-1`, overflow diagnostics, streaming input,
primes across an 8000-number sieve boundary and primes above `2^32`.
V7 make's stack growth during installation is checked separately from the
program run, which reports zero absent/unmapped accesses and protection faults.
Logs and exported native executables are in `tests/build/userland-numeric/`.
The checked disk replaces `tests/build/userland-native-sout/hd.img` after success.

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
s.out. The complete source rebuild follows below.

## Native kernel and disk bootstrap

After the native environment and full userland builds:

```sh
python3 tools/native-system/build.py --setup
```

The host stages original kernel and standalone sources and the verified native
compiler, runtime and commands. It first rebuilds and installs cpp with complete
macro-name matching; MM_STACKSEL and MM_STACKBASE must remain distinct. The cpp
probe covers definition, conditional expansion and undefinition of names sharing
their first eight characters, unary expressions, hexadecimal digits and active/
inactive `#error` guards. Native make compiles every configured kernel C
source, assembles and links the kernel, traps, reset ROM and software EPU service,
and builds board firmware, sector zero and the standalone `/boot` loader.
Native sed adapts the preserved Zilog FPU source; the original V7 standalone
filesystem reader is compiled unchanged through a declaration wrapper. The
portable prefix of standalone prf.c is extracted inside Unix.

The kernel makefile uses CMake's configured source selection; there is no second
list of common services or drivers. This trial selects the `emulated` board,
shared by the standalone emulator and MAME. Inside Unix:

```sh
cd /usr/src/sys
make all
cd /usr/src/boot
make all
make install
```

The native `pack` utility patches the kernel vector reservation and boot entry,
pads the small firmware ROM, and installs the primary bootstrap using `/boot`'s
actual inode block addresses. Installation replaces `/boot`, `/unix` and `/fpe`
on the trial disk and refreshes sector zero. Its block-special device is
`/dev/hd0` (major 1, minor 0); the installer requires `/boot` on that filesystem.
Repeating installation refreshes the sector list. See the
[boot contract](../platforms/z8001-unix.md#boot) for size and layout limits.

The host then extracts the completed artifacts, checks the installed sector list
and compares kernel objects and images with the independent cross build. It
boots with `test_driver -b` and the native ROM; the emulator preloads no kernel
or FPU image. A guest C program is compiled and executed under the rebuilt kernel
to exercise long arithmetic and the software EPU. Package/installer rejection
checks and repeated installation also run under that kernel.

The verified build covers 35 kernel C sources and compares all 39 kernel
objects with the cross build. All eight exported kernel/boot artifacts match;
the disk-boot run passes nine native compiler/runtime/installer commands.

Outputs are under `tests/build/native-system/`: `hd.img`, per-stage logs,
`results.json`, `artifacts/`, `artifacts.json` and `boot.json`. Running without
options resumes; `--limit N` limits additional stages; `--verify` repeats the
artifact and disk-boot checks. `--setup` creates a fresh trial disk. Source or
configuration changes require a fresh setup. The existing cross kernel build
provides independent comparison artifacts, never native build inputs. For a
standalone-loader comparison, use the same verified native runtime archive:

```sh
python3 mame/build_rom.py --output tests/build/native-system/cross-boot \
  --libc tests/build/native-environment-sout/native/lib/libc.a
python3 tools/native-system/build.py --verify
```

Both standalone builds optimize their C objects. Comparing against the
unoptimized bootstrap libc would compare different library implementations.

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
python3 tools/userland/test-formats.py
python3 tools/userland/test-mkfs-boot.py
```

The trial builds positive fixtures as s.out and verifies obsolete a.out
rejection by nm, size, strip and nlist, including portable-archive members.
Rejected strip inputs retain their bytes and permissions. See
[object utilities](../toolchain/object-utilities.md) for coverage.

Kernel exec rejection is tested separately with complete obsolete images and
valid combined/split controls:

```sh
python3 tools/test-object-formats.py v7z8000/usr/sys/build
```

The native environment and full-userland disks install the strict reader.
`formats-readers.json` under each output directory records the targeted native
reader rebuilds. The environment retains per-step `formats-*.log` files; the
full disk retains `formats-readers.log` and the subsequent `smoke.log`.

## s.out bootstrap and native environment

Use the default bootstrap and environment commands above. No format-selection
option is required. See the current
[ABI and formats](../toolchain/abi.md).

## Assembler host/native comparison

```sh
python3 tools/asz8k/native.py --reuse-tool
python3 tools/asz8k/host.py
```

The trial uses the verified native assembler from the environment, produces
s.out, and compares objects, listings and diagnostic cases with the host build.
Use `--setup` instead of `--reuse-tool` for a fresh source rebuild of the
assembler. Both host and native builds write only s.out.

## Complete cross-built userland

The diagnostic cross-build runner also uses the shared s.out assembler/linker:

```sh
python3 tools/userland/build.py [command ...]
```

It compiles on the host and does not establish native build coverage. Old
cached executables must be rebuilt; comparison-image staging rejects their
obsolete headers. The native procedure above is the production path. Package source coverage is
listed in [userland](../toolchain/userland.md).

## Scope

Host Python remains responsible for source preparation, disk construction and
emulator automation. It does not replace the native compile pipeline. For the runtime login image, follow
[multiuser startup](multiuser.md). Full SEG user execution remains separate work.
