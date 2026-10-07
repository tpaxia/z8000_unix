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

This uses the second-generation compiler and performs 96 steps to rebuild make,
ar, yacc, compiler support tools, libc and startup, then compiler passes and
optimizer. Both compiler passes are built before either is activated. The guest `/usr/src/makefile` provides `make all`. Results, exported
executables and `hd.img` are under `tests/build/native-environment/`.
The final ten steps rebuild libc with the new native compiler and compare
all 145 archive members with the current cross-built reference.

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

## Complete userland

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
