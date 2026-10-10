# Unix V7 MAME integration

- `unixv7/unixv7.cpp`, `unixv7/unixv7.h`: shared memory, terminal, clock, disk and swap implementation.
- `unixv7/unixv7_z8001.cpp`, `unixv7/unixv7_z8002.cpp`: CPU configurations, context selection and fault wiring.
- `build.py`: stage the driver in an isolated MAME checkout and build it.
- `build_rom.py`: build the small firmware, primary disk bootstrap and V7 standalone `/boot`.
- `test.py` and `smoke.lua`: disposable-disk console regression runner.

Both machines are maintained on MAME's
[`unixv7_demo` branch](https://github.com/tpaxia/mame/tree/unixv7_demo).
Use one worktree for both. `build.py` produces `unixv7_demo` with both machines;
`--machine z8001unix` or `--machine z8002unix` builds a focused executable.
The source files are installed under MAME's `src/mame/homebrew/`.
Boot sources are grouped under `boot/unixv7/`, with CPU-specific firmware
in `z8001/` and `z8002/`. Each machine needs its own
kernel and boot ROM; user executables and filesystem formats are shared.
Always launch MAME with `-window`.

See [Z8001 machine contract](../doc/platforms/z8001-unix.md),
[Z8002 machine contract](../doc/platforms/z8002-mmu.md) and
[build/run procedure](../doc/development/mame.md).

`install_boot.py` installs `/boot`, `/unix`, `/fpe` and sector zero into a new disk copy.
`check_disk.py` checks the installed boot files, sector list, preservation of existing files and allocation accounting.
