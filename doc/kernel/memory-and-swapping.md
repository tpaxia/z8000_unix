# Memory and Swapping

## Paged MMU

The emulated MMU provides 128 segments x 32 pages x 2KB pages, with only ROM, kernel and EPU banks identity-mapped on construction. User banks start unmapped. UPAGE and WPAGE remap page pairs within segment 1; IMAP selects a separate instruction bank for each logical segment:

### MMU Control Ports

| Port | Width | Name | Function |
|------|-------|------|----------|
| 0x00B0 | word | UPAGE | KDSA6 equivalent: sets seg1 pages 30-31 to frame pair (value, value+1) |
| 0x00B2 | word, read-only | SWAPSIZE | Dedicated swap unit size in 512-byte blocks |
| 0x00B4 | word | WPAGE | Copy window: sets seg1 pages 28-29 to frame pair (value, value+1) |
| 0x00B8 | word | IMAP | High byte: logical segment; low byte: instruction backing segment (7 bits each) |
| 0x00BA | word, read-only | RAMSIZE | Installed low RAM in 2 KB frames (at most 4096) |
| 0x00BC | word | PAGESEL | Select page-table entry: `(segment << 5) | page` |
| 0x00BE | word | PAGEFRAME | Set physical frame plus RO/SYS flags; `0xffff` unmaps it |

### Address Translation

```
segment = (addr >> 16) & 0x7F
if program_access: segment = instruction_bank[segment]
page    = (addr & 0xFFFF) >> 11    // 5 bits → 32 pages
pg_off  = addr & 0x7FF             // 11 bits → 2048 bytes per page
frame   = pages[segment][page]
physical = (frame << 11) | pg_off
```

### u-area Mapping

The u-area occupies virtual 0xF000-0xFFFF (4KB = pages 30-31 of segment 1). The kernel stack grows down from 0xFFFE within these pages. `resume()` writes UPAGE to remap these two pages to the target process's physical frames, swapping the entire u-area + kernel stack with a single I/O port write.

Process 0's u-area is at frame 62 (identity-mapped). Other processes allocate
u-areas and user banks from the resource map starting at physical frame 96.

### Physical memory sizing and resource maps

`mmuinit()` reads RAMSIZE and seeds V7's `coremap` with frames from 96 up to
`min(physmem, 4064)`, exclusive. Only the first 192 KiB (ROM, kernel data, kernel instructions) and
the dedicated EPU service bank are reserved. The emulator retains the EPU
bank when less low RAM is installed; absent physical accesses raise SEGTRAP.

`sys/malloc.c` retains V7 first-fit allocation and adjacent-range coalescing;
only its unit comment differs. `h/map.h` is unchanged. Core-map units are
2 KB frames; process accounting uses 64-byte clicks. `USIZE=64` represents
the 4 KB u-area/system stack. `p_addr` names its two-frame allocation;
`p_size` includes that allocation and private data/stack pages; shared text is
accounted separately. For a nonresident process, `p_addr` is its swap block.
`swapmap` is seeded from the dedicated swap device, excluding block zero; `maxmem=MAXMEM` is the I+D bank limit in accounting clicks.

Machine-layer `newmem()` allocates a u-area and copies the parent's section
sizes. User text, data and stack are separate contiguous physical extents,
rounded to 2 KiB pages. Logical segment numbers remain tied to process slots;
PAGESEL/PAGEFRAME map data from address zero and stack at the top of data space.
Split text gets a read-only mapping shared by processes executing the same inode. Unused pages, including the heap/stack gap,
are unmapped. Fork copies private sections and takes a shared-text reference. Failure rolls back all
provisional storage before publishing the child; exit releases every extent.

`estabur(nt, nd, ns, sep, xrw)` validates 64-byte click sizes and page-rounded
space limits, then acquires all replacement extents before committing mappings
and u-area accounting. Exec uses it before destroying the old image, so ENOMEM
preserves that image. `expand(total_clicks)` resizes data with text, stack and
u-area sizes fixed; `sbreak()` uses it for real heap allocation and release.
`brk(0)` retains the port's click-rounded query interface. New pages are cleared;
regrowth also clears the newly exposed portion of a retained partial page.
`p_size` accounts for the u-area and actual allocated pages.

Growth relocates a section to a larger contiguous extent while preserving its
contents. Shrink releases a data/text suffix or a stack prefix. This is V7-style
contiguous-section allocation at page granularity, not arbitrary scattered
physical-page allocation. Fragmentation or the temporary replacement allocation
can cause ENOMEM even when the final image alone would fit. CMAPSIZ covers four
extents per user process, three provisional replacements and the map terminator.
Only process context performs allocation and remapping.

The initial stack reserves at least `SSIZE=64` clicks (4 KiB), including startup
arguments. Exec enlarges this reservation if arguments plus 256 bytes of spare
stack require more, then rounds it to pages. Stack write warnings extend it
before overflow; supported failed stores can also trigger growth with software
backout. Other user gaps raise SIGSEGV; covered kernel user-copy faults return
EFAULT. `useracc()` checks every data-space page in the requested range.
Private combined-space text/data remain writable, as required for 0407.

The Bourne shell now explicitly reserves heap workspace before stores. Its
original SIGSEGV-driven break extension required restarting a failed store.
Shell word construction, expansions, environment construction and here-documents
use bounds checks; failed break requests preserve its previous break pointer.
Other user programs must request heap memory before using it.

`test-memory` checks the target allocator and sizing helpers, host MMU bounds,
partial fork and resize rollback, low-RAM exhaustion/reaping/reuse, failed exec,
layout transitions, heap zeroing/isolation/reclamation, gap EFAULT/SIGSEGV and
large shell workspace. The low-RAM workloads run at 256, 258 and 320 KiB in both
layouts. The driver reports unmapped accesses separately from accesses to absent
physical RAM; valid mappings must never reach absent RAM.

### Separate Instruction and Data Spaces

`ldz8 -i` emits V7 magic 0411: text starts at instruction address zero and
initialized data starts at data address zero. Ordinary 0407 programs retain a
combined layout. Both use 16-bit pointers and NONSEG execution. Each space has
64 KB of addresses; the a.out header limits an individual section to 65,535
bytes, and executable text must have even length. Data, BSS, heap, arguments,
and stack share the data space. The exec loader reserves a mapped stack of at least 4 KiB, enlarged when
startup arguments plus 256 bytes require more. Heap and stack pages must not
overlap; the stack does not grow automatically.

For process slot `i`, the logical/data segment is `S=i+1`. A split process uses
backing segment `S+NPROC` for instructions. `u.u_sep` records the layout;
`sureg()` selects the instruction bank and sets `useg`/`iseg` for kernel copies.
The emulator routes instruction fetches and PC-relative program accesses
through this selection, while data and stack accesses retain their original
mapping. The kernel stays combined. Fork copies the mapped text, data and stack sections;
exec can change between layouts. Text is neither shared nor write-protected.
This mapping is implemented in the emulated machine; FPGA hardware still needs
an equivalent instruction/data bus mapping.

`copyiin`/`copyiout` and `fuibyte`/`suibyte` access instruction backing memory.
The loader uses the instruction copy path for text and the data path for data
and BSS. It checks header, entry point, file length, and layout before replacing
the old image. PCC places dense switch tables in data space because generated
indirect loads use the data bus. The linker also rejects overflowing layouts
before truncating header fields or symbol values.

`test-split` runs a program with more than 64 KB of total static storage,
including a function and PC-relative constant above address 0x8000. It covers
BSS, initialized data, switch tables, file I/O, signals, fork isolation, failed
exec, and transitions between combined and split programs. It also runs the
libc and signal suites as 0411 binaries and checks linker overflow rejection.

### Shared user-copy policy and machine-helper contract

`passc()`, `cpass()` and `iomove()` now follow V7 transfer policy. `u_segflg`
selects user data (0), kernel memory (1), or user instructions (2). Aligned,
even-length user transfers use the corresponding bulk helper; other transfers
use the byte routines, including kernel-memory transfers. `iomove(..., 0, ...)`
has no effect. Callers supply a nonnegative length no greater than `u_count`;
`passc()` requires a nonzero remaining count.

The replaceable machine layer must implement these return conventions:

| Helpers | Success | Failure |
|---|---|---|
| `fubyte`, `fuibyte` | Unsigned byte, 0–255 | Negative value |
| `subyte`, `suibyte` | Zero | Negative value |
| `copyin`, `copyout`, `copyiin`, `copyiout` | Zero after the whole transfer | Nonzero |

A failed byte access sets `EFAULT` without advancing `u_base`, `u_count` or
`u_offset` for that byte. Earlier successful bytes remain accounted. A failed
bulk operation sets `EFAULT` without advancing any of those fields for that
operation. Its destination may already contain a copied prefix: the V7 bulk
interface has no residual count and does not promise rollback. Successful
operations update the three fields exactly once. `passc()` also returns -1
when the last requested byte succeeds; `u_error` distinguishes failure.

`machine/krt.s` now rejects bulk ranges crossing the 64 KB boundary and odd
word addresses (including a word at `0xffff`). Zero-length bulk copies do not
access memory; a final byte at `0xffff` remains valid. `rdwr()` calls the selected
MMU's `useracc()` before starting a read/write, preventing a long request from
wrapping across multiple buffer-cache blocks or byte transfers.

The current MMU maps private data/stack read/write and shared text read-only and leaves unused pages
unmapped. `useracc()` checks address wrap and every covered data-space page,
including a request spanning the heap/stack gap. A null pointer is not inherently
unmapped when data starts at zero. Future protected MMUs must also check access
permissions.

The SEGTRAP vector now enters the runtime at `0x020a`, saves the same registers
as syscall entry, and calls `segtrap()`. For a kernel fault, `ufixups` recognizes
only the saved PCs immediately following the ten user-access instructions.
Z8001 SEGT is accepted after the instruction, as specified in the CPU manual
(section 7.3.4 and the interrupt transaction description). The handler redirects
IRET to the matching recovery label. That label restores the helper caller's
FCW, including interrupt enables, and unwinds the helper frame with return -1.
No global recovery pointer or shared continuation is needed. Kernel faults
outside those sites panic; user faults use the stack-growth checks above or normal SIGSEGV delivery.

Exec argument-vector faults now return `EFAULT`; a fault after exec has replaced
the old image takes the existing fatal-image path. Signal-frame store failures
terminate with SIGSEGV instead of returning into an incomplete frame. EPU-state
restoration copies into a temporary buffer first, so a failed copy cannot leave
partially restored state.

This mechanism requires a functioning system stack and MMU hardware that
suppresses invalid accesses and raises SEGT. It does not recover arbitrary
kernel bugs, supply demand paging, or provide general instruction restart.

`test-copy` compiles the actual three shared functions with the real target
headers and user structure, relocating `u` into the test program and replacing
only machine helpers. It runs 655 cases in each executable layout: all three
spaces, both directions, odd/even addresses and lengths, zero count, high-bit
bytes, 32-bit offset carry, injected failures and an address-space boundary.
Failures are synthetic; existing boot, libc, TTY and split-I/D suites exercise
the kernel with the actual machine helpers.

`test-fault` adds 30 guest scenarios across combined and split I/D layouts.
They test range rejection, zero-length and last-byte accesses, actual bus faults
in byte/bulk copies, mid-transfer accounting, pathnames, exec vectors, EPU
restoration, signal stacks, split-text loading and direct user accesses.
Each recovery case checks subsequent syscalls and clock progress. The emulator
option `-F r:hex`, `-F w:hex` or `-F u:hex` denies an access at that offset after
the `-w` marker: r/w target kernel segmented user accesses; u targets user-mode
accesses. The bus suppresses the access and requests the CPU's real SEGTRAP
path. This is test-only fault injection, not a new production MMU permission
register. Ordinary runs have no injected denied addresses.

### Kernel split I/D layout

The kernel is also linked as 0411, with 16-bit pointers. Its instruction space
uses logical segment 1, backing map 126 and physical bank 2 (frames 64–95).
Data starts at logical `1:0000`, backed by bank 1; the copy window and u-area
remain at `e000` and `f000`. ROM programs the I map before entering the kernel.
`krt.s` reserves the first 512 instruction bytes for the separately assembled
trap stubs; `handler.bin` begins at physical `2:0200`, and `handler-data.bin`
is loaded at physical `1:0000`. The PSA copy lives in ROM at `0:1000` because
vector fetches use the data bus. Kernel instruction storage counts against
installed RAM. Images and the emulator must be rebuilt together.

### Stack faults and protection

`machine/mmu.h` defines the board contract. PAGEFRAME bits 15 and 14 mean
read-only and system-only; `ffff` remains unmapped. Read-only applies to system
writes too. Kernel/EPU maps are system-only; shared 0411 text is read-only.
Normal processes remain NONSEG, so their ordinary accesses select their own
logical segment. This does not add a segmented user ABI.

The MMU observes first-word instruction fetch (external status 1101) and latches
that logical PC, the fault segment, low/high access offsets and reasons. Ports
`c0/c2/c4/c6/c8/ca` expose those six words; `cc` acknowledges them. Reason bits
are VALID=1, WRITE=2, READ=4, FETCH=8, UNMAP=16, PROT=32, WARN=64, MIXED=128.
`d0/d2` select a logical segment and its stack-warning base; `ffff` disables it.
A normal-mode store into the lowest 256 bytes of the mapped stack succeeds and
raises a warning. Growth is attempted when SP approaches that boundary; a warning
never turns an otherwise valid store into SIGSEGV. Invalid or protected accesses are suppressed and raise SEGT.
Different instructions/segments in one unacknowledged report set MIXED.

Z8001 SEGT occurs after instruction completion. It is **not** Z8003/4 ABORT.
`stackfault()` accepts warnings without replay, or grows and retries a small
whitelist of nonsegmented instructions: LD/LDB/LDL stores, LDM stores, CALL/CALR,
and register/immediate PUSH through R15. PUSH sources containing R15 are rejected.
CALL/PUSH restore the implicit SP decrement before replay. Failed reads, fetches,
read-modify-write operations, mixed reports, protection faults and unsupported
instructions receive SIGSEGV. Growth also checks that the fault lies at or above
the actual SP in the stack gap; arbitrary heap faults do not allocate memory.
Signal delivery reserves stack space before building its frame. This is safe
software backout for supported cases, not general demand paging or CPU rollback.

### Shared text and swapping

`sys/text.c` owns inode-backed text references, resident counts and immutable
swap copies. The original V7 `struct text` is retained, including 64-byte click
units for `x_size`; physical allocation rounds it to 2 KiB pages. Exec prepares the shared 0411 text before replacing the old layout;
fork shares it; exit/exec drop references. ITEXT and inode references prevent
writes while executable text is in use, including swapped-out users. Executing
an inode already open for writing returns ETXTBSY. The last ordinary reference
releases RAM, swap and the inode. Sticky text retains its inode and swap image,
releasing RAM; `xrele()` and `xumount()` remove unused cached images. Failure to
obtain or write swap backing drops an unused cache entry instead of panicking.
Partial loads and traced text are never cached. A new text entry publishes its
inode and XLOCK ownership before sleeping for memory, so concurrent execs cannot
claim the same free entry.

The board supplies a separate ATA unit (major 1, minor 1) for swap; root remains
unit 0. Port `b2` reports its size in 512-byte blocks. Process 0 now runs V7's
`sched()` separately from `swtch()`. The latter selects only resident runnable
processes and restores their saved contexts; it performs no allocation or I/O.
The original `runout` arrival wakeups and `runin` resource/timer wakeups are active.
Background swap-in selects by time out adjusted for nice; eviction prefers the
largest eligible sleeper/stopped process, then resident age plus nice. The original
three-second-out/two-second-resident gates apply to runnable victims.

Contiguous extent growth still prepares all replacement sections before changing
the old image. A failed immediate `corealloc()` posts a pinned reservation request
to process 0 and sleeps. `corework()` services these requests ahead of background
swap-in, using shared `swapvict()` ranking without age delays. It reserves the
actual extent for the requester, skips failed victims, and reports failure if no
eligible victim remains. This is a deliberate adaptation to multiple independent
extents, rather than copying PDP-11 `expand()`'s self-swap of a resized contiguous
image. CMAPSIZ covers four committed and three provisional extents per process,
all text entries, and map termination/headroom (150 entries here). This bounds
concurrent sleeping resizes despite V7 mfree having no overflow check. Fork can still write a child directly to swap if two resident copies do
not fit; the parent remains pinned through the copy and reference updates.

`swapio()` serializes the 512-byte bounce buffer and sleeps on interrupt-driven
completion at PSWP. Other resident processes can execute during these waits.
The buffer owner is pinned even while waiting to acquire it. Physical copies
retain bounded 16-byte interrupt masking. Swap-out clears SLOAD before the first
transfer so the victim cannot run against a partial snapshot; failure restores
residency and frees the provisional swap extent. Swap-in keeps the image
nonresident until all reads succeed, and frees provisional frames on failure.
A failed private-image read also releases newly loaded unused text with valid
swap backing. Transient read errors retain the old swap image for retry; no
permanent-device-failure recovery or bad-block remapping is claimed.

XLOCK spans text allocation, transfers and resident-count changes. The swapper
does not wait on a text lock whose owner may need a reservation from process 0;
locked incoming images use a timed runin retry instead of waiting indefinitely
for another runout arrival. Raw I/O and explicit process locks exclude victims.

Two progress safeguards accommodate immediate-completion controllers and the
emulator's accelerated clock. SREADY protects a new resident image until swtch
first dispatches it, and sched sleeps on runin after successful swap-in so
resident user work and tracing handoffs can progress. The clock or ordinary
sleep/free events wake it. These are explicit additions to original V7, whose
swap loop assumes useful execution opportunities during physical disk waits.
Without them the contention tests exposed repeated eviction without progress.

This remains whole-process swapping, not demand paging. Contiguous-section
fragmentation and temporary resize reservations can cause ENOMEM; exhausted
swap returns allocation failure rather than overwriting the root disk or
panicking. Asynchronous bus-map ownership remains absent.
