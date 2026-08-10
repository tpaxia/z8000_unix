# Z8000 Software Emulator

The project uses a Z8000 software emulator (`z8000_emu/`) for development and testing. The emulator supports both the Z8001 (segmented) and Z8002 (non-segmented) CPU variants and is linked as a C++ library into test drivers.

## Custom Front End

Rather than using the emulator as a standalone tool, the project links it as a library and implements a custom front end (`v7z8000/usr/sys/test_driver.cpp`). This front end can:

- Load binary images at arbitrary physical addresses in the Z8001's 8MB address space, without needing bootstrap code
- Register I/O ports for simulated devices (currently a console TTY)
- Run the CPU with a cycle limit and inspect final register state
- Capture device output for automated test verification
- Enable instruction, register, and memory tracing for debugging

This approach simplifies development considerably — the full kernel trap round-trip can be tested without a real boot sequence or hardware.

## Building

The emulator uses CMake and produces two targets: a static library `z8000` (linked into test drivers) and a standalone executable `z8000emu`.

```sh
cd z8000_emu
cmake -S . -B build
cmake --build build
```

The kernel build does not require this step separately — `v7z8000/usr/sys/CMakeLists.txt` pulls the emulator in with `add_subdirectory(z8000_emu)` and links the `z8000` library into `test_driver`. The submodule does need to be initialised first (`git submodule update --init --recursive`).

Programs run under the emulator exit via `halt`. The return value from `main()` is in R0 at halt time.
