# Bootstrap

This is the path from a clean host checkout to a native development disk.
Commands run from the repository root. For routine incremental work, use
[build and run](build-and-run.md).

## Host prerequisites

Install Git, Make, CMake, Python 3 and a host C/C++ toolchain with C++17 support.
The repository does not provision those host packages. PCC is built from the
submodule below. CMake builds host copies of the shared native `asz8k` and
`ldz8` sources for reset/trap and FPU assembly/linking; GNU Z8000 binutils are
not required. The disk-boot builder uses the same shared tools.

## Cross tools and first boot

```sh
git submodule update --init --recursive
make -C PCC-z8000/z8000/cz8
python3 tools/native-cc/build.py
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

This direct s.out host build prepares the native compiler passes, optimizer, preprocessor,
assembler, linker, cc driver, headers and runtime library. The bootable seed is
`tests/build/native-cc-sout/hd.img`. Passing these tests establishes that the seeded
compiler runs inside Unix; self-hosting is checked in the next stage.
For disk boot in the dedicated MAME branch, follow [the MAME procedure](mame.md)
to install `/boot`, `/unix` and `/fpe` into a copy of this seed.

## Native compiler and development environment

Follow [native rebuild](native-rebuild.md) in order: compiler convergence,
supporting tools/libc, essential userland, then the full native userland rebuild.
The runners execute compilation
inside the emulated Unix system and save the guest disk between steps.

After full userland, follow the native kernel and disk-bootstrap section of the
[native rebuild procedure](native-rebuild.md#native-kernel-and-disk-bootstrap).
It builds and installs kernel, FPU service and boot artifacts inside Unix, then
boots the resulting disk. Host tools stage sources, prepare the initial source
filesystem and launch/save emulator runs. Build fixtures use console init.
For the runtime disk and original V7 login startup, follow
[multiuser startup](multiuser.md).
