# Current Status

Status as of 2026-10-07, including the essential-userland work in the working tree.
See the [documentation index](README.md) for procedures and subsystem references.

## Implemented and tested

The emulated Z8001 machine boots V7 to a Bourne shell. Kernel and user C use
16-bit NONSEG pointers; 0407 combined-space and 0411 split-I/D executables work.
The kernel also uses split I/D. Floating arithmetic uses the separate Zilog
software EPU engine in segment 127.

Kernel coverage includes fork/exec/wait, pipes, signals and user preemption,
V7 filesystem and TTY services, ordinary buffer-cache operation, raw I/O,
resource maps and RAM sizing, shared read-only text, conservative stack growth,
whole-process swapping with a separate V7 swapper, core dumps, ptrace requests
0–8, exec credentials, accounting, profiling and privileged residency locking.
Exec arguments use V7 swap-backed staging; swap is required even at boot.

The native two-pass compiler and optimizer have passed two-generation
convergence. Native make, ar, yacc, compiler support tools and libc have been
rebuilt inside Unix. The essential-userland image builds and installs 26
unchanged original V7 commands, and tests a native C/archive/yacc project.
See [native development](toolchain/native-development.md) for scope and
[compatibility](development/v7-compatibility.md) for source reuse.

## Limitations

- Startup still uses the small console init; original multiuser init/getty/login
  and their account/startup configuration are not integrated.
- Most original command sources are preserved, but the full command set has not
  been built and tested. Z8000 object tools such as nm/strip remain to be ported.
- Host preparation still stages compiler glue, sources and filesystem images.
  A complete native kernel/boot/system rebuild has not been established.
- Only the emulated machine has a kernel configuration. Physical machine ports
  need their own boot, interrupt, device and memory implementations.
- Fault restart accepts a conservative instruction whitelist. Memory sections
  need contiguous physical extents; arbitrary instruction restart and scattered
  allocation are not implemented.
- Full SEG executables, physical/kernel memory-device access, an active
  multiplexor and panic-specific buffer flushing are not implemented.
- Single-stepping requires hardware support; the current machine returns EIO
  for ptrace request 9. Software stepping is deliberately outside the plan.

## Next work

1. Integrate original V7 multiuser startup: init, getty, login, account files and
   startup scripts, retaining original shared policy wherever possible.
2. Expand original command coverage and supply Z8000 object inspection tools.
3. Establish a native whole-system rebuild, including kernel and boot artifacts.
4. Bring up physical machines through the documented configuration interfaces.
