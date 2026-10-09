# Devices and I/O

## RAM Disk DMA

The kernel runs in NONSEG mode with 16-bit pointers (64KB address space). The disk image cannot live in this space alongside the kernel. Instead, the RAM disk driver (`dev/md.c`) uses I/O port-based DMA: it writes a block number and kernel buffer address to I/O ports, and the emulator performs the memory transfer.

### DMA Controller Ports

| Port | R/W | Description |
|------|-----|-------------|
| 0xE0 | W | Block number high byte |
| 0xE1 | W | Block number low byte |
| 0xE2 | W | DMA address high byte (kernel buffer offset) |
| 0xE3 | W | DMA address low byte |
| 0xE4 | W | Command: 1=read block→mem, 2=write mem→block |
| 0xE5 | R | Status: 0=ok, 0xFF=error |

On command write, the emulator immediately copies 512 bytes between the disk image and the memory region at `0x010000 + dma_addr` (segment 1). Reads beyond the end of the disk image return zero-filled blocks.

### Device Switch Tables

```
bdevsw[0] = { mdopen, mdclose, mdstrategy, &mdtab }   — RAM disk
bdevsw[1] = { hdopen, hdclose, hdstrategy, &hdtab }   — IDE hard disk (the root device)
cdevsw[0] = { consopen, consclose, consread, conswrite } — console
cdevsw[1] = the same entry, spare
cdevsw[2] = { syopen, nulldev, syread, sywrite, sysioctl } — controlling /dev/tty
cdevsw[3] = raw IDE disk, block-device unit minors 0/1
cdevsw[4] = memory: minors 0 physical, 1 kernel data, 2 /dev/null
```

`dev/sys.c` is the unchanged V7 controlling-terminal driver. It forwards open,
read, write and ioctl through the process’s controlling device; its switch-table
close is `nulldev`, so closing `/dev/tty` does not tear down an active terminal.

`dev/mem.c` retains V7's minor-2 EOF/rathole behavior: reads return zero
bytes and writes consume the supplied count. The basic and native development
images install `/dev/null` as character 4,2 with mode 0666. The original shell
uses this node for background commands' default stdin.

`/dev/mem` (4,0) reads/writes physical RAM by byte offset; `/dev/kmem` (4,1)
reads/writes the kernel's 16-bit data space, including its current u-area. Both
nodes have mode 0600 and the driver requires superuser privilege at open.
Physical access stops at installed RAM; kernel offsets must be below 65536.
Invalid offsets/minors return ENXIO and invalid user buffers return EFAULT.
The machine supplies `membyte()`; the paged implementation uses the existing
physical window and restores its mapping with interrupts masked.

The installed `/unix` retains global s.out symbols. V7 `ps`, `pstat`, `dmesg`
and `iostat` resolve those symbols and read `/dev/kmem`; process u-areas and
stack extents come from `/dev/mem` or `/dev/swap`. The live views are best-effort
snapshots, not atomic debugger captures. `pstat -u` takes a physical 2 KiB frame
number in octal.

`ps axlk [namelist [core [swap]]]` inspects a kernel RAM dump. Defaults are
`/unix`, `/usr/sys/core` and `/dev/swap`, retaining V7's live-swap default.
Use absolute pathnames because V7 `ps` changes directory to `/dev`. For a
reliable historical view, pass a swap image captured with the RAM dump; live
swap may already have been reused. The namelist must belong to the dumped
kernel and contain its global symbols.

The dump is raw installed physical RAM, starting at address zero, with no
header or MMU translation. Kernel table symbols are offsets into physical bank
1 (`0x10000`); resident u-areas and page extents use their physical frame
addresses. Swapped u-areas/data/private text/stacks use the same offsets as live
inspection. RAM length is checked against the dumped `physmem`; missing kernel
tables or unreadable u-areas fail the command. A missing swap file is permitted
when all selected processes are resident. Zombies need no u-area. Processes
locked during a memory transition are skipped, as in live inspection.

The standalone emulator's `-K` and `-W` options save physical RAM and swap at
the same stopped CPU state. They also work after a panic halt. These remain host
capture options independent of the kernel disk writer below.


The console keeps V7's diagnostic `msgbuf`/`msgbufp` ring for `dmesg`. Normal
TTY transmission bypasses that ring. TTY input/output counters accompany the
existing disk and clock instrumentation. `iostat` resolves each counter
separately and uses the configured buffer count; its HD/SW columns denote root
and swap. Emulated disks have no calibrated transfer latency, so estimated
transfer-time columns are zero.

## Kernel-written crash dumps

The emulated machine selects `machine/dump.c`. After mounting root, `dumpinit()`
checks for a reserved tail beyond the superblock's filesystem size. The required
sector count is `1 + physmem * 4 + swap-unit sectors`: a commit sector, installed
physical RAM and the entire secondary swap unit. Root capacity comes from normal
word port `0x00b6`, clamped to 65535 sectors in both emulators. Insufficient space
silently disables dumping; filesystem blocks and active swap are never used as
output storage. The current writer requires root ATA unit 0 and swap unit 1.

After bounded panic flushing, `panicdump()` copies RAM through the MMU window
and reads swap using `hddump()`. ATA transfers poll with interrupts masked,
without sleeping, allocating buffers or servicing the ordinary queue. Each
poll has a finite bound. The first write clears the old commit; the last write
commits a completed dump. A later failure leaves an invalid record. If even the
initial clear fails, an older valid record can remain. Recursive panics skip
both flush and dump and halt.

The commit sector stores five big-endian 32-bit fields: magic `0x5a384b44`
(`Z8KD`), version 1, RAM bytes, swap bytes and kernel time; remaining bytes are
zero. Raw RAM follows, then raw swap. RAM is copied while the dump routine is
running: its own buffer, stack and polling state change during capture, although
no processes are dispatched. A separate versioned `_kcrash` record in RAM saves
the original access-fault frame or the direct `panic()` caller's integer registers,
FCW, PC, MMU fault latch and physical UPAGE stack mapping. The record is preserved
before flushing and dump I/O; the remaining RAM is not an atomic pre-panic snapshot.
Locked or transitional objects remain
subject to the inspection restrictions above.

Native `savecore` reads `/dev/rhd` (character major 3, minor 0), validates the
record and copies its payload into mode-0600 `core` and `swap` files. `adb -k`
reads kernel globals, saved registers and the kernel stack; see [adb](../toolchain/adb.md).
Preparation and recovery commands are in [crash recovery](../development/crash-dumps.md).

## Terminal control

The libc stubs pass `ioctl(fd, command, address)` in R1–R3. Kernel `ioctl`
handles `FIOCLEX`/`FIONCLEX` for any valid descriptor, rejects ordinary files
with `ENOTTY` for device commands, and calls the character driver's `d_ioctl`.
The two-argument `stty` and `gtty` calls translate to `TIOCSETP` and `TIOCGETP`.

The console's `consioctl` delegates to `ttioccomm`: settings (`GETP`, `SETP`,
`SETN`), special characters (`GETC`, `SETC`), flush, and tty state flags.
`SETP` drains output and flushes input; `SETN` changes settings without that
flush. Raw output sends all eight bits, including values otherwise used as
output-delay markers. Baud values are stored but do not configure hardware on
the host console. Alternate disciplines and modem commands return `ENOTTY`.
The existing exclusive-open and hangup state flags do not implement physical
modem behavior or enforce exclusive console opens.

The emulated console schedules transmit completion on the next clock tick
and drains the queued bytes then. This preserves V7's sleep/wakeup ordering:
`ttwrite` sets `ASLEEP` after calling `ttstart` when the queue exceeds `TTHIWAT`.
Draining synchronously from `ttstart` would wake the writer before it slept,
leaving long output such as `ls /bin` blocked. The shared V7 TTY code is unchanged.

Run `test-tty` for settings and interactive mode regressions; see Step 21 in
[implementation-steps.md](../history/implementation-steps.md).

## Shared TTY ioctl contract

`ttioccomm()` implements the V7 handled/unhandled interface: return 1 for a
recognized request (with `u_error` if it failed), or return 0 so the driver can
try a device-specific command. `consioctl()` sets `ENOTTY` for this latter case.
The configured ordinary line discipline is zero; GETD/SETD are supported,
unconfigured disciplines return `ENXIO`, and DIOCGETP/DIOCSETP route to the
discipline's ioctl callback (the ordinary discipline rejects them with ENODEV).
Parameter updates validate the user copy before flushing or changing state.
Both parameter and special-character commits use interrupt masking.

`test-v7-interfaces` tests the shared contract on the target ABI, including
failure preservation and alternate callback routing through substitute test
disciplines. `test-tty` exercises the real terminal syscall path and input modes.
The same interface suite tests filesystem call sites using the V7 interfaces, including
lookup and creation beyond 64 KB directory offsets and disabled multiplexor
syscall behavior. The real multiplexor remains unconfigured.

## Buffer cache and asynchronous disk requests

`sys/bio.c` uses V7's ordinary cache implementation, word-based `clrbuf`
and `DISKMON` accounting. `io_info.nbuf` is initialized to NBUF; `nread`,
`nreada` and `nwrite` count submitted operations, `ncache` counts `bread()`
cache hits, and `bufcount[]` records the free-list position of reused buffers.
These are diagnostic counters, not completion/durability statistics.

Raw `physio()` lives separately in `sys/physio.c`, preserving the ordinary
cache implementation. Whole-process swap uses its own machine-layer buffer
and block-driver transfers. No current driver creates `B_MAP` requests;
asynchronous bus-map ownership and release remain unimplemented.

The HD driver now queues busy buffers through `av_forw`, headed by
`hdtab.b_actf/b_actl`, with one controller request active at a time. This is
required by V7 `bflush()`: it can submit multiple asynchronous writes while
interrupts are masked. A single active pointer previously let later requests
overwrite earlier ones. Completion removes the head before `iodone()` can
release/reuse its list link, then starts the next request. Failed reads do not
copy controller data into the buffer, and errors do not strand later requests.
Controller-busy and read-not-ready interrupts leave the request pending.

`test-bio` compiles the actual cache, HD driver and `binit()` for the target ABI
with a deferred-completion controller and injectable read/write errors. It
checks cache hits/counters, delayed writes, dirty eviction, read-ahead reuse,
queue order, completion/free-list integrity, buffer clearing, retries and
specific/default error propagation. A separate real-kernel test writes 24
blocks with partial-block updates, calls sync, saves the settled disk image,
and verifies every byte after a fresh boot. As in V7, sync queues delayed writes;
the reboot test waits for completion and does not claim power-loss durability
at the instant sync returns.

### Panic-time flushing

`panic()` masks interrupts, preserves the first diagnostic, calls the separate
`sys/panic.c` flush, invokes the machine dump hook and halts without enabling interrupts. A recursive panic
prints its diagnostic and halts immediately. It never calls V7 `update()`, whose
buffer allocation and I/O waits can sleep on resources owned by the panicking
path.

The panic flush drains queued device operations through `panicpoll()`, writes
available delayed-write buffers, serializes unlocked dirty inodes and writes
unlocked writable superblocks. Its private block buffer avoids allocation or
waiting for cache owners. Metadata reads use coherent cached blocks where
available; a busy or erroneous cache block is skipped rather than reading an
older disk copy. Successful inode writes refresh any cached copy so later
inodes in that block retain earlier updates.

Polling is bounded to 30,000 calls per completion wait. Timeout stops the flush
without reusing an outstanding transfer's buffer; completed errors are reported.
Locked/busy state is skipped because it may be partly modified. This is best
effort, not a filesystem transaction or guaranteed crash consistency. The
machine's `panicpoll()` must service completion without sleeping, enabling
interrupts or dispatching processes; the emulated implementation polls ATA.

`test-kernel-gaps` exercises queue draining, cache and metadata writes, locked
objects, errors and timeouts. It also triggers a real kernel fault after writing
an unclosed file, saves the halted disk and verifies that file's contents.

## Raw physical I/O

`sys/physio.c` retains V7's special-buffer B_BUSY/B_WANTED locking,
uninterruptible PRIBIO completion wait, residual accounting and error handling.
The selected MMU implements `physmap`, `physunmap` and `physio_copy`. Validation
and SLOCK pinning happen after waiting for the special buffer: an unpinned
waiter may have been swapped in the meantime. Pinning lasts through completion;
a pre-existing SLOCK is retained on release. Pending signals cannot bypass cleanup.

The paged MMU checks the complete data-space range, including address wrap and
the heap/stack gap. For B_PHYS, b_addr holds the user virtual address and b_xmem
an opaque owner selector with a saved-lock bit. Drivers must not interpret it as
a physical address. `physio_copy(bp, offset, kernel_buffer, count, writing)`
resolves the pinned owner's pages, independently of the currently running process.
It rejects direction mismatches and transfers outside the submitted range.
Shared read-only instruction pages are not writable through this data-space path.
A replacement protected MMU must enforce its own data-page permissions as well.

Character major 3 selects raw ATA, using the block driver's unit minors 0/1.
The basic boot image supplies root-only `/dev/rhd` (3,0). Invalid units fail
with ENXIO; opening the active swap unit fails with EBUSY. Raw ATA requires a
nonnegative, sector-aligned offset and a multiple-of-512 byte count (EINVAL),
with an even, accessible user address (EFAULT). Zero-length aligned requests
submit no I/O. The driver supports 16-bit sector addresses, up to 32 MiB.

The ATA queue handles multiple sectors per request through one 512-byte staging
buffer. Each successful sector reduces the byte residual; an error leaves the
untransferred suffix intact and releases the queue, special buffer and pin.
V7 returns an error even after partial progress, but advances the file offset
by the completed bytes. The emulator reports EIO beyond either attached disk's
end instead of extending the root image or returning fabricated zero sectors.
Raw requests bypass the buffer cache; callers must coordinate raw access with
filesystem use. There is no automatic cache invalidation or partial-sector I/O.

`test-bio` now includes the actual physio and paged mapping helpers in its
controlled delayed-completion fixture. It switches the current process during
interrupt completion and checks pinning, special-buffer contention, prior-lock
preservation, page crossings, rejected ranges and partial read/write errors.
`test-physio` exercises real syscalls in e707 and e711 s.out executables at 8 MiB and
320 KiB RAM, including eight concurrent workers and verified swap traffic.
It uses appended scratch sectors outside the filesystem, tests data/stack
buffers and alignment/bounds errors, and checks progress and recovery at disk end.
