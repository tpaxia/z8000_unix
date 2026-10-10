# Z8000 Software Emulator

The project uses a Z8000 software emulator (`z8000_emu/`) for development and testing. The emulator supports both the Z8001 (segmented) and Z8002 (non-segmented) CPU variants and is linked as a C++ library into test drivers.

[Z8001-unix in MAME](z8001-unix.md) implements the same kernel-facing board
interface. `test_driver -b ROM` boots its ROM loader instead of directly loading
the kernel and FPU images. `-T 66667` selects approximately 60 Hz at 4 MHz;
the default remains the accelerated 5,000-cycle test clock.

The `z8002-mmu` configuration builds the same harness for a Z8002 with
external mode-selected contexts and a 1 MiB physical range. Its additional
registers, boot images and validation are described in
[the Z8002 reference](z8002-mmu.md).

## Custom Front End

Rather than using the emulator as a standalone tool, the project links it as a library and implements a custom front end (`emu/test_driver.cpp`). It lives outside `v7z8000/` because it is host C++ modelling the machine, not Unix source — keeping it separate means everything under `v7z8000/` stays a diff against the V7 baseline.

This front end can:

- Load binary images at arbitrary physical addresses in the Z8001's 8MB address space, without needing bootstrap code
- Model the paged MMU (128 segments x 32 pages x 2KB) with the UPAGE and WPAGE remap ports
- Register I/O ports for simulated devices: console TTY, IDE/ATA hard drive, RAM disk DMA controller
- Raise interrupts on the CPU — NVI for the clock tick, VI for disk completion and console receive
- Run the CPU with a cycle limit and inspect final register state
- Capture device output for automated test verification
- Enable instruction, register, and memory tracing for debugging

### Raising interrupts

Devices signal the CPU with `pulse_input_line(line, vector)`, which latches an
interrupt request without holding the line asserted. This matters: NVI and VI
are level sensitive, and `CHANGE_FCW` re-latches a pending request whenever the
handler's `IRET` re-enables NVIE or VIE. A device that held the line would
therefore re-enter its own handler forever. Use `set_input_line()` only for a
source that genuinely holds a level until serviced.

## Installed RAM size

`test_driver -R KiB` selects installed low RAM in even KiB increments from
128 to 8192 (default 8192). Read-only port 0x00BA reports the number of 2 KB
frames to the kernel. The backing vector covers the bus address space, but
MMU accesses to absent physical RAM are suppressed and raise SEGTRAP; they
cannot wrap into available RAM. The dedicated EPU bank at 0x7f0000 remains
available independently, and its stack pages still alias the current u-area.

The kernel reserves 192 KiB for ROM/kernel and allocates user text, data and
stack as page-rounded extents, plus a 4 KiB u-area per process. `test-memory`
exercises both layouts at 320, 322, 384 and 8192 KiB, failed allocation/exec,
heap growth/shrink, invalid gaps and shell workspace crossing page boundaries.
PAGESEL (0xBC) and PAGEFRAME (0xBE) program mappings; user banks start unmapped.

Reports distinguish `Absent RAM accesses` from `Unmapped accesses`. Both raise
SEGTRAP, but the latter includes expected user gap faults and invalid-pointer
tests. Successful ordinary workloads should report zero for both. Stack probes intentionally produce warnings and supported gap faults. Shared text
has read-only protection; private data remains writable. See the [memory contract](../kernel/memory-and-swapping.md#physical-memory-sizing-and-resource-maps).

### Shared text, faults and swap device

`-S KiB` creates the dedicated, ephemeral ATA secondary unit used for swap
(default 4096 KiB; maximum 16000 KiB). Zero disables the device, but the
V7 exec argument reservation then panics at boot with `Out of swap`. `-o` saves only
the root unit. Swap traffic never uses the root filesystem's blocks. Final
statistics report swap sectors read/written, peak simultaneous read-only text
mappings, protection faults and stack warnings.

Read-only normal word port `0x00b6` reports root disk sectors, clamped to
65535. A reserved tail enables [kernel panic dumps](../kernel/devices-and-io.md#kernel-written-crash-dumps);
`-o` preserves that tail with the root image.

`-K physical-core` saves raw installed low RAM and `-W saved-swap` saves the
secondary ATA unit when the run stops. Both capture the same stopped CPU state,
including after a panic halt; `-o` remains the root-disk save. Keep the matching
unstripped `/unix` namelist with these files. See
[`ps k` dump inspection](../kernel/devices-and-io.md).

The kernel now uses split I/D; rebuild `kernel.bin`, `handler.bin`,
`handler-data.bin`, ROM and test_driver together. RAM below the 192 KiB fixed
reservation is rejected before boot; the kernel also rejects RAM insufficient
for the initial process. Usable program limits depend on contiguous allocations
and temporary growth reservations, not only total free bytes.

`test-memory` covers growth/backout, rejection of unsafe read-modify-write
replay, shared text/inode write exclusion, low-RAM swapping and full swap.
See the [MMU and swap contract](../kernel/memory-and-swapping.md#stack-faults-and-protection).

## Terminal startup tests

`-7` strips software parity in the displayed/captured console; the default
eight-bit transport remains available for raw-mode tests. `-q text` selects the
prompt at which initial input starts. `-A file` sends an ordered sequence of
`marker<TAB>input` lines, recognizing `\n` escapes in both fields; later markers
are searched only in output following the previous action. It waits 100 clock
ticks after a marker before sending that stage’s input.

For multiuser tests use a clock period near the MAME machine’s 4 MHz / 60 Hz
ratio (`-T 66667`). The default accelerated 5000-cycle clock can expire original
V7 login alarms during otherwise normal filesystem work. See
[multiuser startup](../development/multiuser.md).
