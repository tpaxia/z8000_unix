# Native kernel and disk bootstrap

See the [native rebuild procedure](../../doc/development/native-rebuild.md).

`build.py` stages original kernel and standalone sources and the verified native
compiler, libc and commands. Native make first rebuilds cpp and verifies complete
macro names, including the distinct MMU stack selector/base registers. Kernel selection comes from CMake's
`native-sources.txt`, generated from the same ordered source list as the cross
build. The generated guest makefiles use native cc/asz8k/ldz8 exclusively.

`pack.c` performs source-boundary cuts, firmware padding, kernel vector/header
packaging, and sector-zero installation. Native sed adapts the preserved Zilog
arithmetic source with `fpe.sed`; no floating arithmetic is rewritten. The
standalone filesystem code is unchanged original V7 SYS.c, compiled through a
forward-declaration wrapper. The portable prf.c prefix is cut inside Unix.

Installation checks that `/boot` is a regular e707 image on the selected block
special device, reads its inode and direct/single-indirect sector addresses,
validates them against filesystem size, and writes only sector zero. The primary
loader's 63-sector limit applies. Replacing `/boot` requires installation again;
replacing `/unix` or `/fpe` does not. Work on a trial disk copy.

The host extracts completed artifacts and independently checks the boot-sector
list and cross/native object bytes. `test_driver -b` then starts with only the
native firmware loaded. Sector zero, `/boot`, `/unix` and `/fpe` come from the
native-installed disk. Runtime checks compile and execute floating and long
arithmetic, reject invalid package/install requests and repeat installation.

No target object, generated kernel/FPE assembly, kernel image, standalone loader
or boot firmware is supplied in a fresh native system seed. Host Python prepares
the source filesystem and controls/saves emulator runs.
