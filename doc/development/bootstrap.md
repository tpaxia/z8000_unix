# Bootstrap

This is the path from a clean host checkout to a native development disk.
Commands run from the repository root. For routine incremental work, use
[build and run](build-and-run.md).

## Host prerequisites

Install Git, Make, CMake, Python 3, a host C/C++ toolchain with C++17 support,
and GNU Z8000 binutils providing the `z8k-coff` tools used for reset/trap
assembly. The repository does not provision those host packages. The historical
PCC compiler, assembler and linker are built from the submodule below.

## Cross tools and first boot

```sh
git submodule update --init --recursive
make -C PCC-z8000/z8000/cz8
make -C PCC-z8000/z8000/az8
make -C PCC-z8000/z8000/test ../ldz8
make -C tools
cmake -S v7z8000/usr/sys -B v7z8000/usr/sys/build -DCMAKE_BUILD_TYPE=Release -DKERNEL_CONFIG=emulated
cmake --build v7z8000/usr/sys/build
cmake --build v7z8000/usr/sys/build --target test
```

The boot test checks the shell prompt and `echo hello | cat`. The default disk
contains a small command set and console init. The emulator loads ROM, kernel,
handler and EPU artifacts directly; this does not build a physical boot ROM
installation procedure. Its default dedicated swap device is required by exec.

## Seed the native toolchain

```sh
python3 tools/native-cc/build.py
python3 tools/native-cc/test.py
```

This host build prepares the native compiler passes, optimizer, preprocessor,
assembler, linker, cc driver, headers and runtime library. The bootable seed is
`tests/build/native-cc/hd.img`. Passing these tests establishes that the seeded
compiler runs inside Unix; self-hosting is checked in the next stage.

## Native compiler and development environment

Follow [native rebuild](native-rebuild.md) in order: compiler convergence,
supporting tools/libc, essential userland, then the full native userland rebuild.
The runners execute compilation
inside the emulated Unix system and save the guest disk between steps.

Host tools still prepare sources, two-pass glue, EPU wrapper assembly and disk
images, and launch the emulator. Fully native kernel and boot rebuilding remains
future work. The resulting system still uses console init, not multiuser login.
