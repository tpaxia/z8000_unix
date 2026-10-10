# Current Status

Status as of 2026-10-09.
See the [documentation index](README.md) for procedures and subsystem references.

## Implemented and tested

The emulated Z8001 machine boots V7 to a Bourne shell. Kernel and user C use
16-bit NONSEG pointers; s.out e707 combined-space and e711 split-I/D executables work.
The kernel also uses split I/D. Floating arithmetic uses the separate Zilog
software EPU engine in segment 127.

The [Z8001-unix MAME machine](platforms/z8001-unix.md) boots the same kernel
and FPU code from disk through a small ROM, sector-zero bootstrap and V7 `/boot`. Selected acceptance tests cover
shell pipelines, split I/D, floating point, memory faults/stack growth, shared
text, low-memory swapping and native C compilation. Both environments can boot
the same ROM and bootable disk; their filesystem and user executable formats are unchanged.

Kernel inspection uses root-only physical/kernel memory devices and native
V7 ps, pstat, dmesg and iostat, with paged process-image adaptations. `ps k`
inspects saved physical RAM and swap images using the matching kernel namelist.
Native adb supports Z8000 process tracing/core inspection and mapped kernel
RAM inspection. The kernel writes RAM and swap to a reserved disk tail on panic;
native savecore recovers the files after reboot. See [crash recovery](development/crash-dumps.md).

Kernel coverage includes fork/exec/wait, pipes, signals and user preemption,
V7 filesystem and TTY services, ordinary buffer-cache operation, raw I/O,
resource maps and RAM sizing, shared read-only text, conservative stack growth,
whole-process swapping with a separate V7 swapper, core dumps, ptrace requests
0–8, exec credentials, accounting, profiling and privileged residency locking.
Exec arguments use V7 swap-backed staging; swap is required even at boot.

Aggregate returns use caller-owned frame storage; affected callers/callees must
be rebuilt together. Native combined/split tests cover signal re-entry. Panic
flush tests cover saved-disk contents and controller failures. Stack growth also
replays plain loads whose destinations preserve all address registers.

The native two-pass compiler and optimizer have passed two-generation
convergence. Native make, ar, yacc, compiler support tools and libc have been
rebuilt inside Unix. The essential-userland image builds and installs 45
unchanged original V7 commands, and tests a native C/archive/yacc project.
The full userland inventory now rebuilds inside Unix, including the shell,
games, libraries and terminal tables. Its combined image passes the language,
formatting, spelling, archive and filesystem workloads. See
[userland coverage](toolchain/userland.md) for the complete scope and exclusions.
See [native development](toolchain/native-development.md) for scope and
[compatibility](development/v7-compatibility.md) for source reuse.

Runtime disks use unchanged V7 init/getty/login, update and cron, with account
and startup files for the emulated machine. Native login tests cover password
changes, setuid su, credentials, terminal ownership, getty respawn and session
accounting. The combined load trial passes at 512 KiB and 8 MiB: four private
48,000-byte memory holders, native split/combined builds, pipes, daemons,
logout/relogin and persistence after reboot. Build fixtures retain console init. See
[multiuser startup](development/multiuser.md).

A [Z8002 paged-MMU configuration](platforms/z8002-mmu.md)
and MAME `z8002unix` are also implemented. It runs unchanged user executables with a machine-specific
kernel, ROM and disk bootstrap. Standalone memory/fault/signal/swap tests and
MAME disk-loaded floating point and native C compilation pass. Broader machine
acceptance and native kernel/firmware rebuilding remain to be checked.

## Limitations

- PDP-11 assembly bas/roff, parts of chess and the Fortran backend
  remain unported. Some original games are distributed without sources.
  Device/site-dependent programs are built
  but not all have been exercised.
- Host preparation still stages compiler glue, sources and filesystem images.
  Kernel, FPU service, firmware, sector zero and standalone loader now rebuild
  and install inside Unix; the resulting disk boots and runs native C compilation.
  s.out is now the sole object/executable
  format. The shared host/native assembler and linker build the kernel,
  standalone bootloader, bootstrap tools and default native development image.
  ROM, trap, software EPU and disk-sector artifacts are raw images linked from
  s.out objects. GNU Z8000 tools are not bootstrap dependencies.
  All ten native compiler workloads, 45 unchanged essential V7 commands,
  24 machine-assembly checks and the strict object-utility trial pass. MAME boots
  the s.out disk image and compiles a native program. The default native
  environment passes all 127 stages, exports 17 executables and validates 147
  libc members, including the final 39-check libc test. The full userland
  rebuild has full runtime smoke coverage. Factor and primes also build and
  install natively, with exact-integer tests through the V7 56-bit range. Positive regression producers now use s.out;
  kernel exec and shared object utilities reject obsolete a.out formats. The
  historical assembler/linker writers are removed, and standalone PCC suites
  also use the shared s.out tools. See
  [ABI and formats](toolchain/abi.md), [native rebuild](development/native-rebuild.md)
  and [linker support](toolchain/ldz8.md).
- The standalone emulator and MAME use the same emulated kernel configuration. Physical machine ports
  need their own boot, interrupt, device and memory implementations.
- Fault restart accepts a conservative instruction whitelist. Memory sections
  need contiguous physical extents; arbitrary instruction restart and scattered
  allocation are not implemented.
- SEG user execution and an active multiplexor are not implemented. Panic uses bounded polled buffer/metadata
  flushing; locked/busy state is skipped and errors/timeouts can leave writes
  incomplete.
- Adb breakpoints are one-shot. Its decoder covers the base CPU instruction
  families in NONSEG/SEG and CPU-defined EPA templates; implementation-specific
  EPU operations retain raw fields. SEG executable loading remains unsupported. Kernel panic dumps
  save integer registers and the stack mapping; kernel floating registers and
  unwinding through frameless assembly remain unsupported. See [adb](toolchain/adb.md).
- Single-stepping requires hardware support; the current machine returns EIO
  for ptrace request 9. Software stepping is deliberately outside the plan.

## Next work

1. Complete the remaining machine-dependent userland ports and device/site
   integration described in the userland inventory.
2. Bring up physical machines through the documented configuration interfaces.
