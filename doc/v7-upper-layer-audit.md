# V7 upper-layer restoration audit

Audited against the checked-in pristine `v7unix/usr/sys` tree after kernel
configuration commit `14e85a1` (compiler `67a8dcc`). This is a source audit and
restoration plan. Batches 1–5 and access-fault recovery are implemented and
tested within the scopes below. Batch 4 covers ordinary cache operation; panic
flushing remains deferred. Batch 6 begins with resource maps and real memory
sizing; the remaining proposals are not claims of runtime support.

The objective is to retain V7 policy and interfaces above replaceable CPU,
MMU and device mechanisms. Driver improvements are outside this audit except
where a missing contract prevents an upper-layer feature from working.

## Scope and classification

At the audit baseline, compared all 19 current `sys/*.c` files, common device support `tty.c` and
`partab.c`, and all 20 current kernel headers: 41 files, of which 15 are
byte-identical and 26 differ. `sys/bio.c` is compared with V7's `dev/bio.c`;
its different directory does not make it a new implementation. These counts
exclude machine-specific files and missing modules, so they are not a measure
of overall V7 completeness.

The identical files at that baseline were `alloc.c`, `prim.c`, `partab.c`, and headers `buf.h`,
`callo.h`, `conf.h`, `dir.h`, `fblk.h`, `filsys.h`, `ino.h`, `inode.h`,
`mount.h`, `stat.h`, `timeb.h`, `tty.h`.

Differences fall into four categories:

- **Required adaptation:** Z8000 register/trap conventions, context labels,
  big-endian disk addresses, executable header fields and EPU state.
- **Restorable shared code:** upper-layer rewrites or declarations for which
  there is no inherent Z8000 requirement.
- **Missing facility:** code removed along with an entire feature, such as
  swapping, shared text or multiplexed channels.
- **Behavioral discrepancy:** observable departure from V7 that needs a
  deliberate correction and an independent regression test.

## Findings that affect correctness or compatibility

### Signal termination status (corrected in batch 1)

[`psig()`](../v7z8000/usr/sys/sys/sig.c) now passes the signal number in the
low byte to `exit()`, matching V7. Normal `exit(n)` retains its high-byte
status. Core dumping is still absent, so no core flag is added.

The signal, preemption and EPU tests previously asserted shifted signal
numbers and now check V7 status. Independent normal-exit coverage distinguishes
`exit(15)` from signal 15. The Bourne shell reports status 15 for the former,
and `Terminated` with status 143 for the latter, without a core-dump report.

### User-copy policy (restored in batch 2)

[`subr.c`](../v7z8000/usr/sys/sys/subr.c) now selects user data (0), kernel (1)
and user instructions (2) in `passc()`/`cpass()`, and preserves transfer counters
when a byte helper fails. The original V7 `passc()` ternary is parenthesized so
the negative-result comparison applies to both instruction and data helpers.
This is a correction to the original expression, not a compiler workaround.

[`rdwri.c`](../v7z8000/usr/sys/sys/rdwri.c) now uses V7's original `iomove()`:
aligned user transfers use bulk helpers and check their return values;
other transfers use `passc()`/`cpass()`. Byte failures retain accounting for
completed bytes, while bulk failures leave the operation's counters unchanged.
A failed bulk copy may nevertheless have modified a destination prefix.

The [machine-helper contract](kernel-technical-reference.md#shared-user-copy-policy-and-machine-helper-contract)
records required fault behavior. The following machine-layer step now rejects
address wrap and recovers SEGT faults at the user-access instructions. Policy
tests inject helper failures; additional guest tests exercise actual bus denial
and CPU trap delivery. The fixed-bank MMU still maps full user banks read/write.

### Syscall numbering and exec interfaces reconciled (batch 5)

The table now uses V7's two-argument `exec` at 11, environment-aware `exece`
at 59, `umask` at 60 and `chroot` at 61. The Z8000 EPU-state restore extension
moved from 52 (`sysphys`, still unimplemented) to unused slot 62. Libc
`execve`, `umask`, `chroot` and the signal trampoline use the matching numbers;
`execv` and `execl` inherit `environ`. Both exec entries receive the trap's
successful-exec stack handling. Argument metadata for stty/gtty and the
disabled multiplexor now reflects the register ABI.

These changes require rebuilding installed programs and images together with
the kernel; old slot aliases would collide with the restored interfaces.
Register arguments and error returns remain CPU ABI differences. Missing
optional syscalls remain unimplemented; number alignment does not add them.
See [startup and migration](kernel-technical-reference.md#user-program-startup).

## File-by-file restoration map

Paths in this table are relative to `v7z8000/usr/sys`.

| Files | Difference and disposition |
|---|---|
| `sys/alloc.c`, `sys/prim.c`, `dev/partab.c` | Already identical. Preserve them. |
| `sys/pipe.c`, `sys/sys3.c` | Restored byte-for-byte in batch 1 (the only difference was a removed `reg.h` include). |
| `sys/sys4.c` | Restored byte-for-byte in batch 1, including `u.u_r.r_time`. |
| `sys/sys2.c` | Restored V7 multiplexor branches and named return access; retains the machine `useracc()` range check. |
| `sys/fio.c` | Restored byte-for-byte in batch 3, including the optional channel argument to device close. |
| `sys/nami.c` | Restored byte-for-byte in batch 3; lookup/create beyond a 64 KB directory offset pass. |
| `sys/iget.c` | Keep big-endian three-byte disk-address conversion. Batch 3 restored the multiplexed-inode exception alongside its supporting declarations. Do not reinstate PDP-11 byte order. |
| `sys/rdwri.c`, `sys/subr.c` | Batch 2 restored shared copy dispatch/error accounting as described above. Treat extra offset casts as convergence candidates, not proven compiler requirements. |
| `sys/bio.c` | Batch 4 restored V7 ordinary cache code, DISKMON counters and word clearing. Physical-map release, swap and raw-I/O remain excluded pending their machine/device contracts. |
| `dev/tty.c` | Batch 3 restored original shared control flow, multiplexor callbacks, discipline controls and the common-handler return contract; retains validated/interrupt-protected parameter updates. |
| `sys/prf.c` | `panic()` still omits `update()`. Batch 4 source review found that normal flushing can wait on buffers owned by the panicking path; a bounded panic-specific protocol remains separate. |
| `sys/clock.c` | FCW tests and call signature are architecture adaptations. Profiling and disk/CPU instrumentation were removed. Keep shared accounting/callout policy and place CPU predicates/clock acknowledgement behind machine interfaces. |
| `sys/main.c` | Extra console open/dup bootstrap, relocated global tables, no core/swap map initialization or process-0 swapper. Restore generic initialization as those facilities return; retain configuration boot-device selection. |
| `sys/slp.c` | `sched()`, `swapin()` and real `expand()` removed; `setrun()` forces `SLOAD`; fork assumes fixed user banks and panics on u-area exhaustion. Shared scheduling is retained, but restoring memory policy needs more than changing source paths. |
| `sys/sys1.c` | Fork/exit/wait and exec were substantially rewritten. Exec lacks shared text and set-ID handling; argument collection uses a fixed kernel buffer. Saved-register/stack construction is still embedded in shared code and should move behind CPU helpers. Preserve current executable validation and split-I/D support. |
| `sys/sig.c` | Signal-frame construction is Z8000/EPU-specific and belongs behind a `sendsig`-style interface. Common selection/default-action policy can converge; core dumping, tracing and stack growth are missing. Fix wait status first. |
| `sys/sysent.c` | Preserve register-based dispatch, but reconcile interface numbering and optional syscalls explicitly. Reserved V7 slots should not be counted as missing implemented features. |

## Headers and optional features

The return-value union in [V7 `user.h`](../v7unix/usr/sys/h/user.h) contains
an unnamed register pair, `off_t r_off` and `time_t r_time`. Our struct-only
replacement caused casts in `sys2.c` and `sys4.c`. Batch 1 restored this
declaration and both named accesses, preserving Z8000 `label_t`, the actual
a.out header and EPU fields. Generated code is unchanged except for the
intentional signal-status fix; long time/seek returns pass on target.

`file.h` and `mx.h` now match V7, including the channel pointer and FMP flags.
`proc.h` still lacks `struct xproc`, which was recreated differently inside
`sys1.c`. Move the zombie overlay back to the shared header only after checking
its offsets against the actual target `struct proc`.

`acct.h` is a stub. `reg.h`/`seg.h` need Z8000 definitions rather than PDP-11
register constants. Table sizes in `param.h` are tuning decisions, while the
context-label size is ABI. Its `USIZE` remains 16 clicks (1 KB), although the
current MMU allocates/maps a 4 KB u-area/stack window. Reconcile that accounting
before importing code that allocates, swaps or dumps `USIZE` clicks.

Batch 3 installs V7's [sys/fakemx.c](../v7z8000/usr/sys/sys/fakemx.c) and
restores the associated filesystem/TTY branches. The selected configuration
links these disabled-multiplexor stubs; syscall 56 returns `EINVAL`, matching
V7 without a configured multiplexor. No channel device is installed, `mpxip`
remains null, and ordinary `!` characters retain their pathname meaning.
`KERNEL_OPTIONAL_C` allows a configuration to select optional shared modules.

`ttioccomm()` now returns 1 for recognized requests (including errors), or 0
without setting an error for an unrecognized request. The console driver maps
unhandled requests to `ENOTTY`; other drivers can implement their own fallback.
Discipline zero has real callbacks and `nldisp=1`; unconfigured disciplines
return `ENXIO`. The original line-discipline ioctl routing is restored.

Parameter copies are validated before flushing input, and updates remain
protected by `spl5()`/`splx()`. Special-character updates now also use a temporary
copy, preventing a partial access fault from changing live terminal state.

## Missing memory and process facilities

The following omissions form a dependency chain, not independent file copies:

1. Restore V7's resource-map allocator (`malloc.c`, `map.h`) and define the
   allocation units/ownership expected by generic process code.
2. Implement real `estabur()`/`expand()` semantics and space validation behind
   the selected MMU. Current `paged.c` functions are no-ops; `sbreak()` updates
   accounting without allocation and only enforces a fixed upper limit.
3. Restore shared-text management (`text.c`, `text.h`) and its inode lifecycle.
   `xrele()`, `xfree()` and `xumount()` are currently empty stubs in `machine/cpu.c`.
4. Restore raw/swap I/O policy through a machine/device transfer interface.
   Original `physio()` directly consults PDP-11 UISA/UDSA, and swap buffers
   encode physical addresses in `b_addr`/`b_xmem`; those mechanisms must change.
   The driver must honor `B_PHYS`, transfer length, completion and residual/error
   contracts. We need that support, not a production-quality controller driver.
5. Restore swapper policy, fork/expand allocation failure behavior and shared
   text accounting. `p_addr` currently names just the separately allocated
   u-area, whereas original V7 process-memory policy gives it broader meaning.
6. Restore stack growth, core dumping and ptrace with CPU register access and
   memory-fault support. Keep the existing safe Z8000 signal/EPU restoration.

V7 `ureg.c` is useful as an interface/policy reference, not an implementation
to copy: its mapping registers are specifically PDP-11 hardware. Likewise,
`B_MAP` cleanup is a device/bus mapping contract, not a reason to emulate the
PDP-11 UNIBUS map on every target.

Additional reusable facilities include process accounting (`acct.c`, real
`acct.h`, syscall 51) and profiling. `profil()` currently records parameters,
but `clock()` has no sampling operation. These should be tracked as incomplete
features rather than inferred to work from the presence of syscall entries.

## Compile-only feasibility probe

Eleven pristine files were compiled and assembled successfully with the current
cross-PCC: `pipe.c`, `sys3.c`, `sys4.c`, `sys2.c`, `fio.c`, `nami.c`, `rdwri.c`,
`subr.c`, `tty.c`, `fakemx.c`, and `malloc.c`.

The probe used a disposable copy of the port's headers, restoring pristine
`file.h`, adding pristine `mx.h`/`map.h`, and replacing only `u_r` with V7's
return-value union. It used `cpp -Dz8000 -Dz8002`, `cz8`, then `az8`, without
source-expression workarounds. This establishes compilation feasibility only:
there was no kernel link or execution, no new fault handling, and no proof
that current syscall/driver contracts satisfy every restored caller.

## Recommended implementation batches

| Order | Concrete scope | Required evidence |
|---|---|---|
| 1 (complete) | Correct signal wait status; restore the return union and near-identical pipe/sys3/sys4 code | Distinct normal/signal exit results, shell reporting, long time/seek returns, existing signal/preemption tests |
| 2 (complete, including machine recovery) | Restore user-copy dispatch and accounting; specify lower-layer fault behavior | Kernel/user-D/user-I transfers, odd/even lengths, boundary failures and partial accounting |
| 3 (complete) | Restore optional multiplexor declarations/stubs and filesystem/TTY call sites; restore common ioctl contract | Directory traversal and large offsets, open/dup/close, pipes, ioctl fallback, ordinary/raw/cbreak TTY behavior; no active multiplexor required |
| 4 (ordinary cache complete; panic flush deferred) | Restore remaining ordinary buffer-cache code and instrumentation; review panic flush separately | Cache reuse, delayed writes, read-ahead, async completion, error propagation and reboot persistence |
| 5 (complete) | Reconcile syscall numbering and exec interfaces in one kernel/libc/image migration | All native tools rebuilt; exec/execve environment, umask/chroot and EPU signal return verified |
| 6 | Resource maps, real memory sizing, shared text, physical I/O and swapping, then stack/core/trace facilities | Allocation exhaustion without kernel panic, fork/exec isolation, text lifetime, raw/swap transfers, growth faults and trace/core correctness |

Do not make a lower diff-line count the acceptance criterion. Preserve tested
port fixes and validate observable V7 behavior. Existing passing tests need
review where they encode the port's current behavior rather than V7 semantics.

## Batch 1 validation

- Boot, 39 libc checks, signals, preemption/console wakeup, TTY modes, split
  I/D and EPU arithmetic/process/signal tests pass.
- Shell regression checks distinguish normal exit 15 from signal 15, including
  message and `$?`; the latter is 143 under V7 shell conventions.
- Time returns above 65535 agree with `ftime()`; seek returns preserve
  `0x12345678` and a negative relative seek returns to zero. Both layouts pass.
- Comparing generated assembly before/after for all 29 kernel C files shows
  only the intended `psig()` change. The restored union preserves the target
  layout and all other generated accesses. Full 689-file compiler ratchet passes;
  baselines record the restored unnamed-struct warning and reviewed source hashes.

The time test uses `time(0)`: the existing Z8000 libc wrapper ignores a non-null
pointer argument. Full V7 `time(&value)` compatibility remains separate work.

## Batch 2 validation

`test-copy` passes 655 target-ABI cases in both combined and split I/D layouts,
using real shared functions and injectable machine helpers. Boot, libc, signal,
preemption, TTY, split-I/D and EPU suites also pass with actual kernel helpers.
Only `subr.c` and `rdwri.c` change generated code relative to batch 1; the reviewed
compiler ratchet baselines include those restorations. Machine recovery was
implemented in the following step, described below.

## Machine-layer access-fault recovery

Added a ten-site SEGTRAP recovery table in `krt.s`, real trap entry/dispatch,
range validation for bulk helpers and read/write requests, and alignment checks
for word helpers. Fault recovery preserves helper stack and caller interrupt
state. Unexpected kernel faults still panic; user faults deliver SIGSEGV.
Exec, signal-frame construction and EPU restore now handle helper failures.

All 30 guest fault scenarios pass in combined/split layouts, along with copy
policy and existing runtime suites. These use actual CPU SEGTRAP delivery from
a test-only denied bus access, not mocked helper return values. No unmapped
heap/stack gap or read-only text is claimed: current user banks remain entirely
mapped. Future MMUs must implement their mapping/permission checks in `useracc`
and generate SEGT for denied accesses that reach the bus.

## Batch 3 validation

`test-v7-interfaces` exercises a directory containing 4,100 files (over 64 KB):
late lookup, creation, link/unlink, literal `!` paths, shared dup offsets,
close-reference handling, pipes and syscall 56's disabled result. It also
compiles the actual `ttioccomm()` with target headers and substitute helpers
to verify driver fallback, alternate-discipline callbacks and copy-failure
state preservation. Existing terminal-mode tests cover the real syscall path,
GETD/SETD, unsupported disciplines, discipline ioctl errors and bad addresses.
All previous runtime suites and the compiler ratchet pass; the kernel-only
configuration also builds.

The comparable shared tree now has 43 files (the original 41 plus `mx.h` and
`fakemx.c`); 23 are byte-identical to V7. This records convergence, not feature
completeness. Full multiplexor support remains unconfigured.

## Batch 4 validation

Restored ordinary V7 cache code, original counters and word clearing. The HD
strategy now queues asynchronous requests; V7's interrupt-masked `bflush()` can
submit several before the first completion, which the previous single pointer
could not preserve. `test-bio` uses actual cache/driver code with delayed
completions and injected errors, verifying cache reuse, read-ahead, dirty
victims, asynchronous release, error handling and continuation after failure.
A real-kernel write/sync/save/reboot test verifies full and partial blocks.
Existing runtime suites and the 690-file compiler ratchet pass.

`swap()`, `physio()` and mapped-I/O cleanup still require real lower-layer
support. Panic-time `update()` was deliberately not restored: source review
shows a possible wait on a buffer already owned by the panicking path. This
is a documented remaining behavioral difference, not a tested panic-flush
implementation. See the [cache reference](kernel-technical-reference.md#buffer-cache-and-asynchronous-disk-requests).

## Batch 5 validation

Restored V7 slots 11/59/60/61 and moved EPU restore to extension slot 62 in
one kernel/libc migration. `test-abi` covers both executable layouts, raw SC11
with a poisoned third register, explicit/empty and inherited environments,
failed exec returns, umask file modes, chroot isolation and reserved SC52.
Existing signal/EPU/fault tests verify the relocated restore path. The emulator
memory profiler now recognizes both successful exec entries.

The fresh 52-step self-host run produced identical front/back/optimizer objects
and executables across two generations. All 82 native development stages pass,
including recursive `make all`; all 92 native libc archive members match the
host-built library, and native yacc reproduces the compiler parser exactly.
The final recursive build exposed the eight-process ceiling (a command shell
slept retrying fork with every slot occupied). NPROC is now 16, adding 224 bytes
of BSS. The ABI regression also fills the table, checks EAGAIN, reaps children
and verifies subsequent process/exec reuse. All runtime suites and the full
690-file compiler ratchet pass with the enlarged configuration.
