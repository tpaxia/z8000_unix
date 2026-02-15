# Z8000 Software Emulator

The project uses a Z8000 software emulator (`z8000_emu/`) for development and testing. The emulator supports both the Z8001 (segmented) and Z8002 (non-segmented) CPU variants and is linked as a C++ library into test drivers.

## Custom Front End

Rather than using the emulator as a standalone tool, the project links it as a library and implements a custom front end (`kernel/test_driver.cpp`). This front end can:

- Load binary images at arbitrary physical addresses in the Z8001's 8MB address space, without needing bootstrap code
- Register I/O ports for simulated devices (currently a console TTY)
- Run the CPU with a cycle limit and inspect final register state
- Capture device output for automated test verification
- Enable instruction, register, and memory tracing for debugging

This approach simplifies development considerably — the full kernel trap round-trip can be tested without a real boot sequence or hardware.

## Standalone C Tests

The `tests/run_test.sh` script provides a simpler path for testing ACK-compiled C programs. It compiles a C source file with ACK, prepends a reset vector, and runs the result on the emulator:

```sh
cd tests
./run_test.sh test_add.c                  # Z8001 (default)
./run_test.sh test_add.c -p z8002         # Z8002 non-segmented
./run_test.sh test_add.c -p z8002 -t      # with instruction trace
```

Programs exit via `halt`. The return value from `main()` is in R0 at halt time.
