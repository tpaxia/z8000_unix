# Kernel configurations

For the overall workflow, see the [porting guide](../../../../doc/platforms/porting-guide.md).
This file defines the source-selection variables and machine helper contracts.

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

- `KERNEL_ROM`: reset/boot assembly, assembled by shared `asz8k -zgs`.
- `KERNEL_TRAPS`: PSA and trap assembly, also assembled by shared `asz8k -zgs`.
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

CMake also exports the ordered selection as `native-sources.txt`. The
[native kernel procedure](../../../../doc/development/native-rebuild.md#native-kernel-and-disk-bootstrap)
stages that selection and generates guest makefiles; common services and drivers
are not listed independently for the native build. Private machine headers are
staged alongside shared headers and tracked as build dependencies.

## Current machine boundary

`conf/emulated.c` owns `bdevsw`, `cdevsw`, root/pipe/swap device selection,
early console output, clock enabling and VI dispatch. `devintr(vector)` is
called by the common CPU entry code; the emulated configuration services disk
and console on shared vector zero. Drivers contain their own I/O registers.
`panicpoll()` services disk completions with interrupts masked, without sleeping
or dispatching processes. CPU `panichalt()` halts with VI/NVI disabled.
Machine `dumpinit()` discovers a reserved destination after root mount;
`panicdump()` saves a crash image after panic flushing (no-op hooks are permitted).
The emulated implementation is `machine/dump.c`, using MMU
`dumpcopy(long physical_offset, char *sector)` (0/-1) and driver
`hddump(dev, block, sector, writing)` (1 success, 0 error, -1 timeout).
Both operate on 512-byte sectors without sleeping or enabling interrupts;
see the [dump contract](../../../../doc/kernel/devices-and-io.md#kernel-written-crash-dumps).

`machine/krt.s`, `trap.s` and `trap.c` implement the Z8000 trap and calling
conventions, interrupt masking and user-memory access. `machine/cpu.c` holds
bootstrap code, exec startup and signal-frame construction; `machine/fpe.c` handles
the software EPU. `machine/emurom.s` supplies this board's reset sequence.

Shared exec calls CPU `execsize(nc, na, ne, data_bytes)` (click reservation,
zero on collision), `execstk(bno, nc, na, ne)` (zero/-1), and `execregs()`
(register/EPU reset). `execstk` stages SP in process-local storage for trap
return; it must survive sleeping file cleanup. Shared signal policy calls
`sendsig(handler, signal, &usp)` (zero/-1), which commits SP/PC only after a
complete frame copy. Filesystem policy, credential changes and signal defaults
remain in shared code.

The current MMU implementation is `machine/paged.c` plus `machine/pagert.s`:

| Interface | Contract |
|---|---|
| `mmuinit()` | Establish process 0's initial memory description |
| `newmem(child)` / `freemem(process)` | Allocate/release u-area and mapped sections; allocation returns -1 after full rollback on exhaustion |
| `estabur(nt, nd, ns, sep, xrw)` | Validate and allocate page-rounded sections; failure preserves the old layout and accounting |
| `expand(total_clicks)` | Resize data with text, stack and u-area sizes fixed; return -1/ENOMEM on failure |
| `membyte(offset, kernel, value, writing)` | Read/write one validated physical-RAM or kernel-data byte for memory devices; restore temporary maps before enabling interrupts |
| `sureg()` | Select the current process's instruction/data mappings and user-access selectors |
| `resume(p_addr, label)` | Switch u-area/kernel-stack mapping and restore the saved continuation atomically |
| `copyuarea(child)` | Copy the current u-area, including the continuation saved before this call |
| `copyproc(parent, child)` | Copy user data and, for split executables, instruction space |
| `physmap(bp, rw)` / `physunmap(bp)` | Validate and pin the complete user range after special-buffer acquisition; release the pin while preserving a pre-existing lock |
| `physio_copy(bp, off, buffer, count, writing)` | Copy through the pinned request owner's mappings, independent of the current process; reject bounds/direction violations |
| `coredump(inode)` | Write u-area, data and stack in V7 order through this machine's mappings; preserve errors and reject incomplete writes |
| `traceword(req, addr, value)` | Read/write a tracee word through its I/D mappings; enforce exclusive text ownership and invalidate stale swap copies |
| `traceuser(req, offset, value)` | CPU support reads u-area words and permits only safe register/EPU writes |
| `tracego(addr, step)` | Validate resume state; hardware stepping is optional and currently returns -1 when requested |
| `coreregs(usp)` | CPU support snapshots user registers for the core image, including the process-local SP |
| `useracc(base, count, writing)` | Return nonzero if the complete user-data range permits the requested access; reject address wrap |

The scheduler no longer writes MMU ports or derives user-bank numbers from
process slots. The paged implementation owns those choices. Its copy routines
restore the copy window before admitting interrupts, and `resume()` keeps
interrupts masked until the new stack is valid. Its allocated sections are mapped read/write; `useracc()` checks range wrap
and every covered page, rejecting the unmapped gap. A protected MMU must
additionally check access permissions. CPU support now recovers
SEGT faults at specific user-access instructions; a board must suppress invalid
bus operations and report SEGT for that path to operate. See the
[user-copy contract](../../../../doc/kernel/memory-and-swapping.md#shared-user-copy-policy-and-machine-helper-contract).

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
PAGESEL/PAGEFRAME (0xBC/0xBE). See the [memory contract](../../../../doc/kernel/memory-and-swapping.md#physical-memory-sizing-and-resource-maps).

Shared text ownership lives in `sys/text.c`. A replacement MMU must provide the
physical allocation/copy and swap-transfer services it calls, together with
process residency transitions used by the scheduler. The emulated implementation
keeps those services in `machine/paged.c`; swap uses the configured block device.
The split kernel reserves a separate instruction bank and keeps PSA vectors in
ROM data space. See the kernel reference before reusing its reset/trap layout.

## Raw device transfers

Shared `sys/physio.c` owns buffer locking, completion waits and byte residuals.
A character driver supplies raw entry points and passes its strategy routine and
special buffer to physio. Its strategy handles B_PHYS through the selected MMU's
opaque descriptor and `physio_copy`; ordinary kernel-buffer transfers still use
b_addr directly. Pinning must prevent swapping or relocation until completion.
Drivers must call iodone on success and failure, retaining the untransferred
byte count in b_resid. No current device uses B_MAP bus-map allocation.

The emulated configuration adds raw ATA at character major 3, with sector-aligned
requests and the same minors as block major 1. The active swap unit rejects raw
opens. See the [raw-I/O contract](../../../../doc/kernel/devices-and-io.md#raw-physical-io).


## Tracing capability

The current Z8001 configuration supports ptrace requests 0–8 and the SC 255
breakpoint trap. Request 9 fails with EIO: software single-stepping is deliberately
excluded. Any future hardware implementation must provide instruction-step
completion as a kernel-visible trace event, with correct process ownership and
interrupt handling; the STOP pin alone does not supply that interface. Shared
V7 stop/wait/IPC code remains independent of this optional capability.


## Public ABI and clock sampling

After changing kernel headers, run `python3 tools/export-headers.py` from the
repository root. Kernel/libc/native-image builds check the matching installed
headers; public user.h exposes the structure without the kernel address macro.

CPU `clktick(frame)` passes saved PC/FCW to shared `clock(pc, flags)`.
`usermode`, `basepri` and `idlepc` interpret CPU state; `addupc` performs a
non-sleeping, fault-safe profiling word update. Clock acknowledgement belongs
to the machine. Disk drivers supply dk_busy/dk_numb/dk_wds instrumentation.
An MMU implementing eviction must honor SULOCK as well as its internal locks;
shared syslock supplies permission and flag policy.

Shared process policy supplies `swapvict(skip)`: `skip` is an NPROC-byte exclusion
array owned by the allocating machine routine. It ranks eligible residents with
V7's sleeper/stopped-size and age/nice rules. The machine layer performs the
transfer, skips failed candidates for that allocation attempt and resets p_time
on successful swap-in/out. It must not swap the current process or locked text.
Process 0 runs the adapted original sched loop; swtch selects residents only.
corealloc may sleep on a pinned extent reservation, serviced by corework in proc 0.
Background swap-in uses V7 aging gates, while explicit reservations use immediate
victim ranking. Swap I/O must sleep with buffer ownership and pinned sources;
swap-out must remove SLOAD before a transfer can schedule another process.
Shared text callers hold XLOCK across transfers and count changes. SREADY is
cleared by swtch at first dispatch, and excludes an undispatched image from
victim selection. A post-swap-in runin wait provides resident execution time on
immediate-completion controllers.

Sticky text can outlive its final process reference on swap. Configurations must
size swap-map storage for process, text and concurrent exec argument extents,
including free holes and the terminator; paged.c checks
SMAPSIZ >= 2*NPROC+NTEXT+2. Exec always reserves ten blocks for arguments and
retains V7's Out of swap panic when no reservation fits. An unused cache
entry is discarded if backing storage cannot be obtained or written.

Sleeping allocation permits concurrent provisional replacements. The paged MMU
requires CMAPSIZ >= 7*(NPROC-1)+NTEXT+5: four committed plus three provisional
extents per process, all text entries and map termination/headroom. The bound is
conservative; it avoids relying on only one allocator being active at a time.


The emulated configuration selects `dev/mem.c` at character major 4. Minor 2
is V7's EOF/rathole `/dev/null`: reads leave the residual count unchanged and
writes set it to zero. Minors 0/1 provide root-only physical/kernel-data access
through `membyte()`; other minors fail open with ENXIO. Native images install
memory devices 4,0 and 4,1 with mode 0600, and 4,2 with mode 0666. See the
[memory-device reference](../../../../doc/kernel/devices-and-io.md).
