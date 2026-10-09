# Z8001-unix in MAME

The `z8001unix` MAME machine implements the current Unix board interface. It
runs the same `emulated` kernel configuration and NONSEG user binaries as the
standalone emulator. No shared V7 kernel C or device drivers are changed for
MAME. The assembler/s.out migration is independent and remains on its branch.

The dedicated MAME branch is `z8001_unix`; its permanent local worktree and
build/test commands are recorded in the [MAME procedure](../development/mame.md).

## Machine

- Z8001 at 4 MHz, with a 60 Hz clock interrupt.
- Configurable low RAM: 320, 322 or 384 KiB, or 1, 2, 4 or 8 MiB; default 8 MiB.
- The existing 2 KiB paged MMU, separate instruction maps, u-area and copy
  windows, read-only/system-only protection, fault latches and stack warnings.
- Dedicated software EPU storage at physical `0x7f0000`, with its upper stack
  pages remapped to the current u-area.
- Byte console registers at `0xf0`/`0xf2`, connected to MAME's generic terminal.
- The kernel's ATA-style PIO register interface at `0x1f0`–`0x1f7`: root on unit
  zero, and a separate ephemeral 4 MiB swap unit. Root is a writable CHD image.
- Latched NVI clock requests; VI for disk completion and console input; SEGT
  for MMU faults. Interrupt acknowledgement clears the corresponding request;
  queued console data continues asserting VI until consumed.

The authoritative MMU register definitions and semantics remain in
[the memory reference](../kernel/memory-and-swapping.md) and
[`mmu.h`](../../v7z8000/usr/sys/machine/mmu.h).

This deliberately retains the existing kernel-facing console and disk
interfaces. It does not emulate a Z80-SIO/CTC board, or the Z8002-demo MMU.
The ROM provides a polling sector-read service and the kernel handoff. The legacy standalone harness's RAM-disk DMA
peripheral and fault-injection switches are not implemented in this driver;
the supported root/swap path is the ATA interface. MAME save states are not
supported.

## Boot

The boot chain follows V7's separation of firmware, primary bootstrap,
standalone loader, and kernel:

1. The 2 KiB ROM (604 bytes used) initializes mappings, reads sector zero into
   `3:fe00`, checks its signature and executes it. It contains no kernel or FPU
   image and knows nothing about the filesystem.
2. Sector zero loads `/boot` into bank 3 using an installer-generated sector
   list. It strips the e707 s.out header and segment descriptor and enters the standalone program.
3. `/boot` uses the original V7 `standalone/SYS.c` filesystem routines to open
   the selected kernel. Press Return at `: ` for `hd(0,0)/unix`, or enter another
   pathname such as `hd(0,0)/ounix`.
4. The Z8001 loader accepts the e711 s.out kernel layout, loads instruction
   bytes into physical bank 2, data/BSS into bank 1, and copies its vectors to
   RAM at `0:1000`. It reads the software EPU image from `/fpe` into bank 127,
   then establishes the normal kernel mappings and enters at `1:01f0`.

MAME does not preload kernel RAM or manufacture a CPU register state. The
standalone emulator's `-b ROM` option executes the same boot chain; its test
harness selects the default kernel at the loader prompt. `-T 66667` approximates
4 MHz/60 Hz; without it, the accelerated 5,000-cycle regression period remains.

The primary sector list is the deliberate difference from the PDP-11 V7
filesystem-searching boot block: moving or replacing `/boot` requires refreshing
sector zero. Replacing `/unix` or `/fpe` does **not** require a ROM rebuild or
primary-bootstrap update. These remain ordinary filesystem files. The installer
in `mame/install_boot.py` creates a new disk copy and refuses existing `/boot`
and `/fpe` files. It can reuse a matching development `/unix` in place, updating
only its entry point and reserved vectors; a different kernel is refused.
`/unix` retains global symbols for the kernel inspection tools. The loader reads
only text and data into memory. The [native rebuild](../development/native-rebuild.md#native-kernel-and-disk-bootstrap)
provides `pack install block.bin /boot /dev/hd0`, which regenerates sector zero
from the installed loader inode on the same filesystem.

The bootstrap supports root unit zero, filesystem offset zero, and the controller's
28-bit LBAs. `/boot` is limited to 63 sectors and text+data+BSS below `e000`;
the optimized native loader uses 6,400 bytes text, 1,280 data and 4,608 BSS. Temporary bank 3
requires at least 256 KiB during boot and becomes ordinary allocatable memory
after the handoff. The original standalone filesystem reader handles indirect
blocks; the host installer supports boot files through single indirection and
a root directory through ten direct blocks. Disk errors stop boot; invalid
kernel headers return to the pathname prompt. Firmware prints `E` on a disk
error or missing primary signature; the primary prints `B` for a bad sector
count or `/boot` header. Sector zero uses signature `0x5a39` and a list of
32-bit block numbers. Images using the earlier `0x5a38`/16-bit list require
reinstalling sector zero together with the new ROM; they are rejected rather
than interpreted as the new format.

Historical sources: [V7 boot(8)](../../v7unix/usr/man/man8/boot.8),
[primary PDP-11 bootstrap](../../v7unix/usr/mdec/hpuboot.s),
[standalone boot.c](../../v7unix/usr/src/cmd/standalone/boot.c), and
[SYS.c](../../v7unix/usr/src/cmd/standalone/SYS.c). `SYS.c` is compiled unchanged,
with a forward declaration supplied by the build wrapper. The portable prefix
of V7 `prf.c` is also reused; disk I/O, console and executable placement are
Z8001-specific. [The M20 example](../../m20/bootloader.s) instead uses BIOS calls
to read fixed raw sectors; it is not a full V7 filesystem bootstrap.

## Validation

Initial MAME bring-up passed the shell pipeline, a split-I/D program exceeding 64 KiB
in combined static storage, the floating-point vector/process/signal suite in
both layouts, shared-text lifecycle, and page protection/automatic stack growth
including rejection of unsafe read-modify-write replay. A 320 KiB run passed the
12-child split-I/D memory/swapping workload. Native PCC compiled, linked and
executed a C program inside the MAME guest.
The resulting writable disk was exported to raw format and the standalone
emulator successfully executed that MAME-built program using the same ROM.

The disk-bootstrap checks are recorded in the build procedure. These are
selected machine acceptance tests, not a claim that every existing regression
or a full native system rebuild has run in MAME.

See [build and run instructions](../development/mame.md).
