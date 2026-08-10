# Step 7: V7 Kernel — Process 0, Filesystem, /dev/console

Replaced the test syscall handler with a real V7 kernel that boots to process 0, mounts a root filesystem from a RAM disk, opens `/dev/console`, and prints a message. This proves the entire V7 filesystem + buffer cache + device driver stack works end-to-end.

## What Was Built

- Imported V7 kernel headers (`kernel/h/`) adapted for Z8000: reduced table sizes (NBUF=8, NPROC=4, NINODE=16, NFILE=16, NMOUNT=2), removed PDP-11 MMU fields from `user.h`, stubbed `seg.h` and `acct.h`.
- Imported V7 kernel source (`kernel/sys/`): `bio.c` (buffer cache), `alloc.c` (block/inode allocation), `iget.c` (inode read/write with big-endian 3-byte address conversion), `nami.c` (pathname resolution), `rdwri.c` (read/write I/O), `subr.c` (bmap, bcopy), `fio.c` (file descriptor operations), `prf.c` (printf/panic).
- Created `kernel/sys/main.c`: simplified V7 main — no fork/exec/sched, just process 0 setup, `binit()`→`iinit()`→`iget()`→`namei()`→`open1()`→`printf("Z8000 Unix\n")`→`idle()`.
- Created `kernel/sys/machdep.c`: machine-dependent stubs — `sleep()` (panic), `wakeup()` (no-op), `spl0/spl6/splx()` (no-ops), `plock/prele()` (flag set/clear), `cinit()`, `bzero()`, `xrele()`.
- Created device drivers (`kernel/dev/`): `md.c` (RAM disk via I/O port DMA), `cons.c` (console character device), `conf.c` (bdevsw/cdevsw tables).
- Extended `kernel/krt.s`: added BSS zeroing loop, `putchar()` (alias for `putc()`), `inb()`/`outb()` I/O port wrappers, `idle()` halt.
- Extended `kernel/test_driver.cpp`: DMA controller for RAM disk (I/O ports 0xE0-0xE5), disk image loading.
- Used `tools/v7mkfs` (built in a prior step) to create a minimal root filesystem image with `/dev/console` (major 0, minor 0) and `/dev/tty` (major 2, minor 0).

## Simplifications

Single-process bring-up with no user/kernel boundary:

- Everything runs in kernel space (`u_segflg` always 1). No copyin/copyout.
- `sleep()` panics (RAM disk is synchronous, should never block). `wakeup()` is a no-op.
- `spl0()`/`spl6()`/`splx()` are no-ops (no interrupts).
- No fork/exec/swap — just proc[0].

## RAM Disk I/O Architecture

The Z8000 kernel runs in NONSEG mode with a 64KB address space. The disk image can't live in this space alongside the kernel. Instead, the RAM disk driver uses I/O port-based DMA: it writes a block number and kernel buffer address to I/O ports (0xE0-0xE4), and the emulator performs the memory transfer between the disk image and the kernel's memory region. Reads return zero-filled blocks for addresses beyond the end of the disk image.

See [kernel-technical-reference.md](kernel-technical-reference.md) for the DMA port table.

## Key Bugs Fixed

Eight ACK compiler/assembler/runtime bugs were uncovered by compiling real V7 code. (ACK was the toolchain for Steps 1-10; it was replaced by PCC in Step 11 and has since been removed from the tree.)

1. **Assembler relocation bug**: `relonami` not reset between instruction operands, causing incorrect relocations in combined address+immediate instructions.
2. **`ldb` encoding**: assembler generated wrong opcodes for byte-register load instructions.
3. **Anonymous structs**: `user.h`/`inode.h` anonymous struct/union not supported by ACK's K&R frontend; named the inner structs.
4. **libem return addresses**: `cuu`, `cmi4`, `cms` used `popl` (4-byte pop) for return addresses, assuming z8001 segmented `call` which pushes 4 bytes. In z8002 mode, `calr` only pushes 2 bytes, corrupting the stack. Fixed to use `pop` (2-byte).
5. **libem `*SP` register encoding**: all 33 libem files were assembled without `-z8002`, so `*SP` mapped to R14 (z8001 segmented stack pointer RR14) instead of R15 (z8002 stack pointer). Rebuilt entire libem.a with `-z8002`.
6. **`cms.s` indirect addressing**: `*RR2` dereferences R2 (the even register) in z8002, not R3 as in z8001 segmented mode. Fixed to put the pointer in R2.
7. **`inb()` return register**: the assembly wrapper returned the value in R7, but ACK's calling convention returns in R0. Added `ld R0, R7`.
8. **BSS not zeroed**: C requires globals to be zero-initialized. Added a zeroing loop in `krt.s` before calling `_main`.

## Boot Flow

```
ROM reset → seg0:0x0010 (init)
  → set PSAP, system stack, NSP
  → IRET to seg1:0x0100 (NONSEG+SYS)
  → call 0x0200 (krt.s entry)
    → zero BSS
    → call main()
      → binit()         — init 8-buffer cache
      → iinit()         — bread superblock, mount root
      → iget(ROOTINO)   — load root inode
      → namei("/dev/console") — walk directory tree
      → open1()         — falloc, openi, cdevsw[0].d_open
      → dup fd 0 → fd 1, fd 2
      → printf("Z8000 Unix\n")
      → idle()          — halt
```

## Test

CPU halted, console output = "boot\nZ8000 Unix\n", no panics. PASS.
