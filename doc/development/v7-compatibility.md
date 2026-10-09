# Comparison with Original Unix V7

`v7unix/` is the pristine distribution used for comparison; `v7z8000/`
is the current port. This reference describes their present source and behavior
differences. Subsystem references define the detailed contracts; the
[current status](../status.md) lists remaining work.

The port retains V7 filesystem, terminal and process policy wherever its
interfaces can be preserved. PDP-11 CPU, MMU, interrupt and peripheral code
is replaced with Z8000 machine support. Some shared files still contain
substantial port implementations; they are identified below rather than
counted as unchanged V7 merely because their behavior follows V7.

## Kernel source comparison

Paths below are relative to `usr/sys`. `sys/bio.c` corresponds to original
`dev/bio.c`; `sys/physio.c` separates the raw-I/O routine originally in that
same file. Machine and device implementations are compared separately.

Eight shared kernel files are byte-identical: `sys/alloc.c`, `sys/fakemx.c`,
`sys/fio.c`, `sys/nami.c`, `sys/pipe.c`, `sys/prim.c`, `sys/sys3.c` and
`sys/sys4.c`. The common character table `dev/partab.c` and controlling-terminal driver
`dev/sys.c` are also identical.
These comparisons cover the files present in the port, not every facility
in the original distribution.

| Shared source | Current differences from V7 |
|---|---|
| `sys/acct.c` | Original accounting and residency-locking bodies, wrapped with serialization between accounting-file replacement and exit writers. This is a concurrency fix, not a CPU requirement. |
| `sys/bio.c` | Ordinary cache policy is retained. PDP-11 physical swap transfers are replaced by machine services; raw I/O is separated into `physio.c`. UNIBUS `B_MAP` cleanup is absent. DISKMON initializes the buffer count, and error comments describe specific driver errors. |
| `sys/physio.c` | Retains V7 exclusive-buffer, wait/completion and residency policy. MMU hooks validate and pin memory and describe transfers for drivers; the code does not simulate PDP-11 mapping registers. Zero-length requests, byte residual validation and completed-byte accounting are explicit. |
| `sys/clock.c` | Original callout, CPU/time accounting, alarms, profiling and scheduler policy. The machine acknowledges the clock; CPU helpers interpret the saved PC/flags and idle state. Its entry signature and priority predicates replace PDP-11 trap arguments/macros. |
| `sys/iget.c` | Converts inode addresses to/from big-endian three-byte disk fields instead of PDP-11 byte order. |
| `sys/main.c` | Configured global tables and boot devices; MMU initializes real memory and process storage. Process 0 runs the swapper. As in V7, process 0 holds no terminal descriptors; original init opens the runtime console. Build fixtures use the small console init. |
| `sys/malloc.c` | First-fit allocation/free code is unchanged; its comment describes 2 KiB physical frames instead of 64-byte core-map units. |
| `sys/prf.c` | Panic uses a bounded polled flush instead of V7's sleeping `update()`. The separate `sys/panic.c` writes coherent cache blocks, unlocked dirty inodes and unlocked superblocks, then invokes the machine disk-dump hook and halts with interrupts masked. |
| `sys/rdwri.c` | Original read/write and `iomove()` policy. Differences are two explicit low-word offset casts, whitespace and comments; the casts are retained departures, not an established CPU requirement. |
| `sys/subr.c` | Original byte-copy policy, with parentheses correcting V7's conditional-expression precedence in `passc()` and comments clarifying the three copy spaces. |
| `sys/slp.c` | Adapts V7 scheduling, sleep/wakeup and swapper policy to separately allocated process sections. Only residents can run. Allocation/copy/swap use MMU services; extent reservations and dispatch/yield safeguards prevent races and starvation with fast emulated transfers. Fork retains direct-to-swap fallback and rolls back allocation failure. |
| `sys/sys1.c` | Fork/exit/wait retain V7 semantics through different memory and context operations. Exec validates only NONSEG s.out, supports combined/split I/D and shared text, and uses original swap-backed argument staging and set-ID rules. CPU helpers construct startup stacks/registers and initialize EPU state. Process zombie storage uses the port's `xproc` overlay. |
| `sys/sys2.c` | Original syscall policy plus an MMU `useracc()` check before read/write touches a user range. |
| `sys/sysent.c` | V7 syscall numbers with register-based argument dispatch. EPU signal-state return occupies unused slot 62. There are no aliases for earlier port numbering. |
| `sys/sig.c` | Original signal selection/default-action and tracing semantics adapted to Z8000 saved registers. CPU helpers build signal frames and MMU helpers write core images. Ptrace supports requests 0–8; request 9 has no implementation without hardware stepping. |
| `sys/text.c` | Substantial adaptation for separate physical text extents and machine swap transfers. Preserves inode ownership/write exclusion, sticky text, reference/resident counts and V7 text lock/cache-release policy. Exclusive ptrace writes invalidate backing and prevent fresh sharing. Includes a precedence correction in `xrele()`. |
| `dev/tty.c` | Original line-discipline, queue, ioctl and multiplexor control flow. Parameter copies complete before flushing; live parameter/special-character updates are interrupt-protected and cannot be partially changed by a user-copy fault. Remaining differences include whitespace. |

These changes include architecture requirements, defensive fixes and retained
implementation choices. They are not all unavoidable consequences of Z8000.
In particular, process-memory and shared-text policy have more source differences
than ordinary filesystem policy.

## Kernel headers and machine boundaries

Sixteen headers are byte-identical: `acct.h`, `callo.h`, `conf.h`, `dir.h`,
`fblk.h`, `file.h`, `filsys.h`, `ino.h`, `inode.h`, `map.h`, `mount.h`,
`mx.h`, `stat.h`, `text.h`, `timeb.h` and `tty.h`.

| Header | Current differences |
|---|---|
| `buf.h` | Clarifies that driver residuals are bytes; the structure matches V7. |
| `param.h` | Z8000 types/context labels, configured table sizes and a 4 KiB u-area/system-stack window. V7 memory accounting still uses 64-byte clicks. |
| `proc.h` | Separately allocated memory sections, residency/swap state and the shared zombie overlay. |
| `reg.h` | Z8000 trap and saved-user register indices, replacing PDP-11 indices and trace-bit definitions. |
| `seg.h` | Does not expose PDP-11 hardware mapping registers. |
| `systm.h` | Machine declaration changes, including absence of PDP-11 `regloc`; original shared declarations otherwise remain. |
| `user.h` | Z8000 context/register/EPU storage, fixed u-area mapping, executable state and section sizes. V7's return-value union is retained. |

Public kernel ABI headers are exported from the kernel definitions:

```sh
python3 tools/export-headers.py --check
```

CPU support lives in `machine/`; the selected MMU supplies mapping, physical
allocation, user-access recovery, growth, raw-I/O and swap operations. `conf/`
selects the machine, MMU, devices and global tables for both host and native
kernel builds. Device switch interfaces and common TTY code remain V7-shaped;
`dev/cons.c`, `hd.c` and `md.c` implement the current emulated hardware rather
than PDP-11 peripherals. See the [kernel overview](../kernel/overview.md) and
[porting guide](../platforms/porting-guide.md).

The current MMU allocates contiguous 2 KiB-frame extents separately for text,
data, stack and u-area. Unused gaps are unmapped; shared text is protected.
Stack growth and access recovery use a conservative Z8001 software-backout
whitelist. Arbitrary instruction restart and scattered-page allocation remain
absent. Whole-process swapping uses original V7 selection/aging policy with
machine transfer services and the port safeguards described above. See
[memory and swapping](../kernel/memory-and-swapping.md).

## Executables, compiler and libc

s.out replaces PDP-11 a.out for objects and executables. NONSEG e707 combines
instruction/data space; e711 separates them. SEG objects support kernel/boot
machine code, but SEG user execution requires further process ABI/MMU work.
Portable ASCII archives replace V7 binary archives. Raw ROM, sector-zero and
EPU images are hardware images linked from s.out, not alternate Unix executable
formats. See [ABI and formats](../toolchain/abi.md).

The native compiler is the two-pass Z8000 PCC backend. Its assembler and linker
use the same sources on the host and in Unix. Native rebuilding covers the
compiler, supporting tools, libc, C userland, kernel, software EPU and disk
bootstrap. Host scripts construct seed filesystems and supervise trials; target
compilation, assembly, linking and boot installation run inside Unix. See the
[native rebuild procedure](native-rebuild.md).

Libc's original portable C is retained where possible; CPU startup, arithmetic,
context switches and syscall wrappers implement the Z8000 ABI. Wrappers use V7
syscall numbers and are separate archive members. Shared brk/sbrk bookkeeping
accounts for the separate data/stack layout. `nlist` decodes s.out through the
shared object reader. Floating operations trap to the separately mapped Zilog
software EPU, replacing PDP-11 hardware FPS or its emulator. This service uses
the preserved Zilog arithmetic source, not a rewritten V7 floating library.
Structure/union returns use a hidden pointer to caller-owned frame storage
instead of the historical static result buffer; see the
[aggregate-return ABI](../toolchain/structure-return-abi.md).

## Userland source comparison

The original-command inventory covers 158 top-level units and 762 files in
`usr/src/cmd`: none missing, 697 byte-identical and 65 changed. This includes
preserved PDP-11 implementations that are not built for Z8000. Recompute it with:

```sh
python3 tools/native-cc/userland.py --audit
```

The report is `tests/build/userland-sout/audit.json`; it compares source files,
not installed command availability. Current changed groups are:

| Source group | Reason for differences |
|---|---|
| `ar.c`, `make/files.c` | Portable archive members and s.out symbol lookup. |
| `adb/` | Z8000 instruction display, public register/core layout, s.out symbols and one-shot breakpoints replace PDP-11 mechanisms. Shared V7 command and expression handling is retained; symbol comparison has an explicit false return instead of relying on a fall-through return value. |
| `nm.c`, `size.c`, `strip.c`, `prof.c`, `file.c`, both `mkfs.c` copies | Target object formats and inspection. Installed object utilities use the shared s.out reader. |
| `cpp/cpp.c`, `cpy.y`, `yylex.c` | Signed-character tables for Z8000, complete macro names, `#error`, and corrections to unary-expression and hexadecimal-digit evaluation. Complete names distinguish MMU register macros sharing their first eight characters. |
| `sh/` | Target headers/types, signed-character tables, allocation/stack handling and CPU-specific signal/exec details. The Bourne shell runs natively. |
| `yacc/dextern` | Generator configuration for the target's memory budget. |
| `dc/dc.c` | Terminates the original free list without writing beyond its array. |
| `lint/lint.c` | Target alignment for long and floating types. |
| `factor.c`, `primes.c`, `num56.h`, `num56.az8` | Added C implementations retain the original assembly algorithms and interfaces. Integer limbs and Z8000 division preserve the PDP-11's 56-bit numeric range; IEEE doubles would lose precision. Original `.s` files remain references. |
| `ps.c`, `pstat.c`, `dmesg.c`, `iostat.c` | Kernel-data symbols use `/dev/kmem`; paged physical/swap process extents replace PDP-11 contiguous images. Pstat reads Z8000 registers/console/u-area frames. Iostat resolves counters individually and uses configured buffers and emulated disk labels. |

The native userland includes support programs and programs still needing
machine integration; it does not imply all original commands are operational.
The [userland reference](../toolchain/userland.md) owns the
installed inventory, runtime coverage and remaining command ports.

## Remaining departures from the original system

- Original init/getty/login, update and cron are unchanged. Runtime account,
  terminal and rc files configure the emulated machine; build fixtures retain
  console init. See [multiuser startup](multiuser.md).
- Memory-device translation uses the paged MMU instead of PDP-11 mapping
  registers; inspection tools follow the current physical extent layout.
  Live snapshots can race process changes. Kernel-dump inspection adds bank-1
  translation for raw physical RAM and optional saved RAM/swap pathnames.
- Original disabled-multiplexor stubs are selected. There is no active channel
  device or UNIBUS map implementation.
- Ptrace single-stepping requires hardware support. Automatic growth has the
  restart restrictions described above.
- Panic flushing is best effort: busy or locked state is skipped, and controller
  errors/timeouts can leave data unwritten.
- PDP-11 assembler/compiler/Fortran backends and assembly commands require
  replacement or further porting; preserved sources are not target support.
- Physical disks, tape, printers, serial terminals and site communications still
  require board drivers/configuration and testing.

The syscall, executable and register ABI must match the installed libc and
programs. Rebuild them together after an ABI change; see
[startup and migration](../kernel/processes-and-exec.md#user-program-startup).
