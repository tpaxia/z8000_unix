# Processes and Exec

## Caught Signals

The libc `signal()` wrapper keeps a 17-entry table of C handler addresses.
For a caught disposition it registers a shared libc trampoline with syscall
48; default and odd/ignored dispositions are passed through. It translates
the previous kernel disposition back to the previous C handler on return,
and rolls back its table update if registration fails. Fork copies the table
and kernel dispositions; exec resets caught dispositions and preserves ignores.

At return to user mode, `psig(usp)` calls CPU `sendsig()` to construct this
104-byte user frame and redirect the saved PC to the registered trampoline:

| Offset from new user SP | Value |
|-------------------------|-------|
| 0 | Signal number |
| 2 | Interrupted FCW |
| 4 | Interrupted R0 |
| 6 | Interrupted PC offset |
| 8–103 | Saved EPU registers and control state (96 bytes) |

The returned SP remains on the process's kernel stack through scheduling;
`userret()` installs it in NSPOFF only at final return. Delivery rejects an
odd SP or insufficient space above the data area for the frame and trampoline
entry, terminating with SIGSEGV rather than wrapping the user stack.

The trampoline saves R1-R14, calls the C handler with the signal number,
restores EPU state through syscall 62, restores the registers, and uses unprivileged `LDCTLB FLAGS,rl0` to restore
condition flags. It then pops R0 and returns to the interrupted PC, restoring
the original SP. No privileged FCW bits are loaded from user memory, and no
CPU-context signal-return syscall is needed; syscall 62 restores only EPU
state. A handler may instead use `longjmp`.

As in V7, caught dispositions reset before delivery except SIGILL and
SIGTRAP; SIGKILL cannot be caught or ignored. A caught signal interrupting a
blocking syscall unwinds through `u_qsav`, and the syscall returns EINTR after
the handler returns. This implements neither modern signal masks/alternate
stacks, and does not add general hardware-exception routing. Core dumps and
V7 tracing are implemented as described below.

`test-signal` covers asynchronous register/flag restoration, handler syscalls,
an interrupted pipe read, nested delivery, one-shot and persistent dispositions,
ignore/error cases, longjmp, fork inheritance, exec reset, and invalid stack
rejection. Its alarm tests use two seconds because V7's next-second rounding
can make a one-second alarm fire before the blocking call starts.

## User Program Startup

The syscall table preserves V7 numbering for these interfaces:

| Number | Interface | Arguments |
|---:|---|---|
| 11 | `exec` | pathname, argv; always an empty environment |
| 52 | `sysphys` | unimplemented (`ENOSYS`); no longer EPU restore |
| 59 | `exece` / libc `execve` | pathname, argv, envp |
| 60 | `umask` | creation mask |
| 61 | `chroot` | pathname |
| 62 | Z8000 EPU restore extension | saved 96-byte EPU state |

`execv()` and `execl()` call `execve()` with `environ`. Both successful exec
syscalls update the saved user stack pointer before trap return. The boot
icode keeps using two-argument syscall 11. Sysent argument counts describe
16-bit words in registers here; the dispatcher copies R1–R5 directly rather
than decoding PDP-11 inline arguments.

This migration breaks compatibility with earlier port binaries using the old
execve, umask, chroot or caught-signal trampoline slots. Rebuild both libc
archives, relink programs, and regenerate boot/test/native disks with the
matching kernel. There are no legacy slot aliases: the old numbers conflict
with the restored interfaces. Rebuild the native environment from the repository
root (with the cross-toolchain available):

```sh
python3 tools/native-cc/build.py
cmake -S v7z8000/usr/sys -B v7z8000/usr/sys/build -DCMAKE_BUILD_TYPE=Release
cmake --build v7z8000/usr/sys/build --target kernel test_driver
cmake -S v7z8000/usr/sys -B tests/build/selfhost/host -DCMAKE_BUILD_TYPE=Release
cmake --build tests/build/selfhost/host --target test_driver
python3 tools/native-cc/selfhost.py --setup
python3 tools/native-cc/environment.py --setup
```

Rebuild the separate host driver as well: the native scripts prefer it when
present, and its profiler must recognize syscall 59. Refreshing an old disk is
insufficient. `test-abi` verifies the raw slots and libc interfaces in combined
and split I/D executables, including inherited/empty environments, file modes,
child-only root changes and the now-reserved slot 52. It also verifies that
process-table exhaustion returns EAGAIN and that slots can be reused afterward.
The configured limit is 16 processes: eight cannot accommodate recursive make,
its command shells and the compiler passes. This adds 224 bytes to kernel BSS.
The emulator profiler observes both exec syscall numbers.

`execve()` builds a user stack containing `argc`, the `argv` pointers and
their null terminator, then the environment pointers and their null
terminator. PCC startup in `tools/libc/crt0.az8` clears BSS, sets its
`_environ` global to `&argv[argc+1]`, and calls `main(argc, argv, envp)`.
Returning from main calls C `exit()`, which flushes stdio and calls `_exit()`.
`split-syscalls.py` emits one libc archive member per wrapper from
`syscalls.az8`. Each selected wrapper shares the common `_errno` symbol;
unused wrappers no longer export symbols into a command's link. There is no
separate errno archive member or startup initialization helper.

The Z8000 `brk` wrapper and `sbrk` share one exact break value, initialized to
`end`, matching V7's libc bookkeeping. A successful nonzero `brk` updates it;
a failed call leaves it unchanged, and `sbrk(0)` returns it without a syscall.
The port's raw `brk(0)` size query remains available and does not alter the
tracked break. This matters for original utilities such as sort, which mix
explicit break changes with stdio allocations. Libc also supplies `mknod` (14)
and `stime` (25); the latter loads the pointed-to time into R1:R2, high word first.

### Exec policy and CPU helpers

`sys/sys1.c` retains shared image-loading policy and the original V7 set-ID
block. An untraced successful exec applies the executable owner's UID when
ISUID is set and the current effective UID is nonzero; an effective root UID
remains root. ISGID changes the effective GID, including for root. Real IDs do
not change. `p_uid` follows effective UID for signal permission checks. A traced
exec suppresses both set-ID changes and stops with SIGTRAP. Failed exec never
applies the new credentials. Existing core policy rejects mismatched real and
effective user or group IDs before creating or truncating a core file.

Exec collects arguments with V7's original swap-backed buffer-cache loop.
Each call reserves `(NCARGS+BSIZE-1)/BSIZE` blocks (ten here), writes strings with
`getblk()`/`bawrite()`, reads them back with `bread()`, then frees the reservation
on success or failure. There is no global argument array or exec lock. `na`
counts all strings and `ne` counts environment strings, as in V7. The original
`NCARGS-1` byte limit includes terminating NULs; a null argv pointer suppresses
environment collection. Register syscall arguments use `u_arg` rather than
PDP-11's `u_ap`. The stack copy checks user-store and read errors; failure after
image replacement kills the process instead of returning into the old program.

V7 reserves the full argument extent even with no arguments and calls
`panic("Out of swap")` if allocation fails. This behavior is retained; booting
without swap therefore fails at init's exec. There is no memory-only fallback.
SMAPSIZ covers process, cached-text and concurrent exec extents, including holes
and the terminator: `2*NPROC+NTEXT+2`, or 74 entries in this configuration.

`machine/cpu.c` supplies `execsize(nc, na, ne, data_bytes)` for stack reservation,
`execstk(bno, nc, na, ne)` for argument layout and `execregs()` for register/EPU
reset. Shared `setregs()` retains signal reset, close-on-exec and accounting.
R0–R14 are cleared and the entry PC comes from the validated executable header.
The stack starts below fff0 with word alignment. Its SP is staged in the process's
`u_regs[15]`, since file cleanup can sleep and another process can change hardware
NSPOFF. Successful exec trap return uses that staged SP; `userret()` installs it
only at final return to user mode.

CPU `sendsig(handler, signal, &usp)` owns the existing libc signal-frame format,
stack growth/checks and saved-PC change. It updates SP/PC only after all copies
succeed and returns -1 on failure; shared policy then terminates with SIGSEGV.
The user signal ABI and EPU restore syscall are unchanged.

`test-exec` exercises both layouts at 8 MiB and 320 KiB. It checks credentials
through created-file ownership, real-ID queries and signal permissions; covers
the effective-root exception, a subsequent ordinary exec, traced set-ID files,
failed exec, and core suppression for unequal IDs. Tracing inspects startup
registers, EPU state and argc before the new program executes. Argument probes
exercise 5,119 bytes (including high-bit characters), E2BIG, bad pointers, V7
null-argv/environment behavior and concurrent different-inode execs with delayed
swap interrupts. Six-KiB swap runs allow only one argument reservation and verify
repeated failure cleanup and successful reuse. Zero/undersized swap probes check
the original panic explicitly.

## Core dumps

The default actions for SIGQUIT, SIGILL, SIGTRAP, SIGIOT, SIGEMT, SIGFPE,
SIGBUS, SIGSEGV and SIGSYS use the original V7 fatal-signal switch and `core()`
file policy in `sys/sig.c`. Caught or ignored signals do not dump. A completed
dump sets bit 0200 in the low wait-status byte; the signal is in bits 0–6.
Normal exit status stays in the high byte. SIGKILL/SIGTERM do not dump.

The kernel looks up `core` in the current directory, creates it with V7 mode
0666 subject to umask when absent, checks write access and regular-file type,
and truncates an accepted existing file. It uses the existing namei/maknode,
access/itrunc, writei and iput paths. Two deliberate corrections to the original
policy are documented in the source: mismatched real/effective user **or group**
IDs reject dumping before pathname lookup; a rejected nonregular target cannot
report a successful dump merely because u_error was zero. Detected write errors
leave a possible partial file and do not set the core bit. As with V7 buffered
file writes, success does not promise power-loss durability or detect a later
asynchronous write failure.

The format retains V7's u-area/data/stack ordering, using this port's sizes:

| File offset | Contents |
|---|---|
| 0 | USIZE × 64 = 4096 bytes of u-area and kernel stack |
| 4096 | u_dsize × 64 bytes from user data address 0 |
| 4096 + u_dsize × 64 | u_ssize × 64 bytes from the top-of-address-space stack mapping |

The unmapped gap and allocation padding are excluded. Shared 0411 instruction
text is omitted; 0407 text already lies within its combined data image.
`machine/paged.c:coredump()` writes the sections through their existing mappings.
It does not call estabur, allocate replacement memory, or reproduce the PDP-11's
temporary contiguous remapping. Normal scheduling/swap-in restores the mappings
if file I/O sleeps. A different MMU implements this helper for its own layout.

Core files use Z8000 big-endian values and the kernel's `h/user.h` layout; they
are not binary-compatible with PDP-11 core files. `u_ar0` remains a kernel virtual
pointer to the original trap frame (subtract 0xf000 to locate it in the file).
The added `u_regs[19]` contains R0–R15, FCW, PC segment, and PC offset. Entry wrappers
pass R13/R14 before C reuses them; the EPU adapter takes them from its full frame.
`coreregs(usp)` completes the snapshot from the saved frame and process-local SP.
The ordinary trap frame and user syscall ABI are unchanged. The existing u_fpe
field contains software EPU state. Core readers must use the matching kernel
header. Ptrace exposes these saved registers as described below; a native
debugger frontend remains future work.

`test-core` reads real core files in both executable layouts, checking data and
stack markers, sizes, modes, registers and PCs after syscall, SEGTRAP and timer
entries. It tests refused targets, non-core signals, partial files on ENOSPC,
recovery after freeing disk space and process churn with verified swap traffic
at 320 KiB RAM. A target-ABI fixture exercises the actual `core()` policy with
mismatched credentials and a read-only filesystem. Existing signal/fault tests
mask the core bit when testing only the terminating signal.

## Process tracing

V7 syscall 26 now implements `ptrace(req, pid, addr, data)`. The libc wrapper
retains V7's kernel argument order (data, pid, addr, req) and clears errno:
a successful read of word 0xffff returns -1 with errno zero. Existing syscall
numbers and the user register ABI are unchanged.

The original V7 `issig`, `fsig`, `stop` and parent-side ptrace protocol are reused,
together with wait's stopped-child branch. A process opts in with request 0;
signals stop it even when their disposition is ignored. Successful exec reports
SIGTRAP before the new program runs. Wait reports `(signal << 8) | 0177` once per
reported stop. Parent requests run in the stopped child, after the scheduler has
restored its mappings. The global V7 IPC lock serializes debugger pairs. Only a
stopped, traced child of the caller is eligible; other targets return ESRCH.
Requests run at uninterruptible IPC priority. Reparenting to init releases orphaned
stopped tracees through V7's stop/exit path. Fork does not automatically trace the
tracee's children.

| Request | Behavior |
|---|---|
| 0 (V7 also accepts negative values) | Mark the caller traced |
| 1 / 2 | Read one instruction/data-space word |
| 3 | Read an aligned word at a byte offset in the 4096-byte u-area |
| 4 / 5 | Write one instruction/data-space word |
| 6 | Write an allowed saved user register or EPU-state word |
| 7 | Continue at addr, or retain PC when addr is 1; data selects the delivered signal, with zero suppressing it |
| 8 | Force child exit with its pending signal status, following V7 |
| 9 | Return EIO: this configuration has no hardware single-step facility |

Failed memory/register requests return EIO and leave the tracee stopped. Word
accesses must be even and wholly mapped; u-area offsets must be within bounds.
Request 6 permits `u_regs` R0–R15, FCW and PC offset, plus the first 96 bytes of
software EPU state. PC segment and credentials/mappings are read-only; PC/SP
must be even. FCW writes change arithmetic flags only, retaining mode, EPA and
interrupt enables. Writes through the original saved-frame aliases for R0–R12,
FCW and PC are also accepted. R13/R14 updates are copied through the entry
wrappers, SP through the process-local return state, and EPU returns update their
full saved frame. Arbitrary u-area writes are rejected.

The shared protocol calls `traceword`, `traceuser` and `tracego` machine helpers.
V7's instruction-write restriction is retained: shared or sticky pure text fails
with EIO. An exclusive 0411 image is patched through the physical copy window;
its user instruction mapping remains read-only. Any older swap copy is freed so
subsequent eviction writes the modified image. Unlike blindly reusing V7's
ITEXT-clearing operation, this port retains inode write exclusion and marks the
image XTRC; a fresh exec of that patched prototype returns ETXTBSY until its last
reference exits. Other executing processes and the executable file are untouched.
0407 instruction writes affect only that process's private combined image.

`SC #255` (word 0x7fff) is the breakpoint trap, outside the syscall table.
It preserves general registers and reports SIGTRAP. As specified for SC, the
saved PC points two bytes beyond the breakpoint; a debugger restores the saved
instruction word and supplies the desired resume PC. This supports breakpoints
without implementing instruction stepping. Single-stepping is deliberately
hardware-only: there is no software instruction decoder or temporary-breakpoint
stepping implementation. A future machine can implement request 9 through the
CPU/board support; an external STOP pin alone is not a kernel trace exception.

Entry wrappers use nearby NONSEG veneers to preserve the six fixed two-byte
jumps at 0x0200–0x020a. The SEGTRAP/EPU entries retain direct relative jumps.
`bout2bin.py` rejects a kernel whose assembler relaxed those table entries into
longer instructions and displaced the entry addresses.

`test-ptrace` covers both layouts at 8 MiB and 320 KiB: stopped wait statuses,
access control, 0xffff reads, memory/register changes, FCW protection, signal
suppression/delivery, exec stops, breakpoint restore/resume, shared/sticky text
refusal, patched-text swapping and fresh-exec exclusion, debugger death,
concurrent IPC users and unsupported request 9. Low-memory runs verify swap I/O.


## Accounting, profiling and residency locking

Syscall 51 and libc acct(path) enable V7 process accounting; acct(0) disables it.
The file must already exist and be regular, and only root may change the setting.
V7's original sysacct/acct routine bodies, compression and syslock policy are
retained in sys/acct.c (the first two bodies have internal names). A small shared
lock serializes enable/disable and exit writes across sleeping inode I/O, so a
waiting writer cannot lose its inode when accounting is disabled. The original
acct.h supplies AFORK=01 and ASU=02; records retain V7's zero memory/I/O fields.
Disk-full writes restore the previous file size and do not prevent process exit.

Syscall 44 and libc profil select a user-data histogram. Each user-mode tick calls
CPU addupc with the interrupted PC. It reproduces V7's unsigned delta/scale
halving, multiplication, shift and word rounding; only complete buffer words
are sampled. A counter wraps at 16 bits. Invalid/alignment/wrapping addresses
and copy faults disable sampling without changing syscall errno. The helper
never sleeps. Scales 0/1 disable collection; fork inherits it and exec disables it.
The unchanged V7 monitor() library routine can write a sampled mon.out histogram.
Compiler call-count instrumentation and the prof command are separate work.

Clock policy again maintains V7's 32 dk_time CPU/disk buckets. CPU helpers decode
mode/priority and identify the idle return PC. The HD driver supplies per-unit
busy bits, command counts and V7 transfer counts (b_bcount >> 6 units).

Syscall 53 and libc lock(flag) implement the original root-only SULOCK policy.
The MMU's eviction scan skips SULOCK as well as kernel-locked/system/current
processes. Locked residency is not inherited by fork; exit clears it. Locking
can leave allocations unable to find a victim and return ENOMEM. The current
contiguous allocator still does not promise that a locked process can grow.

`test-services` exercises both layouts at 8 MiB and 320 KiB: public type sizes,
time stores, IDs, profiling samples/fork/exec/invalid buffers, monitor output,
accounting records/credentials/flags, concurrent exits and toggling, disk-full
recovery and privileged lock calls. Target-ABI fixtures run the actual addupc
and eviction scan with deterministic scaling, rollover, read/write faults and
locked/unlocked/stopped candidates. test-exec also checks effective-ID wrappers
across actual set-ID exec; test-bio verifies disk instrumentation.

### Fork admission and process policy

Fork uses original V7 per-effective-UID counting and its `a > MAXUPRC` boundary,
and reserves the final process-table slot for root. With NPROC=16 and MAXUPRC=25,
the reserved slot is the effective limit in this configuration. Root can use it;
a full table returns EAGAIN to everyone. The PDP-11 preliminary reservation of a
maximum-size swap image is omitted: resident fork remains possible without swap,
and actual allocation/direct-to-swap failure rolls back references and returns
EAGAIN. Exit retains V7 resource/reparenting policy with `freemem()` replacing
physical release; wait retains original zombie and traced-stop collection.
