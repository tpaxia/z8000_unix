# Documentation

Start with [current status](status.md) for implemented features and remaining work.
All shell commands in the development guides run from the repository root unless
an explicit working directory is given.

| Task | Start here |
|---|---|
| Bootstrap the existing system | [Bootstrap](development/bootstrap.md) |
| Build and test an existing checkout | [Build and run](development/build-and-run.md), [testing](development/testing.md) |
| Rebuild tools inside Unix | [Native rebuild](development/native-rebuild.md) |
| Port to a new machine | [Porting guide](platforms/porting-guide.md) |
| Understand the kernel | [Kernel overview](kernel/overview.md) |
| Compare with original Unix V7 | [V7 compatibility](development/v7-compatibility.md) |

## Kernel

- [Overview and boot flow](kernel/overview.md)
- [Traps, interrupts and software EPU](kernel/traps-and-interrupts.md)
- [Memory, protection and swapping](kernel/memory-and-swapping.md)
- [Processes, exec, signals and tracing](kernel/processes-and-exec.md)
- [Devices, terminal support and I/O](kernel/devices-and-io.md)

## Toolchain

- [ABI, executable formats and archives](toolchain/abi.md)
- [Native development tools](toolchain/native-development.md)
- [Shared Zilog assembler](toolchain/asz8k.md)
- [s.out linker](toolchain/ldz8.md)
- [V7 adb](toolchain/adb.md)
- [Object utilities and nlist](toolchain/object-utilities.md)
- [V7 userland coverage and remaining ports](toolchain/userland.md)
- [Structure-return ABI and historical comparison](toolchain/structure-return-abi.md)

## Platforms

- [Common porting guide](platforms/porting-guide.md)
- [Implemented emulated machine](platforms/emulated.md)
- [Z8001-unix in MAME](platforms/z8001-unix.md)
- [MMU design background and proposals](platforms/mmu-design.md)
- [M40 feasibility notes](platforms/m40.md)

## Development

- [Bootstrap](development/bootstrap.md)
- [Build and run](development/build-and-run.md)
- [Build and run in MAME](development/mame.md)
- [Native rebuild](development/native-rebuild.md)
- [Kernel crash recovery](development/crash-dumps.md)
- [Testing and emulator diagnostics](development/testing.md)
- [V7 compatibility](development/v7-compatibility.md)

## History

These records preserve observations from their implementation stage. Old limits,
size measurements and missing-feature statements are not current specifications.

- [Implementation journal](history/implementation-steps.md)
- [Interrupt-masking investigation and clock measurements](history/interrupt-masking.md)
- [Early trap infrastructure](history/step3-trap-infrastructure.md)
- [Initial V7 kernel](history/step7-v7-kernel.md)
- [Early fork and MMU implementation](history/step8-fork-mmu.md)
- [Compiler research](history/pcc-research.md)
