# Z8001-unix MAME integration

- `z8001unix.cpp`: MAME driver implementing the current kernel board interface.
- `build.py`: stage the driver in an isolated MAME checkout and build it.
- `build_rom.py`: build the small firmware, primary disk bootstrap and V7 standalone `/boot`.
- `test.py` and `smoke.lua`: disposable-disk console regression runner.

The dedicated MAME branch is `z8001_unix`, built in
`~/Projects/mame_latest/mame-z8001-unix`.

See [machine contract](../doc/platforms/z8001-unix.md) and
[build/run procedure](../doc/development/mame.md).

`install_boot.py` installs `/boot`, `/unix`, `/fpe` and sector zero into a new disk copy.
`check_disk.py` checks the installed boot files, sector list, preservation of existing files and allocation accounting.
