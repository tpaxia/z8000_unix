# Kernel Overview

The kernel keeps shared V7 services in `sys/`, reusable device/TTY code in
`dev/`, interfaces in `h/`, Z8000 and MMU mechanisms in `machine/`, and machine
selection/device tables in `conf/`. The separate software EPU is under `fpe/`.
The build selects and links implementations; there is no runtime driver loader.

| Subject | Current reference |
|---|---|
| CPU entry, interrupts, software EPU | [Traps and interrupts](traps-and-interrupts.md) |
| Mapping, user access, shared text, swapping | [Memory and swapping](memory-and-swapping.md) |
| Exec, signals, core, tracing, process services | [Processes and exec](processes-and-exec.md) |
| Device tables, TTY, buffered/raw I/O | [Devices and I/O](devices-and-io.md) |
| Calling convention, headers and archives | [ABI](../toolchain/abi.md) |
| New board or MMU | [Porting guide](../platforms/porting-guide.md) |

The [source-adjacent configuration reference](../../v7z8000/usr/sys/conf/README.md)
defines exact selection variables and machine interfaces.

## Boot Flow (V7 Kernel)

The emulated ROM installs the PSA at segment 0, offset 0x1000, establishes
system/user stack state and enters the NONSEG system-mode boot entry. Machine
startup clears kernel BSS and calls `main()`.

`main()` initializes process 0 and its MMU mapping, then calls `devinit()` and
`swapinit()`. The emulated configuration selects disk minor 0 for root/pipes
and minor 1 for swap. It starts the clock, initializes clists and buffers,
mounts root, obtains the root directory and opens `/dev/console` as descriptors
0, 1 and 2.

`newproc()` creates process 1. The child establishes its user mapping, copies
`icode`, and returns through machine startup into user mode. `icode` executes
`/etc/init`, which currently starts the console shell. Process 0 enters V7's
`sched()` swapper; its sleeps dispatch resident processes through `swtch()`.
