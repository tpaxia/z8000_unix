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

Kernel coverage includes fork/exec/wait, pipes, signals and user preemption,
V7 filesystem and TTY services, ordinary buffer-cache operation, raw I/O,
resource maps and RAM sizing, shared read-only text, conservative stack growth,
whole-process swapping with a separate V7 swapper, core dumps, ptrace requests
0–8, exec credentials, accounting, profiling and privileged residency locking.
Exec arguments use V7 swap-backed staging; swap is required even at boot.

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

## Limitations

- Startup still uses the small console init; original multiuser init/getty/login
  and their account/startup configuration are not integrated.
- PDP-11 assembly bas/roff/factor/primes, parts of chess and the Fortran backend
  remain unported. Some original games are distributed without sources.
  Adb needs its Z8000 machine layer; ps/pstat/dmesg/iostat need memory-device
  access and kernel-layout review. Device/site-dependent programs are built
  but not all have been exercised.
- Host preparation still stages compiler glue, sources and filesystem images.
  A complete native kernel/boot/system rebuild has not been established.
  On `work/native-asz8k`, s.out is now the sole production object/executable
  format. The shared host/native assembler and linker build the kernel,
  standalone bootloader, bootstrap tools and default native development image.
  ROM, trap, software EPU and disk-sector artifacts are raw images linked from
  s.out objects. GNU Z8000 tools are not bootstrap dependencies.
  All ten native compiler workloads, 45 unchanged essential V7 commands,
  24 machine-assembly checks and 174 object-utility checks pass. MAME boots
  the s.out disk image and compiles a native program. The default native
  environment passes all 127 stages, exports 17 executables and validates 147
  libc members, including the final 39-check libc test. The full userland
  rebuild passes all 194 stages, validates 192 installed outputs and passes
  the full runtime smoke suite. Legacy regression producers and
  a.out readers remain during the phaseout. See
  [ABI and formats](toolchain/abi.md), [native rebuild](development/native-rebuild.md)
  and [linker support](toolchain/ldz8.md).
- The standalone emulator and MAME use the same emulated kernel configuration. Physical machine ports
  need their own boot, interrupt, device and memory implementations.
- Fault restart accepts a conservative instruction whitelist. Memory sections
  need contiguous physical extents; arbitrary instruction restart and scattered
  allocation are not implemented.
- SEG user execution, physical/kernel memory-device access, an active
  multiplexor and panic-specific buffer flushing are not implemented.
- Single-stepping requires hardware support; the current machine returns EIO
  for ptrace request 9. Software stepping is deliberately outside the plan.

## Next work

1. Complete the remaining machine-dependent userland ports and device/site
   integration described in the userland inventory.
2. Integrate original V7 multiuser startup: init, getty, login, account files and
   startup scripts, retaining original shared policy wherever possible.
3. Establish a native whole-system rebuild, including kernel and boot artifacts.
4. Bring up physical machines through the documented configuration interfaces.
