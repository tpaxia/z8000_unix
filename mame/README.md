# Unix V7 MAME integration

- `z8001unix.cpp`, `z8002unix.cpp`: Z8001 and Z8002 drivers implementing the Unix board interfaces.
- `build.py`: stage the driver in an isolated MAME checkout and build it.
- `build_rom.py`: build the small firmware, primary disk bootstrap and V7 standalone `/boot`.
- `test.py` and `smoke.lua`: disposable-disk console regression runner.

Both machines are maintained on MAME's
[`unixv7_demo` branch](https://github.com/tpaxia/mame/tree/unixv7_demo).
Use one worktree for both; `build.py --machine z8001unix` or
`--machine z8002unix` selects the driver and focused executable.
The drivers currently remain separate source files. Each machine needs its own
kernel and boot ROM; user executables and filesystem formats are shared.
Always launch MAME with `-window`.

See [Z8001 machine contract](../doc/platforms/z8001-unix.md),
[Z8002 machine contract](../doc/platforms/z8002-mmu.md) and
[build/run procedure](../doc/development/mame.md).

`install_boot.py` installs `/boot`, `/unix`, `/fpe` and sector zero into a new disk copy.
`check_disk.py` checks the installed boot files, sector list, preservation of existing files and allocation accounting.
