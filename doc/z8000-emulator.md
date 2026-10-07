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
| `-o <image>` | save the final guest HD contents, including failed runs; the guest must call `sync()` to flush filesystem buffers |
| `-P <file.tsv>` | observe user stack pointers at instruction-space word reads and successful `brk` calls, grouped by executed program |

The kernel build wraps these as `cmake --build build --target test`,
`--target test-libc`, `--target test-preempt`, `--target test-signal`,
`--target test-tty`, `--target test-split`, and `--target test-fpe`. The split-space target checks
0411 loading, separate instruction/data mapping, fork/exec, and linker limits.
The preemption target runs CPU-bound scheduling/default-signal checks and a
console-wakeup check with delayed input. The signal target checks caught
handlers and context restoration, waiting for its completion marker so idle
HALTs during pending alarms do not terminate the test early.

The driver also requires `fpe.bin`, built with the kernel and loaded into
reserved segment 127. Its upper two pages alias the current kernel stack
through UPAGE. The floating-point engine executes guest Z8000 instructions;
there is no host floating-point shortcut. `test-fpe` covers the arithmetic
runtime and process/signal isolation in combined and split I/D executables.

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

The emulated kernel configuration does not require this step separately:
`v7z8000/usr/sys/conf/emulated-tests.cmake` pulls in the emulator and links
the `z8000` library into `test_driver`. The submodule must be initialised first
(`git submodule update --init --recursive`). The `kernel` target builds guest
artifacts; test targets also build the host driver. Configure with
`-DKERNEL_HOST_TESTS=OFF` to omit the host harness entirely. See
[kernel configuration](../v7z8000/usr/sys/conf/README.md).

Standalone compiler test programs exit via `halt`, with the return value from
`main()` in R0. Unix programs exit through the kernel syscall; the kernel may
HALT while idle, which the integration driver handles according to its test mode.

## Kernel access-fault tests

The kernel test driver supports `-F r:HEX`, `-F w:HEX`, and `-F u:HEX` with
`-w marker -I input`. Once the marker appears, an access touching that user-bank
offset is denied and raises the CPU's SEGTRAP. Modes r/w restrict injection to
kernel segmented reads/writes; u restricts it to user-mode accesses. The failed
bus access is suppressed. Kernel/ROM/EPU-service banks are excluded. This test
option leaves normal user-bank mappings unchanged and reports the denied-access
count. Run `cmake --build tests/build/kernel-config --target test-fault` for
range, copy recovery, exec, signal-stack and direct-user-fault tests in both
executable layouts. Expected-text runs stop at idle HALT only after their verdict
appears, or at the configured cycle limit.

## User memory profiling

`-P report.tsv` observes user stack minima and successful break requests,
recording executable names at successful exec transitions. It recognizes both
V7 exec (11) and environment-aware exec (59), preserves the current record on
failed exec, and finishes records on exit. `test-abi` checks that boot exec and
libc execve both produce records. These are workload observations, not memory
bounds for arbitrary inputs.

After changing the driver or profiler, rebuild each harness used for tests.
In particular, native self-hosting prefers `tests/build/selfhost/host/test_driver`
when present, and the development-environment runner uses that driver. See the
[ABI rebuild sequence](kernel-technical-reference.md#user-program-startup).

## Installed RAM size

`test_driver -R KiB` selects installed low RAM in even KiB increments from
128 to 8192 (default 8192). Read-only port 0x00BA reports the number of 2 KB
frames to the kernel. The backing vector covers the bus address space, but
MMU accesses to absent physical RAM are suppressed and raise SEGTRAP; they
cannot wrap into available RAM. The dedicated EPU bank at 0x7f0000 remains
available independently, and its stack pages still alias the current u-area.

The kernel reserves 192 KiB for ROM/kernel and allocates user text, data and
stack as page-rounded extents, plus a 4 KiB u-area per process. `test-memory`
exercises both layouts at 320, 322, 384 and 8192 KiB, failed allocation/exec,
heap growth/shrink, invalid gaps and shell workspace crossing page boundaries.
PAGESEL (0xBC) and PAGEFRAME (0xBE) program mappings; user banks start unmapped.

Reports distinguish `Absent RAM accesses` from `Unmapped accesses`. Both raise
SEGTRAP, but the latter includes expected user gap faults and invalid-pointer
tests. Successful ordinary workloads should report zero for both. Stack probes intentionally produce warnings and supported gap faults. Shared text
has read-only protection; private data remains writable. See the [memory contract](kernel-technical-reference.md#physical-memory-sizing-and-resource-maps).

### Shared text, faults and swap device

`-S KiB` creates the dedicated, ephemeral ATA secondary unit used for swap
(default 4096 KiB; maximum 16000 KiB). Zero disables the device, but the
V7 exec argument reservation then panics at boot with `Out of swap`. `-o` saves only
the root unit. Swap traffic never uses the root filesystem's blocks. Final
statistics report swap sectors read/written, peak simultaneous read-only text
mappings, protection faults and stack warnings.

The kernel now uses split I/D; rebuild `kernel.bin`, `handler.bin`,
`handler-data.bin`, ROM and test_driver together. RAM below the 192 KiB fixed
reservation is rejected before boot; the kernel also rejects RAM insufficient
for the initial process. Usable program limits depend on contiguous allocations
and temporary growth reservations, not only total free bytes.

`test-memory` covers growth/backout, rejection of unsafe read-modify-write
replay, shared text/inode write exclusion, low-RAM swapping and full swap.
See the [MMU and swap contract](kernel-technical-reference.md#stack-faults-and-protection).

### Swap scheduling fault probes

The kernel test driver accepts `-D cycles` to delay swap-unit completion
interrupts (0 by default, maximum 10,000,000 cycles), and `-E r:N` or `-E w:N`
to fail exactly the Nth swap read or write command once. Root-disk commands are
unaffected. Delays are checked at the existing 5,000-cycle clock slices. The
summary reports injected errors and user-mode samples observed while a delayed
swap interrupt is pending. The latter verifies resident execution during swap
waits; it is a scheduling observation, not a throughput measurement.

`test-memory` combines 320 KiB RAM, both executable layouts, 50,000-cycle swap
completion delays and first-read/twentieth-write errors (skipping early exec
argument writes for the output probe). Ordinary immediate-I/O, full/small-swap
and lock/trace/core regressions remain in use. Memory-only pressure probes use
six KiB of swap for the mandatory exec reservation; their process/text images
do not fit there. `test-exec` separately checks zero/undersized-swap panics.

Use `-DCMAKE_BUILD_TYPE=Release` when configuring the kernel's host test driver
for long native compiler regressions. An unoptimized host driver can exceed the
wall-clock timeout while completing within the same guest cycle budget. This
setting optimizes the host emulator, not the cross-compiled Unix kernel.
