# V7 upper-layer restoration audit

Audited against the checked-in pristine `v7unix/usr/sys` tree after kernel
configuration commit `14e85a1` (compiler `67a8dcc`). This is a source audit and
restoration plan. Batches 1–5 and access-fault recovery are implemented and
tested within the scopes below. Batch 4 covers ordinary cache operation; panic
flushing remains deferred. Batch 6 now includes resource maps and installed RAM
sizing, page-granular section allocation and real estabur/expand sizing.
Subsequent batches add conservative stack backout/growth, shared read-only text,
whole-process swapping, raw physical I/O and V7 core-file creation. Ptrace
requests 0–8 are now implemented; single-stepping requires hardware support.
Batch 10 restores V7 set-ID exec policy and separates CPU startup/signal frames
from shared exec and signal policy. Batch 11 synchronizes installed ABI headers
and restores accounting, profiling and user residency locking.

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
  multiplexed channels. Raw physical I/O, core dumping and ptrace requests 0–8 are now implemented.
- **Behavioral discrepancy:** observable departure from V7 that needs a
  deliberate correction and an independent regression test.

## Findings that affect correctness or compatibility

### Signal termination status (corrected in batch 1)

[`psig()`](../../v7z8000/usr/sys/sys/sig.c) now passes the signal number in the
low byte to `exit()`, matching V7. Normal `exit(n)` retains its high-byte
status. Core dumping was added in batch 8; completed dumps now add the core flag.

The signal, preemption and EPU tests previously asserted shifted signal
numbers and now check V7 status. Independent normal-exit coverage distinguishes
`exit(15)` from signal 15. The Bourne shell reports status 15 for the former,
and `Terminated` with status 143 for the latter, without a core-dump report.

### User-copy policy (restored in batch 2)

[`subr.c`](../../v7z8000/usr/sys/sys/subr.c) now selects user data (0), kernel (1)
and user instructions (2) in `passc()`/`cpass()`, and preserves transfer counters
when a byte helper fails. The original V7 `passc()` ternary is parenthesized so
the negative-result comparison applies to both instruction and data helpers.
This is a correction to the original expression, not a compiler workaround.

[`rdwri.c`](../../v7z8000/usr/sys/sys/rdwri.c) now uses V7's original `iomove()`:
aligned user transfers use bulk helpers and check their return values;
other transfers use `passc()`/`cpass()`. Byte failures retain accounting for
completed bytes, while bulk failures leave the operation's counters unchanged.
A failed bulk copy may nevertheless have modified a destination prefix.

The [machine-helper contract](../kernel/memory-and-swapping.md#shared-user-copy-policy-and-machine-helper-contract)
records required fault behavior. The following machine-layer step now rejects
address wrap and recovers SEGT faults at the user-access instructions. Policy
tests inject helper failures; additional guest tests exercise actual bus denial
and CPU trap delivery. Allocated pages remain read/write; unused gaps are now unmapped.

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
See [startup and migration](../kernel/processes-and-exec.md#user-program-startup).

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
| `sys/bio.c` | Batch 4 restored V7 ordinary cache code, DISKMON counters and word clearing. Raw physio now has separate MMU mapping/pinning hooks; whole-process swap now has a separate machine-layer transfer buffer. |
| `dev/tty.c` | Batch 3 restored original shared control flow, multiplexor callbacks, discipline controls and the common-handler return contract; retains validated/interrupt-protected parameter updates. |
| `sys/prf.c` | `panic()` still omits `update()`. Batch 4 source review found that normal flushing can wait on buffers owned by the panicking path; a bounded panic-specific protocol remains separate. |
| `sys/clock.c` | FCW tests and call signature are architecture adaptations. Batch 11 restores profiling and disk/CPU instrumentation. CPU predicates, saved PC and fault-safe samples are behind machine interfaces; shared accounting/callout policy remains V7. |
| `sys/main.c` | Extra console open/dup bootstrap, relocated global tables, core-map initialization delegated to the MMU; a dedicated swap map and residency-aware process-0 scheduler. Configuration retains boot-device selection. |
| `sys/slp.c` | Process 0 runs adapted V7 sched with runin/runout and aging; swtch selects residents only. Machine services allocate/copy extents and sleep during swap I/O. Explicit extent reservations, first-dispatch protection and a post-swap-in yield are documented port additions. Fork retains direct-to-swap fallback and EAGAIN rollback. |
| `sys/sys1.c` | Fork/exit/wait and exec were substantially rewritten. Exec supports shared 0411 text and original V7 set-ID rules, including tracing suppression and the effective-root exception. Argument collection uses a serialized fixed kernel buffer. CPU helpers now own startup stack sizing/construction and register/EPU initialization. Preserve current executable validation and split-I/D support. |
| `sys/sig.c` | Z8000/EPU signal-frame construction now lives behind the CPU `sendsig` interface. Common selection/default-action policy can converge; core dumping now uses V7 policy with machine-layer image writing; V7 stop/wait/ptrace requests 0–8 are restored; signal frames now request stack growth; wait status was fixed in batch 1. |
| `sys/sysent.c` | Preserve register-based dispatch, but reconcile interface numbering and optional syscalls explicitly. Reserved V7 slots should not be counted as missing implemented features. |

## Headers and optional features

The return-value union in [V7 `user.h`](../../v7unix/usr/sys/h/user.h) contains
an unnamed register pair, `off_t r_off` and `time_t r_time`. Our struct-only
replacement caused casts in `sys2.c` and `sys4.c`. Batch 1 restored this
declaration and both named accesses, preserving Z8000 `label_t`, the actual
a.out header and EPU fields. Generated code is unchanged except for the
intentional signal-status fix; long time/seek returns pass on target.

`file.h` and `mx.h` now match V7, including the channel pointer and FMP flags.
`proc.h` now owns the port's zombie overlay, formerly private to sys1.c; its
extra size word preserves the target offsets. Installed sys/proc.h is exported
from the same definition.

`acct.h` is restored unchanged. `reg.h` describes Z8000 trap/core registers;
`seg.h` deliberately exposes no PDP-11 MMU registers. Table sizes in `param.h` are tuning decisions, while the
context-label size is ABI. Batch 6 corrects `USIZE` to 64 clicks (4 KB), matching the current
u-area/system-stack mapping. `p_addr` still names only that separately allocated
window; text/data/stack have separate page-rounded core-map allocations. Whole-process swapping and core dumping are implemented through MMU helpers.

Batch 3 installs V7's [sys/fakemx.c](../../v7z8000/usr/sys/sys/fakemx.c) and
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

1. Resource maps and installed RAM sizing are implemented. Core-map units are
   2 KiB frames; V7 accounting uses 64-byte clicks. User sections allocate on demand.
2. `estabur()` validates and commits page-rounded text/data/stack extents;
   `expand()` and `sbreak()` resize real data storage with rollback on failure.
   Gaps are unmapped. Stack warnings and a conservative Z8001 software-backout
   whitelist now enable automatic stack growth; arbitrary instruction restart
   and scattered-page allocation remain absent.
3. Shared 0411 text lifecycle is implemented in `sys/text.c`, with inode write
   exclusion, resident/reference counts and an immutable swap copy. Sticky text
   retains unused swap backing; original xrele/xumount lookup policy releases it.
4. Whole-process swap transfers use machine physical-copy helpers and the block
   driver. Raw `physio()` now uses MMU validation/pinning and an opaque request
   descriptor; PDP-11 UISA/UDSA translation is not copied into shared policy.
   Bus-map ownership remains future work.
5. Process 0 runs V7 sched with aging and runin/runout wakeups; swap transfers
   sleep while residents execute. Fork can create a child directly on swap.
   Extent reservations and fast-controller progress safeguards remain port
   additions. Contiguous growth reserves replacements before freeing old extents.
6. Core dumping and ptrace requests 0–8 now use CPU register and MMU access
   interfaces. Request 9 is unsupported without hardware single-step support.
   Preserve safe Z8000 signal/EPU restoration and the explicit retry whitelist.

V7 `ureg.c` is useful as an interface/policy reference, not an implementation
to copy: its mapping registers are specifically PDP-11 hardware. Likewise,
`B_MAP` cleanup is a device/bus mapping contract, not a reason to emulate the
PDP-11 UNIBUS map on every target.

Batch 11 restores process accounting (`acct.c`, original `acct.h`, syscall 51),
profiling and privileged residency locking (53). Sampling is now connected to
clock ticks and tested through the public libc interfaces; the previous
parameter-only profil implementation is no longer the current state.

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
| 6–9 (memory, raw I/O, core dumping and tracing implemented) | Hardware single-step remains optional | Allocation exhaustion without kernel panic, fork/exec isolation, text lifetime, raw/swap transfers, growth faults and trace/core correctness |

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

The batch-1 time test used `time(0)`. Batch 11 adds the missing time(&value)
store and effective-ID wrappers, with guest tests.

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

At batch 4, `swap()`, `physio()` and mapped-I/O cleanup still required lower-layer
support. Batch 6 supplies whole-process swap; raw/mapped I/O remains absent. Panic-time `update()` was deliberately not restored: source review
shows a possible wait on a buffer already owned by the panicking path. This
is a documented remaining behavioral difference, not a tested panic-flush
implementation. See the [cache reference](../kernel/devices-and-io.md#buffer-cache-and-asynchronous-disk-requests).

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

## Batch 6: resource maps and RAM sizing

Restored `map.h` unchanged and `malloc.c` with only its allocation-unit comment
adapted. The comparable shared tree now has 45 files, 24 byte-identical to V7.
The emulated board reports installed RAM through port 0x00BA, and its bus
rejects absent physical memory. Core-map seeding excludes ROM/kernel
and EPU service. `USIZE` now matches the actual 4 KB kernel stack/u-area.
Fork allocation failure returns EAGAIN without leaving a process slot or
additional file/inode references behind.

`estabur`/`expand` now allocate text/data/stack extents in 2 KiB units, retaining
contiguity within each section. Exec and heap growth reserve replacement storage
before committing; shrinking releases complete pages, and regrowth clears newly
exposed bytes. Fork copies mapped sections only. `useracc` rejects gaps; user
accesses there raise SIGSEGV. The configured initial stack is at least 4 KiB;
the follow-up below adds conservative growth/backout, shared text and swapping.
Scattered physical pages remain outside the current allocator.

Bourne shell workspace allocation is explicit because its original SIGSEGV-driven
heap growth depended on instruction restart. This is a CPU/MMU compatibility
adaptation, not a compiler workaround. Header dependencies now rebuild all shell
objects after changes to its allocation macros.

`test-memory` covers physical bounds, page rounding, partial rollback, heap
zeroing/growth/shrink, gap faults, fork isolation and reuse, failed exec, layout
changes and large shell words/here-documents. Low-memory cases use 256, 258 and
320 KiB. All 31 kernel files and 20 shell files compile and assemble. Runtime
suites, 17 memory guest scenarios, the compiler ratchet, kernel-only build,
native compiler pipeline and recursive-stack/profile probe pass.

## Batch 6 combined follow-up: text, protection, swap and stack faults

The kernel now has separate instruction/data spaces, reserving 192 KiB for
ROM and its two banks. Shared 0411 text, ITEXT lifetime/write exclusion and
resident counts live in sys/text.c. The selected MMU supplies physical copies,
whole-process swap, fault latches and page permissions. Swap uses a separate
ATA unit, never the root filesystem. Fork has a direct-to-swap fallback.

The Z8001 implementation grows the stack after successful write warnings or
whitelisted failed stores/CALL/PUSH with software backout. It rejects failed
reads, unsafe read-modify-write operations and unsupported instruction forms.
This leaves a deliberate architectural limitation versus a fully restartable
MMU/CPU; it does not emulate Z8003/4 ABORT on a Z8001.

The memory suite covers warning growth, page-skipping stores, CALL/PUSH backout,
unsafe retry rejection, inode lifetime, shared mappings, full/disabled swap,
low-RAM process churn and private-data isolation. Original ordinary cache code
remains intact; raw physio and asynchronous bus-map ownership are still absent.


## Batch 7: raw physical I/O

Added shared `sys/physio.c` with V7 special-buffer ownership, uninterruptible
completion waits and residual/error handling. The MMU validates and pins the
user range, preserves existing SLOCK ownership, and resolves transfers using
the sleeping owner's mappings. Device code contains no process-map arithmetic.
The ATA driver supplies raw character entry points, multi-sector queue service
and a 512-byte staging buffer. Active swap is protected from raw opens.

Deferred-controller tests cover owner changes during completion, buffer waits,
lock preservation, bounds and partial errors. Real guest tests cover both
executable layouts, page-crossing data/stack buffers, disk-end errors and eight
concurrent workers under actual swapping. Bus-map allocation, core dumps and
ptrace remain separate work. Ordinary cache policy remains unchanged.


## Batch 8: V7 core files

Reused V7's fatal-signal switch and core-file lookup/create, access, truncation
and inode-release sequence in sys/sig.c. The PDP-11-specific estabur remapping
and contiguous memory write are replaced by one machine-layer coredump helper.
It writes the u-area, data and stack separately through existing mappings.
No changes to common filesystem writing or allocation policy were needed.

The explicit policy deviations are early rejection of unequal effective/real
user or group IDs and failure reporting for a nonregular target. The original
could leave an empty file for elevated credentials and could return success
when no dump had been written. Z8000 CPU wrappers preserve the missing R13/R14
values for a full register snapshot without changing the trap-frame layout.

Target tests check real images in both layouts, all ordinary entry paths,
permissions, disk-full partial writes and memory pressure with swap traffic.
Ptrace and bus-map ownership remain separate work.


## Batch 9: V7 ptrace, with optional hardware stepping

Restored original V7 signal selection, tracing stops, parent/child IPC and stopped
wait reporting. The child executes requests against its own restored mappings;
stopped processes are swappable. CPU/MMU hooks replace PDP-11 register offsets
and writable-text remapping. Requests 0–8 are supported. Request 9 returns EIO
without hardware support; software stepping is explicitly outside the scope.

V7's exclusive/non-sticky instruction-write rule remains. Patched pure text stays
read-only to user code, invalidates prior swap backing and rejects fresh exec
sharing until released; inode write exclusion remains active. This is a deliberate
adaptation to the port's immutable shared-text lifecycle. A reserved SC 255 supplies
a breakpoint trap. Register writes preserve privileged FCW fields and propagate
R13/R14/SP updates through the unchanged saved-frame layout.

Tests cover memory/register access, signals, exec, breakpoints, concurrent debugger
pairs, orphan cleanup, protected text and patched images surviving real swapping.
Fixed entry jumps now use nearby veneers and a build-time layout check.


## Batch 10: exec credentials and CPU context separation

Restored the original V7 set-ID block: untraced exec applies ISUID to effective
UID and p_uid unless already effectively root, and applies ISGID to effective
GID. Real IDs remain unchanged. Tracing suppresses both changes and reports the
existing SIGTRAP exec stop. Credentials change only after successful image and
stack construction; early failures leave the old process intact and late copy
failures kill the unusable image without applying new credentials.

Shared exec retains argument collection, image loading, signal reset, EXCLOSE
handling and accounting. CPU helpers now own startup stack sizing/construction,
entry PC and register/EPU reset. All R0–R14 are cleared, including the separately
saved R13/R14. The new SP stays in process-local storage through sleeping cleanup;
trap return installs it in NSPOFF. Shared signal policy calls sendsig for the
Z8000/libc/EPU frame and retains SIGSEGV handling if frame construction fails.

The new test-exec target checks both layouts at 8 MiB and 320 KiB, including
file ownership, real IDs, p_uid signal permissions, effective-root behavior,
credential retention through another exec, traced set-ID suppression, startup
registers/EPU/stack, signal dispositions, failed exec and core-file suppression.


## Batch 11: installed ABI and remaining shared services

Public kernel headers now export the real Z8000 layout, with build-time drift
checks. Public typedefs/context size and proc/user/register layouts match the
kernel. Core/trace tests consume those installed headers. Libc adds effective-ID
queries, time-pointer stores, acct/lock/profil and unchanged V7 monitor(). Pstat's
Z8000 user dump no longer refers to nonexistent PDP-11 mapping registers; complete
live kernel-inspection tools remain outside this batch.

Accounting restores V7 routine bodies and acct.h, including AFORK/ASU values.
Thin wrappers serialize accounting file changes with exit writes during sleeping
I/O. Profiling and CPU/disk counters are restored through CPU helpers; the original
syslock sets SULOCK and the actual MMU eviction scan now honors it. Exec disables
profiling and fork inherits samples but not residency locking. Sampling never
sleeps and disables itself on a user-copy fault.

Current comparable shared tree: 48 files, 25 byte-identical to original V7, plus
physio.c split out from original dev/bio.c. Machine code/drivers are excluded.
At batch 11, remaining substantive differences included synchronous swap policy,
exec argument staging/formats, panic flushing and optional multiplexor/
bus-mapping facilities. The installed userland and machine-specific inspection
utilities still need expansion.

## Batch 12: process admission, eviction and sticky text

Function comparison confirmed that exit differs chiefly at machine memory release,
wait retains V7 collection policy, and the channel-wide wakeup in setrun is original
V7 behavior. These already-reused policies were preserved. Fork restores original
per-user counting, the MAXUPRC comparison and final-slot reservation for root;
Z8000 return registers and allocation rollback remain necessary adaptations.

Shared swapvict now reuses the selection loop from V7 sched: largest eligible
sleeper/stopped process, otherwise age plus nice. Machine corealloc supplies
failed-transfer exclusions and performs actual allocations/transfers. Ages reset
on swap transitions. Unlike the original background swapper, synchronous allocation
does not wait for age thresholds; negative-nice young residents remain eligible.

Sticky text now retains swap backing and its inode after the last user exits.
Original xlock/xunlock and xumount lookup bodies are reused; xrele's ITEXT test
has explicit parentheses to correct the original precedence bug. Frame rounding,
conditional swap allocation and inode locking are port adaptations. Failed cache
writes release unused images, and failed loads or traced text cannot enter the cache.
The separate V7 sched loop, swap-backed exec arguments and additional executable
formats remain outstanding; this batch does not claim byte-identical scheduling.

Validation passes: 28 memory scenarios, process/text/victim policy fixtures,
tracing, exec, raw I/O, core, services, signal/preemption/ABI, all ten native compiler
cases, kernel-only build and the 694-file compiler regression baseline comparison.

## Batch 13: separate swapper and sleeping transfers

Restored process 0's sched loop, resident-only swtch and original runin/runout
wakeups. Background incoming/outgoing selection retains V7's age/nice policy and
its three-second-out/two-second-resident gates. Swap transfer sleeps now permit
resident execution; the swap buffer, source process, victim residency and text
locks remain protected throughout I/O. Failed writes preserve the resident image;
failed reads release provisional storage and retain the swap image for retry.

Port additions are explicit: corework reserves independent replacement extents
for pinned callers instead of self-swapping a partly resized image; locked incoming
texts get timed retries; SREADY and a post-swap-in runin wait prevent fast-controller
thrashing before resident user and tracing work can progress. This is a restored
V7 swapper with tested adaptations, not a byte-identical copy.

Memory regressions now include delayed swap interrupts and one injected read or
write failure in each executable layout. User-mode samples while a swap interrupt
is pending prove that resident code executes during I/O. Target fixtures execute
the actual sched loop to check idle/locked waits, aging thresholds, victim ranking,
first-dispatch protection, failure retry and reservation priority.

Swap-backed exec argument staging follows in batch 14. Additional executable
formats and full distribution startup/commands remain separate work.

Validation: all 32 memory scenarios, scheduler/process/text/victim fixtures,
tracing, exec, core, raw I/O, services and the broader runtime suites pass. All
ten native compiler cases pass; optimized split I/D was rechecked on the final
kernel with the host emulator built in Release mode. Kernel-only build, public
header consistency and the 694-file compiler regression comparison also pass.


## Batch 14: original swap-backed exec arguments

Reused V7's argument collection loop, fixed-size swap reservation, buffer-cache
staging and cleanup. Removed the global NCARGS array and exec serialization.
The Z8000 stack helper uses the original copy-back structure with checked stores,
buffer read errors, the existing stack top and process-local SP commit. It keeps
V7's total-string/environment counts, NCARGS-1 limit and null-argv semantics.
The allocation failure policy is also original: `panic("Out of swap")`, including
exec with no arguments. No no-swap fallback was added.

Concurrent execs publish ownership of newly allocated shared-text entries before
sleeping in corealloc. SMAPSIZ now also includes concurrent argument reservations
(74 entries). These are consequences of removing serialization, rather than a
replacement for V7 exec policy. The configured 0407/0411 image loader remains
specific to the Z8000 executable header and paged MMU.

Argument regressions cover both layouts at 320 KiB and 8 MiB, maximum-size/high-bit
strings, concurrent different-inode execs, bad vectors/strings, environment and
null argv, and repeated failures with room for only one reservation. Explicit
zero/undersized-swap tests require the V7 panic. Memory-pressure tests formerly
using no swap now provide six KiB: enough for argument staging but too little
for their process/text images. Ordinary process swapping still uses four MiB.

Validation passes: all 32 memory scenarios, the expanded exec suite, ptrace,
core, services/policy, raw I/O and the broader runtime suites; all ten native
compiler cases; the 694-file compiler ratchet, kernel-only build and public
header consistency check. The kernel's global argument array is gone; final
text/data/BSS sizes are 52,116/2,624/10,074 bytes.


## Essential userland audit

`tools/native-cc/userland.py --audit` inventories all 158 top-level source units
under the original `usr/src/cmd`: 762 files, none missing from the port. 728 files
are byte-identical; 34 differ, confined to `ar.c`, `make/files.c`, `pstat.c`, the
shell and yacc's configuration header. A source unit can be a directory or a
single file; these counts do not imply 158 working executables. The original
`/bin` has 153 entries, many not installed or runtime-tested in this port.

The essential userland batch builds 26 commands from byte-identical original
V7 sources using native make and cc: cat, echo, ls, pwd, mkdir, rmdir, ln, cp, mv,
rm, chmod, chown, chgrp, wc, grep, tail, sort, uniq, tee, cmp, date, sleep, sync,
kill, test and ed. It reuses the existing portable-archive ar and make plus yacc
from the native development environment. It does not rewrite command C code.

The audit found missing Z8000 libc wrappers for existing kernel syscalls:
`mknod` (14), needed by original mkdir, and `stime` (25), needed by date. These
wrappers marshal the existing register ABI; shared kernel implementations remain
unchanged. Runtime testing also exposed the old port's independent brk/sbrk
bookkeeping: sort grew its workspace with brk, then stdio's sbrk could shrink it.
The libc adaptation now shares one exact break initialized from the linker end
symbol, as in original V7's sbrk.s; unsuccessful brk leaves it unchanged.
The install rules mark mkdir, rmdir and mv set-user-ID root for V7's
privileged directory link operations; the original commands retain their real-ID
permission checks. Full multiuser startup, login/getty, remaining commands and
object-inspection tools remain separate work.


Background execution also required installing `/dev/null`. The emulated
configuration now selects the V7 EOF/rathole portion of the memory driver at
character 4,2; other memory minors are rejected. The portable ar adaptation's
strict header reader now treats a missing archive as empty for `ar r`, retaining
V7's creation behavior while continuing to reject malformed existing headers.


Validation passes all 29 userland build/integration steps, including the 26
unchanged commands, native portable-ar creation/linking, yacc generation, and
0407/0411 syscall and ordinary-user directory tests. The archive interoperability
suite, libc/ABI, all 32 memory scenarios, services/policy, signals and all ten
native compiler cases pass. The bootable disk and detailed audit/size reports
are under `tests/build/userland/`.

The compiler baseline check now covers 695 files, including all 35 kernel C
files compiling and assembling; it passes along with public-header consistency
and the kernel-only build.
