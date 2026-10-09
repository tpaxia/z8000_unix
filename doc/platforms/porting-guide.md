# Porting to a New Machine

The port retains original V7 upper-layer policy wherever possible. A new machine
supplies CPU/MMU mechanisms, boot support and drivers through compile-time
configuration. Only `emulated` is currently implemented; [M40 notes](m40.md)
are a feasibility study, not a working configuration.

## Source hierarchy

| Directory under `v7z8000/usr/sys` | Responsibility |
|---|---|
| `sys/` | Shared V7 scheduler, filesystem, process and syscall policy |
| `dev/` | Device drivers and reusable TTY support |
| `h/` | Kernel interfaces and data structures |
| `machine/` | CPU trap/runtime support and MMU implementations |
| `conf/` | Machine source selection, device tables and interrupt routing |
| `fpe/` | Zilog software EPU engine and Unix adapter |

Keep device registers and address translation out of shared V7 policy. Reuse
existing CPU/MMU implementations when the board meets their contracts. Fix
compiler defects in the toolchain rather than rewriting original V7 C.

## Add a configuration

Add `conf/<name>.cmake`, its configuration C file and the necessary drivers.
The [configuration reference](../../v7z8000/usr/sys/conf/README.md) lists the
source-selection variables and machine helper signatures.

```sh
cmake -S v7z8000/usr/sys -B tests/build/new-machine -DKERNEL_CONFIG=<name> -DKERNEL_HOST_TESTS=OFF
cmake --build tests/build/new-machine --target kernel
```

Replace `<name>` with the new configuration. Use a separate build directory
for each machine. A configuration selects
reset/trap assembly, ordered runtime assembly, machine C, drivers and optional
shared services. Runtime entry-table placement and unique object basenames must
follow the configuration reference. Supply a board-specific test harness before
enabling its host tests.

## Boot and CPU support

Provide reset loading, PSA placement, kernel instruction/data mappings and a
valid system stack before admitting interrupts. Implement early console output,
clock enable/acknowledgement and device-vector dispatch in the configuration.
The emulated machine loads images directly; physical ROM and disk boot require
a real loader and hardware-specific placement.

Preserve the saved-context, syscall, signal and exec contracts in
[traps and interrupts](../kernel/traps-and-interrupts.md),
[processes and exec](../kernel/processes-and-exec.md) and the
[ABI reference](../toolchain/abi.md). The software EPU currently requires segment
127 and aliases of the current kernel stack. Account for those mappings when
choosing memory layout.

Supply non-sleeping `panicpoll()` for device completion with interrupts masked,
and a CPU `panichalt()` that keeps them disabled; see
[panic flushing](../kernel/devices-and-io.md#panic-time-flushing).

## MMU and memory

The existing kernel expects NONSEG C, a fixed u-area/system-stack window at
`0xF000` and the existing context layout. Supply installed-RAM discovery,
reserved regions, allocation/copy services, user-access mappings, section
resizing and residency/swap transitions. A different pointer model or virtual
layout requires coordinated header, trap, runtime and loader changes.

Use [memory and swapping](../kernel/memory-and-swapping.md) for allocation,
protection, shared text and user-copy contracts. Invalid accesses must be
suppressed before side effects and delivered as faults where CPU recovery
expects them. Automatic stack growth requires the supported fault/backout
contract; a different MMU cannot assume arbitrary Z8001 instruction restart.

MMU operations must restore temporary mappings before admitting interrupts and
keep interrupts masked while replacing a live system stack. Swapping must
respect locked processes/text, pin transfer sources and permit resident
execution while transfers sleep. Preserve V7 scheduler and shared-text policy;
replace the physical mechanism behind it.

## Devices and filesystems

Provide `bdevsw`, `cdevsw`, `linesw`, boot root/pipe/swap devices and interrupt
routing. Add console and disk drivers under `dev/`. Retain common V7 TTY,
buffer-cache and filesystem policy. Drivers must satisfy completion, error and
residual-count contracts; raw I/O uses MMU validation and pinning.

See [devices and I/O](../kernel/devices-and-io.md). Install matching device nodes
in the filesystem prototype/image builder, including `/dev/console`, `/dev/tty`,
`/dev/null`, and root-only `/dev/mem`, `/dev/kmem` and `/dev/swap`. Supply
`membyte()` for physical RAM/kernel-data access; see the memory-device contract. Character/block major numbers are configuration choices.
Supply usable swap before the first exec: original V7 argument staging reserves
ten blocks even at boot. Root and swap must refer to the intended devices.

## Bring-up and validation order

1. Check reset, early console, image placement, stack and traps before enabling
   asynchronous sources.
2. Validate RAM discovery, reserved memory and user access, including rejected
   accesses and recovery. Verify mapping changes across context switches.
3. Bring up the clock and device interrupts, filesystem root and swap, then the
   first user exec and shell. Check masking and acknowledgement under load.
4. Exercise fork/exec/wait, pipes, signals, both executable layouts, heap/stack
   growth, protection, shared text and low-memory swapping.
5. Run applicable [regressions](../development/testing.md), then native compiler
   and userland builds as sustained system workloads. Emulator-only fault
   injection needs a corresponding board harness or hardware test method.

## Optional capabilities

Hardware single-step support is optional. Without a usable trace event, ptrace
request 9 returns EIO; software stepping is outside the plan. Bus maps, alternate
TTY disciplines and a multiplexor require their own implementations/configuration.
