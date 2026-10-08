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
cdevsw[2] = { consopen, consclose, consread, conswrite } — /dev/tty alias
cdevsw[3] = raw IDE disk, block-device unit minors 0/1
cdevsw[4] = memory special file, only minor 2 (/dev/null)
```

`dev/mem.c` retains V7's minor-2 EOF/rathole behavior: reads return zero
bytes and writes consume the supplied count. The basic and native development
images install `/dev/null` as character 4,2 with mode 0666. Other minors return
ENXIO; physical/kernel-memory access is not implemented. The original shell uses
this node for background commands' default stdin.

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
The same interface suite tests restored filesystem call sites, including
lookup and creation beyond 64 KB directory offsets and disabled multiplexor
syscall behavior. The real multiplexor remains unconfigured.

## Buffer cache and asynchronous disk requests

`sys/bio.c` again uses V7's ordinary cache implementation, word-based `clrbuf`
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

### Panic-time flushing remains separate

The port still prints the panic and idles without calling `update()`. Source
review shows that `update()` can call `getblk()`/`bwrite()` and sleep on a busy
buffer or I/O. If the panicking path owns that buffer, completion of a flush
cannot be guaranteed; device/cache corruption is another possible panic cause.
Restoring V7's unconditional `update()` here would risk hiding the panic behind
a deadlock. A future best-effort panic flush needs a separate bounded protocol
that avoids owned buffers and does not depend on normal interrupt completion.

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
`test-physio` exercises real syscalls in 0407 and 0411 executables at 8 MiB and
320 KiB RAM, including eight concurrent workers and verified swap traffic.
It uses appended scratch sectors outside the filesystem, tests data/stack
buffers and alignment/bounds errors, and checks progress and recovery at disk end.
