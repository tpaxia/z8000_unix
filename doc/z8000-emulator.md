# Z8000 Software Emulator

The project uses a Z8000 software emulator (`z8000_emu/`) for development and testing. The emulator supports both the Z8001 (segmented) and Z8002 (non-segmented) CPU variants and is linked as a C++ library into test drivers.

## Custom Front End

Rather than using the emulator as a standalone tool, the project links it as a library and implements a custom front end (`emu/test_driver.cpp`). It lives outside `v7z8000/` because it is host C++ modelling the machine, not Unix source — keeping it separate means everything under `v7z8000/` stays a diff against the V7 baseline.

This front end can:

- Load binary images at arbitrary physical addresses in the Z8001's 8MB address space, without needing bootstrap code
- Model the paged MMU (128 segments x 32 pages x 2KB) with the UPAGE and WPAGE remap ports
- Register I/O ports for simulated devices: console TTY, IDE/ATA hard drive, RAM disk DMA controller
- Raise interrupts on the CPU — NVI for the clock tick, VI for disk completion and console receive
- Run the CPU with a cycle limit and inspect final register state
- Capture device output for automated test verification
- Enable instruction, register, and memory tracing for debugging

### Raising interrupts

Devices signal the CPU with `pulse_input_line(line, vector)`, which latches an
interrupt request without holding the line asserted. This matters: NVI and VI
are level sensitive, and `CHANGE_FCW` re-latches a pending request whenever the
handler's `IRET` re-enables NVIE or VIE. A device that held the line would
therefore re-enter its own handler forever. Use `set_input_line()` only for a
source that genuinely holds a level until serviced.

### Running tests

`test_driver` runs the boot test by default: it types `echo hello | cat` and `exit` at the shell and requires the exact console transcript. Options run something else under the same kernel:

| Option | Meaning |
|--------|---------|
| `-c <cycles>` | cycle limit (64-bit) |
| `-d <image>` | hard disk image to boot from, instead of `hd.img` |
| `-i <text>` | console input; `\n` written as two characters is a newline |
| `-x <text>` | pass if the console output contains this text, the system comes to rest and there is no panic |
| `-t`, `-r`, `-m` | instruction, register and memory traces |
| `-w <marker> -I <text>` | after initial input, wait for output containing the marker plus 100 ticks, then type a second input (`\n` is decoded) |
| `-n <ticks> -M <marker>` | measure exactly this many clock pulses after the marker (default `# `); keep running through idle HALTs |

The kernel build wraps these as `cmake --build build --target test`,
`--target test-libc`, and `--target test-preempt`. The last runs CPU-bound
scheduling/signal checks and a console-wakeup check with delayed input.

The final report includes generated clock ticks, actual NVI dispatches,
merged pulses (a pulse arriving while NVI is already pending), and the final
pending bit. It checks `generated = accepted + merged + pending`.
Measurement mode also accounts for the pending bit at both boundaries;
`-x` still requires its text, but a final HALT is not required. Reaching the
cycle limit before completing the sample is a failure. See
[interrupt-masking.md](interrupt-masking.md) for measured workloads.

Ticks are
injected after each slice requested as 5,000 cycles; instruction overshoot
and early HALT mean this count cannot be inferred from total cycles alone.

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
