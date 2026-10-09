# Z8000 ABI and Object Formats

## Execution and object model

Kernel and user C use NONSEG 16-bit pointers. **s.out is the sole production
object and executable format.** e707 combines code and data; e711 provides
separate 64 KiB instruction and data/BSS/heap/stack spaces. The same format
family represents SEG objects, without changing the NONSEG C ABI.

The kernel C executable, standalone `/boot`, installed `/unix`, bootstrap
compiler tools and default native rebuilds use s.out. ROM, trap veneers,
primary disk sectors and the software EPU bank are raw machine images produced
from s.out objects. They have no Unix executable header.

SEG user processes remain unsupported. Kernel exec accepts only NONSEG s.out;
obsolete a.out images fail with ENOEXEC. The shared nm/size/strip/nlist reader
also rejects obsolete objects, including archive members. Positive regression
producers use the shared s.out assembler/linker; obsolete objects are retained
only as rejection fixtures. Unidot is confined to the separate historical
assembler oracle, not the installed toolchain. `.b` object filenames
and the default output name `a.out` do not specify the file's format.

See [split-I/D mappings](../kernel/memory-and-swapping.md#separate-instruction-and-data-spaces),
[syscall convention](../kernel/traps-and-interrupts.md#syscall-calling-convention),
and [startup/migration](../kernel/processes-and-exec.md#user-program-startup).
The [structure-return proposal](structure-return-abi.md) is not an adopted ABI change.

## Library Archives

`ldz8`, native `ar` and native `make` use portable ASCII archives with the
eight-byte `!<arch>\n` signature and 60-byte member headers. Native libraries
use unindexed members with names of at most 14 characters. The archive
container does not determine CPU addressing mode: object headers and
relocations, followed by the linker and loader, determine that. Current
executable support is NONSEG combined space and separate I/D in s.out.
Full segmented user executables require further ABI and loader work.

## PCC Calling Convention (Z8002)

| Aspect | Convention |
|--------|-----------|
| Stack pointer | R15 |
| Frame pointer | R13 |
| Return value | R0 (int/pointer), RR0 (long) |
| Arguments | Pushed right-to-left onto R15 stack |
| Callee-saved | R4-R7, R10-R12, R14 |
| C symbol names | Leading underscore, eight characters in all (`main` → `_main`), as on the PDP-11. Runtime support routines (`lmul`, `ldiv`, `fadd`, ...) have no underscore |
| Function prologue | `push @sp, r13; ld r13, sp; sub sp, #N` |
| Function epilogue | `ld sp, r13; pop r13, @sp; ret` |

Assembly functions called from C are defined with the underscore (`_save`, `_resume`, `_spl0`, ...) and must return values in R0. `save()` in `machine/krt.s` and `resume()` in `machine/pagert.s` preserve all callee-saved registers, the caller's R13 (FP), and the return address in `label_t`.

Note: Steps 1-10 used ACK which has the same R13 frame pointer convention. PCC was changed from R14 to R13 for Z8001 segmented mode compatibility (RR14 is the system stack pointer in SEG mode).

PCC now emits `ld r8,#frame_size; call csv` and `jp cret` directly. The shared
helpers in `PCC-z8000/z8000/lib/csv.az8` preserve the existing frame layout and
R4–R7, R10–R12, R14 and R13, leaving R0–R3 return values untouched. R8/R9 are
call-clobbered scratch registers. Unix libc archives include the helpers;
the kernel links its own copy. No assembly postprocessor is required for
these entry/return sequences.

## Installed kernel ABI headers

`usr/sys/h` is the source for matching `usr/include/sys` headers. Run
`python3 tools/export-headers.py` after a kernel-header change; kernel, libc and
native-image builds run its `--check` mode and reject stale installed copies.
Public param/types headers use guarded shared typedefs. Public user.h declares
`extern struct user u` instead of the kernel's fixed-address `u` macro.

The installed layout includes the 24-byte label_t, 4 KiB u-area, actual configured
table sizes, current exec header and EPU/register fields. Zombie xproc now lives
in the shared proc.h rather than a private sys1.c declaration; its padding keeps
times over the intended proc fields. sys/reg.h distinguishes common trap-frame
indices (R0–R12, RPS=14, PCSEG=15, PC=16) from the complete u_regs image
(UREG_SP=15, UREG_FCW=16, UREG_SEG=17, UREG_PC=18, UREG_NREG=19). It supplies
no fictitious trace bit. Full adb/ps/pstat runtime support still needs machine
adaptations and a kernel-memory device; pstat's user dump selects u_regs on Z8000.

Core, ptrace and exec regression programs now include the installed public
headers. Libc provides geteuid/getegid, and time(tloc) stores the returned 32-bit
value when tloc is non-null. ENOSYS is also declared in public errno.h.
