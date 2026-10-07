# Build and Run

For a fresh checkout, complete [bootstrap](bootstrap.md) first. For an existing
checkout with the cross-toolchain available, run from the repository root:

```sh
make -C tools
cmake -S v7z8000/usr/sys -B v7z8000/usr/sys/build -DCMAKE_BUILD_TYPE=Release -DKERNEL_CONFIG=emulated
cmake --build v7z8000/usr/sys/build
cmake --build v7z8000/usr/sys/build --target test
```

Use separate build directories for different machine configurations.
`--target kernel` builds guest artifacts; `-DKERNEL_HOST_TESTS=OFF` omits the
host harness. See [porting](../platforms/porting-guide.md) for configuration.

To run a specific bootable disk with the scripted emulator, change into the
kernel build directory so its ROM/kernel/EPU artifacts are found:

```sh
cd v7z8000/usr/sys/build
./test_driver -d ../../../../tests/build/userland/hd.img -i 'echo hello | cat\nexit\n' -x hello
```

Build that disk using [native rebuild](native-rebuild.md) first. The driver is
a scripted test front end, not an interactive terminal session. Its options and
persistence requirements are documented in [testing](testing.md).

## Building

The emulator uses CMake and produces two targets: a static library `z8000` (linked into test drivers) and a standalone executable `z8000emu`.

```sh
cd z8000_emu
cmake -S . -B build
cmake --build build
```

The emulated kernel configuration does not require this step separately:
`v7z8000/usr/sys/conf/emulated-tests.cmake` pulls in the emulator and links
the `z8000` library into `test_driver`. The submodule must be initialised first
(`git submodule update --init --recursive`). The `kernel` target builds guest
artifacts; test targets also build the host driver. Configure with
`-DKERNEL_HOST_TESTS=OFF` to omit the host harness entirely. See
[kernel configuration](../../v7z8000/usr/sys/conf/README.md).

Standalone compiler test programs exit via `halt`, with the return value from
`main()` in R0. Unix programs exit through the kernel syscall; the kernel may
HALT while idle, which the integration driver handles according to its test mode.
