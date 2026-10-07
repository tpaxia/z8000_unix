# Kernel Overview

The kernel keeps shared V7 services in `sys/`, reusable device/TTY code in
`dev/`, interfaces in `h/`, Z8000 and MMU mechanisms in `machine/`, and machine
selection/device tables in `conf/`. The separate software EPU is under `fpe/`.
The build selects and links implementations; there is no runtime driver loader.

| Subject | Current reference |
|---|---|
| CPU entry, interrupts, software EPU | [Traps and interrupts](traps-and-interrupts.md) |
| Mapping, user access, shared text, swapping | [Memory and swapping](memory-and-swapping.md) |
| Exec, signals, core, tracing, process services | [Processes and exec](processes-and-exec.md) |
| Device tables, TTY, buffered/raw I/O | [Devices and I/O](devices-and-io.md) |
| Calling convention, headers and archives | [ABI](../toolchain/abi.md) |
| New board or MMU | [Porting guide](../platforms/porting-guide.md) |

The [source-adjacent configuration reference](../../v7z8000/usr/sys/conf/README.md)
defines exact selection variables and machine interfaces.

## Boot Flow (V7 Kernel)

```
ROM reset → seg0:0x0010 (init)
  → set PSAP to seg1:0x0000, system stack RR14 = seg1:0xFFF0
  → set NSP = 0xFFF0
  → IRET to seg1:0x01F0 (NONSEG+SYS)

seg1:0x01F0 (trap.s boot entry):
  → call 0x0202

seg1:0x0202 (krt.s boot_entry):
  → zero BSS (_edata.._end)
  → ld sp, #0xFFFE    (kernel stack at top of u-area page)
  → FCW = 0x5000      (NONSEG+SYS, devices enabled, clock not yet)
  → calr _main

main() (sys/main.c):
  → proc[0] setup: p_stat=SRUN, p_flag=SLOAD|SSYS, p_addr=62
  → u.u_procp = &proc[0], u.u_error = 0
  → rootdev = makedev(1, 0)   — the IDE hard disk; also pipedev, swapdev
  → printf("boot\n")
  → clkstart()      — enable the clock
  → cinit()         — clist free list
  → binit()         — init 8-buffer cache, count block devices
  → iinit()         — open block device, bread superblock, mount root
  → iget(ROOTINO)   — load root inode
  → namei("/dev/console") — walk root→dev→console via bread/bmap/iget
  → open1()         — falloc(), openi(), cdevsw[0].d_open()
  → dup fd 0 → fd 1, fd 2
  → printf("Z8000 Unix\n")
  → newproc()       — fork process 1
    → child: copyout(icode) → return → krt.s → retu() → user mode
    → parent: swtch() → resumes child → child runs icode
  → icode: exec("/etc/init") → init execs /bin/sh
```
