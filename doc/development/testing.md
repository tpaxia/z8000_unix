# Testing

## Regression entry points

Complete [bootstrap](bootstrap.md) first. Run from the repository root:

```sh
cmake --build v7z8000/usr/sys/build --target \
  test test-libc test-signal test-preempt test-tty test-split test-fpe \
  test-copy test-fault test-v7-interfaces test-bio test-abi test-memory \
  test-physio test-core test-ptrace test-exec test-services
python3 PCC-z8000/z8000/test/ratchet/run.py
python3 tools/export-headers.py --check
```

Use the suites relevant to a change: ABI/libc changes need libc, ABI and native
compiler checks; memory/scheduler work needs memory, faults, signals, exec and
tracing; drivers need boot, TTY or buffered/raw I/O as applicable. Native
convergence and sustained userland tests have separate, longer procedures in
[native rebuild](native-rebuild.md). Compiler baselines are reviewed evidence,
not something to accept automatically after a failure.

Rebuild every driver used by a test after harness changes. The native environment
uses `tests/build/selfhost/host/test_driver`; rebuilding only the default kernel
build directory does not update that executable.

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
[interrupt-masking.md](../history/interrupt-masking.md) for measured workloads.

Ticks are
injected after each slice requested as 5,000 cycles; instruction overshoot
and early HALT mean this count cannot be inferred from total cycles alone.

This approach simplifies development considerably — the full kernel trap round-trip can be tested without a real boot sequence or hardware.

## Kernel access-fault tests

The kernel test driver supports `-F r:HEX`, `-F w:HEX`, and `-F u:HEX` with
`-w marker -I input`. Once the marker appears, an access touching that user-bank
offset is denied and raises the CPU's SEGTRAP. Modes r/w restrict injection to
kernel segmented reads/writes; u restricts it to user-mode accesses. The failed
bus access is suppressed. Kernel/ROM/EPU-service banks are excluded. This test
option leaves normal user-bank mappings unchanged and reports the denied-access
count. Run `cmake --build v7z8000/usr/sys/build --target test-fault` for
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
[ABI rebuild sequence](../kernel/processes-and-exec.md#user-program-startup).

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
