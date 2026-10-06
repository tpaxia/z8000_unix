# Kernel configurations

The kernel uses V7's compile-and-link model. A configuration selects machine
support and drivers; there is no runtime driver or MMU plugin framework.

| Directory | Responsibility |
|---|---|
| `sys/` | Shared kernel services, scheduler, filesystem and syscalls |
| `dev/` | Device drivers, common TTY support and character tables |
| `h/` | Kernel data structures and interfaces |
| `machine/` | Z8000 CPU support and MMU implementations |
| `conf/` | Source selection, device tables, boot devices and interrupt routing |
| `fpe/` | Separate Zilog software EPU engine and Unix entry adapter |

## Building

From the repository root, with the cross-toolchain already built:

```
cmake -S v7z8000/usr/sys -B v7z8000/usr/sys/build -DKERNEL_CONFIG=emulated
cmake --build v7z8000/usr/sys/build --target kernel
cmake --build v7z8000/usr/sys/build --target test
```

`emulated` is the default and currently the only supplied configuration.
Unknown configurations fail at configure time. Use separate build directories
for different machines. `-DKERNEL_HOST_TESTS=OFF` omits the host emulator and
test targets; `kernel` itself produces only guest artifacts. Tests explicitly
depend on the host harness.

`conf/emulated.cmake` selects:

- `KERNEL_ROM`: reset/boot assembly, assembled by GNU Z8000 binutils.
- `KERNEL_TRAPS`: PSA and trap assembly, also assembled by GNU binutils.
- `KERNEL_ASM`: ordered PCC-assembler runtime sources. The first object must
  contain the entry table linked at `0x0200`.
- `KERNEL_MACHINE_C`: CPU, MMU and configuration C sources.
- `KERNEL_DRIVERS`: driver names from `dev/`, without the `.c` suffix.
- `KERNEL_OPTIONAL_C`: optional shared services; defaults to `sys/fakemx.c`,
  the original V7 disabled-multiplexor stubs.
- `KERNEL_TEST_FILE`: optional board-specific host harness and image/test rules.

The build records the source selection so switching configurations invalidates
existing kernel outputs. Source basenames for assembly outputs must be unique;
C and assembly sources must not produce the same object path.

## Current machine boundary

`conf/emulated.c` owns `bdevsw`, `cdevsw`, root/pipe/swap device selection,
early console output, clock enabling and VI dispatch. `devintr(vector)` is
called by the common CPU entry code; the emulated configuration services disk
and console on shared vector zero. Drivers contain their own I/O registers.

`machine/krt.s`, `trap.s` and `trap.c` implement the Z8000 trap and calling
conventions, interrupt masking and user-memory access. `machine/cpu.c` holds
bootstrap code and remaining compatibility stubs; `machine/fpe.c` handles
the software EPU. `machine/emurom.s` supplies this board's reset sequence.

The current MMU implementation is `machine/paged.c` plus `machine/pagert.s`:

| Interface | Contract |
|---|---|
| `mmuinit()` | Establish process 0's initial memory description |
| `newmem(child)` / `freemem(process)` | Allocate/release u-area and mapped sections; allocation returns -1 after full rollback on exhaustion |
| `estabur(nt, nd, ns, sep, xrw)` | Validate and allocate page-rounded sections; failure preserves the old layout and accounting |
| `expand(total_clicks)` | Resize data with text, stack and u-area sizes fixed; return -1/ENOMEM on failure |
| `sureg()` | Select the current process's instruction/data mappings and user-access selectors |
| `resume(p_addr, label)` | Switch u-area/kernel-stack mapping and restore the saved continuation atomically |
| `copyuarea(child)` | Copy the current u-area, including the continuation saved before this call |
| `copyproc(parent, child)` | Copy user data and, for split executables, instruction space |
| `useracc(base, count, writing)` | Return nonzero if the complete user-data range permits the requested access; reject address wrap |

The scheduler no longer writes MMU ports or derives user-bank numbers from
process slots. The paged implementation owns those choices. Its copy routines
restore the copy window before admitting interrupts, and `resume()` keeps
interrupts masked until the new stack is valid. Its allocated sections are mapped read/write; `useracc()` checks range wrap
and every covered page, rejecting the unmapped gap. A protected MMU must
additionally check access permissions. CPU support now recovers
SEGT faults at specific user-access instructions; a board must suppress invalid
bus operations and report SEGT for that path to operate. See the
[user-copy contract](../../../../doc/kernel-technical-reference.md#shared-user-copy-policy-and-machine-helper-contract).

## Adding a machine or MMU

Add drivers under `dev/` and a `conf/<name>.cmake` source selection. Supply a
configuration C file with the appropriate device tables, boot devices, clock,
console and interrupt dispatch. Select the relevant reset/trap assembly and
MMU C/assembly implementations. Existing implementations can be shared by
configurations; a new board does not require duplicating the common kernel.

An MMU implementation must satisfy the existing kernel ABI: NONSEG kernel
code, a fixed virtual u-area at `0xF000`, the saved-context layout, and the
user-access selectors consumed by CPU support. A different virtual layout or
pointer model also requires coordinated header, trap and loader changes.
The separate EPU service currently assumes segment 127 and an alias of the
current kernel stack. This organization makes implementations selectable; it
requires each board to supply its MMU hardware contract. The emulated board
now supplies protection, stack-warning/fault latches and whole-process swapping.
Full SEG executables remain unsupported.

## Common TTY and optional multiplexor interfaces

A configuration provides `linesw` and `nldisp`. The emulated machine installs
ordinary V7 discipline zero, with no alternate disciplines. `ttioccomm()` returns
1 when it recognizes a request, including requests failing with `u_error`, and
0 without setting an error for requests a driver may handle itself. Console
fallback sets `ENOTTY`. Device close calls receive V7's `(dev, flag, channel)`;
ordinary drivers can ignore the third argument.

`fakemx.c` satisfies shared filesystem/TTY references without an active
multiplexor and makes syscall 56 return `EINVAL`. Changing `KERNEL_OPTIONAL_C`
can select another implementation, but a real multiplexor also needs its
configured device and channel lifecycle; restoring these interfaces alone does
not install one.

## Memory sizing

The emulated MMU reads installed 2 KB frame count from port 0x00BA at boot.
It seeds V7's resource map above the 192 KiB ROM/kernel reservation and below
the EPU bank. A different machine must supply its own RAM discovery and
reservation policy; generic allocation code stays in `sys/malloc.c`. The
current core-map unit is a 2 KB frame, while process accounting uses 64-byte
clicks. USIZE is 64 clicks for the 4 KB u-area and system stack. The emulated board reports a dedicated swap unit through port 0xB2;
`swapinit` populates its block map. The MMU provides `newmem`, `freemem`, `estabur` and `expand` for section
allocation, rollback and layout changes; the emulated implementation programs
PAGESEL/PAGEFRAME (0xBC/0xBE). See the [memory contract](../../../../doc/kernel-technical-reference.md#physical-memory-sizing-and-resource-maps).

Shared text ownership lives in `sys/text.c`. A replacement MMU must provide the
physical allocation/copy and swap-transfer services it calls, together with
process residency transitions used by the scheduler. The emulated implementation
keeps those services in `machine/paged.c`; swap uses the configured block device.
The split kernel reserves a separate instruction bank and keeps PSA vectors in
ROM data space. See the kernel reference before reusing its reset/trap layout.
