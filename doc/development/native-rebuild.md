# Native Rebuild

Complete [bootstrap](bootstrap.md) through the native seed first. All commands
below run on the host from the repository root; the runners perform the actual
compilation inside Unix. `--setup` recreates the corresponding disk and resets
its results, so preserve guest work before using it.

## Compiler convergence

Build both the kernel artifacts used by the runner and its preferred Release
host driver. Rebuild both directories after emulator changes; the self-hosting
runner prefers its private driver when it exists.

```sh
cmake --build v7z8000/usr/sys/build --target kernel test_driver
cmake -S v7z8000/usr/sys -B tests/build/selfhost/host -DCMAKE_BUILD_TYPE=Release
cmake --build tests/build/selfhost/host --target test_driver
python3 tools/native-cc/selfhost.py --setup
python3 tools/native-cc/selfhost.py --summary
```

The 52-step workload builds two native generations and checks identical objects
and executables. Artifacts, logs, `convergence.json` and `memory.json` are under
`tests/build/selfhost/`. Running without options resumes; `--limit N` caps the
number of additional steps. Observed memory margins are workload measurements,
not guarantees for arbitrary inputs.

## Supporting tools and libc

```sh
python3 tools/pcc-native/build.py
python3 tools/native-cc/environment.py --setup
python3 tools/native-cc/environment.py --summary
python3 tools/native-cc/test-archives.py
```

This uses the second-generation compiler and performs 98 steps to rebuild make,
ar, yacc, compiler support tools, libc and startup, then compiler passes and
optimizer. Both compiler passes are built before either is activated. The guest `/usr/src/makefile` provides `make all`. Results, exported
executables and `hd.img` are under `tests/build/native-environment/`.
The final ten steps rebuild libc with the new native compiler and compare
all 145 archive members with the current cross-built reference.
The native compiler checks include large local arrays, structure offsets and
repeated member names, stack growth, and compilation of the unchanged V7
`pstat` and `dc` sources. The `dc` build also checks assembler storage capacity.

Running without options resumes. `--limit N` limits further steps; `--refresh`
updates staged sources while retaining guest outputs; `--from-step N` restarts
at a zero-based step. Retained outputs require care after ABI or library changes:
use a fresh setup when they are no longer compatible.
`--refresh --reset-compiler` restores the bootstrap compiler, discards retained
compiler objects and resumes at the frontend rebuild.

## Essential userland

```sh
python3 tools/native-cc/userland.py --audit
python3 tools/native-cc/userland.py --setup
```

The runner uses the preceding native make/ar/yacc outputs and the current native
compiler seed. It rebuilds the kernel/driver, builds 45 unchanged V7 commands,
rebuilds portable ar, installs them and tests command and native-project use.
Its 49 steps produce `tests/build/userland/hd.img`, logs, `results.json`,
`audit.json` and `summary.json` with source hashes and executable sizes.

Running without options resumes; `--limit N` limits further steps.
`--reuse-commands` recreates the image and reruns installation/integration while
retaining a completed prefix of command builds only when sources, libc and
startup match. Newly added commands are then built natively. It
**discards other guest changes**, just as a fresh setup does.

The installed source and command makefile are under `/usr/src/cmd`; the C,
archive and yacc integration project is under `/usr/src/demo`. See
[native development](../toolchain/native-development.md) for installed coverage.

## Full native userland

The full native rebuild passes all 193 build/install steps and the combined
runtime suite. Its checked-in makefiles cover
161 commands (including the Bourne shell), seven games, 12 libraries and
12 terminal tables. The preceding native environment and essential-userland
builds supply the seed tools.

```sh
python3 tools/userland/native.py --setup
```

The host runner stages sources and data, boots Unix, monitors native make and
saves the disk between packages. It does not invoke a host compiler, assembler,
linker, yacc or lex. It excludes the full cross-built userland outputs from the
seed. Original plot source archives are unpacked during source staging.

The actual build needs no Python inside Unix:

```sh
cd /usr/src/build
make all
make install
```

Each package has its own makefile. Native yacc and lex generate parsers and
scanners; awk's procedure-table generator is built and run natively. Libraries
are assembled with native ar. Installation follows the complete build and uses
temporary files plus rename to replace executing tools safely.
Live `mv`, `make` and shell executables retain PID-suffixed backup links:
V7 forbids removing the last link of an executing text inode. Those links can
be removed after reboot. Installation sets the required set-user-ID modes on
mkdir, rmdir and mv.
V7 `mv` reads from `/dev/null` during installation so its write-access check
cannot prompt when replacing an executing tool. The unchanged
awk generator has an unspecified normal exit value; a small native-built
wrapper rejects signals and failed exec while allowing that exit value.

Results are under `tests/build/userland-native/`: `hd.img`, per-package logs,
`results.json`, the seed manifest and, on completion, an output summary and
runtime-test log. Running without options resumes. `--limit N` limits additional
packages. `--refresh` restages recipes while preserving guest outputs; it does
not invalidate already compiled objects, so changed compiler flags or ABI
require an appropriate clean rebuild. `--setup` discards the previous disk.
After rebuilding and validating the native development environment,
`--refresh --update-toolchain` installs its exported tools on the retained disk
before resuming.

Replacing the installed yacc makes parser dependencies newer, so native make
may regenerate and rebuild those packages during installation. The install
step has a larger emulator cycle budget than individual package steps.

## Experimental Zilog assembler

On `work/native-asz8k`, after completing the native environment and full native
userland above:

```sh
python3 tools/asz8k/native.py --setup
```

This creates a separate trial disk under `tests/build/asz8k`. Host Python stages
sources and monitors execution; native Unix `make`, PCC, az8 and ldz8 build the
assembler. Its guest build directory is `/usr/src/asz8k`, where `make` builds
`asz8k` and `./asz8k -s -l seg.8ks` exercises segmented assembly.
The trial checks Unidot output against recorded original-assembler results.
`./asz8k -a -o probe.b probe.8kn` writes a NONSEG a.out object directly; native
link/run checks compare both executable modes with az8, including partial
linking. PCC still invokes az8 in the installed compiler pipeline.
`./asz8k -z -s soutseg.8ks` and `./asz8k -z soutnon.8kn` write SEG and NONSEG
s.out objects directly. The trial checks local/external segment, offset and
short-address relocations and format limits. It then builds the complete host
assembler from the same C sources and independently assembles the same inputs,
comparing objects and listings byte for byte, including cross references and
32-bit expression boundaries. Error cases run on both assemblers too.
Logs, objects, executable sizes and per-process memory profiles remain beside
the disk. Running without options resumes. `--refresh` restages sources and
retains objects only when their source and headers match; `--setup` discards the
trial disk and rebuilds everything.

To rebuild the host assembler or repeat its comparison against a completed
native trial:

```sh
make -C tools/asz8k
python3 tools/asz8k/host.py
```

The host executable and results are under `tests/build/asz8k-host`. The comparison
requires a completed native trial with matching sources; refresh that trial
after source changes.

To independently reproduce the oracle with the CPM8000 checkout and its built
hosted Z8001 emulator:

```sh
python3 tools/asz8k/oracle.py ~/Projects/CPM8000
```

See [the experimental assembler assessment](../toolchain/asz8k.md) for format
limitations and remaining kernel-build integration.
The [common host/native s.out migration plan](../toolchain/asz8k.md#planned-common-host-and-native-format)
is in progress. The assembler trial covers object emission; linker and loader
checks use the separate procedure below. Replacing the installed compiler
pipeline remains pending.

## s.out linking and execution

After the native environment and assembler trial are available:

```sh
make -C v7z8000/usr/sys/build
make -C tools/ldz8
python3 tools/ldz8/test.py
```

The host linker is `tests/build/ldz8-host/ldz8`. Use `-z` to select s.out;
supported layouts and options are in [the linker reference](../toolchain/ldz8.md).
The test stages the shared linker sources and real asz8k objects in
`/usr/src/ldz8`, compiles and links the new linker with native V7 cc, and links
the same fixtures on host and guest. Python prepares disks and checks bytes;
guest cc/ldz8 and the kernel execute the native build, link and exec operations.
Trial disks, logs, linked images and native linker sizes are in
`tests/build/sout-link`. Both NONSEG layouts execute; concurrent exec and
malformed-image probes exercise shared text and ENOEXEC before replacement.
The concurrent test holds eight children behind pipes, verifies four shared
text mappings, and repeats at 320 KiB with process/text swap traffic.
Unchanged source objects can be reused from the previous trial disk.
The existing `test-exec`, `test-split` and `test-ptrace` targets cover the
retained a.out paths. Kernel/boot pipeline migration remains pending.

## s.out native C pipeline

After building the kernel, native environment and userland test runner:

```sh
python3 tools/sout-cc/build.py
python3 tools/sout-cc/test.py
python3 tools/sout-cc/edges.py
```

The first command assembles startup and all libc members directly with the
host asz8k, then cross-builds the minimum updated cc/assembler/linker seeds.
The second boots V7 with those seeds. Guest cc runs cpp, the two PCC passes,
oz8, asz8k and ldz8. Guest make recompiles the unchanged V7 libc C sources,
assembles the machine support routines, and archives them with native ar.
The trial then rebuilds and installs asz8k, ldz8 and cc through that s.out
pipeline and reruns the C/libc checks. Host Python stages the disk, drives the
console and verifies results; it does not compile or assemble the native steps.

Artifacts and per-step logs are in `tests/build/sout-cc`. The trial compares
every native assembly object against the complete host object, including
symbols and relocations. Legacy image comparisons account for section padding;
compact byte loads can also change instruction sizes. Runtime checks cover
optimized/unoptimized C, combined/split I/D, direct assembly input, common
storage and partial links, and the 39-check libc test with startup/environment,
stdio, allocation, floating arithmetic and file/syscall operations.
The final command uses the self-rebuilt tools to check compiler controls,
exact JR range boundaries, and byte-identical SEG assembly/linking on host and
guest. An interrupted C trial can resume with `test.py --resume`; it verifies
that staged sources and plans still match before reusing completed steps.

## s.out object utilities

After completing the s.out native C pipeline above:

```sh
make -C tools/sout-utils
python3 tools/sout-utils/test.py
```

The host command builds the shared C utility sources. The test stages the
self-rebuilt s.out compiler, assembler, linker and libc, then compiles `nm`,
`size`, `strip` and the `nlist` adapter inside V7. Host Python prepares real
assembler/linker fixtures, drives the guest and compares results. It does not
compile the native replacements.

The test disk and logs are under `tests/build/sout-utils`. It checks host/native
output, identical stripped files, execution after stripping, both NONSEG
layouts, SEG inspection, partial links, mixed archives, malformed inputs and
the existing libc lookup ABI. See [object utilities](../toolchain/object-utilities.md)
for supported formats and installation scope.
If the native build passed but the check stage was interrupted, use
`python3 tools/sout-utils/test.py --reuse-build`. It checks the saved C/header
sources against the checkout before reusing the compiled utilities on a fresh
test disk.

## Complete cross-built userland

After bootstrap has built the emulated kernel and driver:

```sh
python3 tools/userland/run.py
```

This rebuilds the native compiler seed, cross-builds the full C command
and game inventory plus support libraries, runs V7 lex and the awk table generator inside Unix, builds the
remaining generated sources, then creates and checks the combined image.
Unlike the 45-command workload above, the full command compilation runs on
the host. Runtime checks and data generation run on the target.

Outputs are under `tests/build/userland-all/`: `hd.img`, individual compiler
and linker logs, `report.json`, `inventory.json`, and `smoke.log`.
The image includes manuals, learn lessons, formatter data, target-generated
spelling dictionaries, seven games and 12 support libraries. The checks also exercise
native compilation and disposable filesystem creation/checking.
See [userland coverage](../toolchain/userland.md) for unported and
machine-dependent programs.

For development after a completed build, `tools/userland/build.py NAME ...`
rebuilds selected programs; `tools/userland/test.py --setup` recreates and
tests the image. Run `tools/userland/generate.py` again after changing the
awk/beautify grammars or the structure formatter's preprocessing inputs.

## Scope

These procedures establish native toolchain and selected userland rebuilding.
They do not yet establish a host-independent kernel/boot/system rebuild.
[Historical validation](../history/implementation-steps.md#step-29-native-compiler-self-hosting)
records convergence and measured memory use at each implementation stage.
