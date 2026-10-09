# Build and run Z8001-unix in MAME

Build the ordinary `emulated` kernel and a root filesystem using
[bootstrap](bootstrap.md). The driver and bootstrap tools are in `mame/`.
The tested MAME CPU/worktree baseline is commit `9119a0e7ad6` on
[tpaxia/mame's z8001_unix branch](https://github.com/tpaxia/mame/tree/z8001_unix),
based on revision `e1b99a60ff5`; `build.py` installs the current driver from
`mame/z8001unix.cpp`. The base includes the Z8001
CPU fixes, first-word instruction-fetch interface and normal/system output used
by this machine. Compatibility with arbitrary upstream releases is not claimed.
See the [machine reference](../platforms/z8001-unix.md#boot) for the boot sequence,
historical V7 sources and current limits.

## Build and install

The local MAME branch is `z8001_unix` in the permanent worktree
`~/Projects/mame_latest/mame-z8001-unix`. Its executable is
`~/Projects/mame_latest/mame-z8001-unix/z8001unix`. This worktree belongs to the
`tpaxia/mame` repository; the original checkout remains on `m40_z8010_sup_test`.

From the Unix repository root, set:

```sh
MAME_UNIX="$HOME/Projects/mame_latest/mame-z8001-unix"
CHDMAN="$HOME/Projects/mame_latest/mame/chdman"
git -C "$MAME_UNIX" branch --show-current
```

That should report `z8001_unix`. On a different machine, create an equivalent
worktree once from the published branch (skip this for the existing local worktree;
these commands assume `origin` points to `tpaxia/mame`):

```sh
git -C "$HOME/Projects/mame_latest/mame" fetch origin z8001_unix
git -C "$HOME/Projects/mame_latest/mame" worktree add -b z8001_unix \
  "$MAME_UNIX" origin/z8001_unix
```

`build.py` copies the driver from the Unix repository into this MAME branch,
adds its `mame.lst` entry and compiles there. It does not change CPU sources.
Repeat it after changing `mame/z8001unix.cpp`; a plain MAME build alone does not
synchronize the source copy.

```sh
cmake --build v7z8000/usr/sys/build --target kernel test_driver
python3 mame/build_rom.py
python3 mame/build.py "$MAME_UNIX" -j 8
python3 tools/multiuser.py --rebuild-startup
python3 mame/install_boot.py tests/build/multiuser/hd.img \
  tests/build/z8001unix/full-userland.img
```

The last command installs disk boot into the [multiuser runtime image](multiuser.md),
which packages the complete native userland disk from
[the native rebuild procedure](native-rebuild.md#full-native-userland).
It includes the normal V7 commands, manuals, libraries and native compiler.
The runtime image already includes `/.profile` to configure Backspace.
For a console-init build fixture, `--console-profile` installs that configuration.
The compiler seed at
`tests/build/native-cc-sout/hd.img` is suitable for compiler regressions but lacks
normal commands such as `ls`; it is not the interactive testing disk.
It creates a **new copy**, adds `/boot`, `/unix` and `/fpe` through the V7 free
block/inode lists, then installs sector zero's `/boot` block list. The source
image is unchanged. The destination must not exist and the source must not
already contain `/boot` or `/fpe`; a matching, unbooted development `/unix` is
reused. The reserved crash-dump tail is preserved. Allow roughly 140 free disk blocks;
the tiny kernel regression root image does not have room for these files.
To verify preservation of existing files and free-list accounting:

```sh
python3 mame/check_disk.py tests/build/userland-native-sout/hd.img \
  tests/build/z8001unix/full-userland.img
```

Input images must be offline, unmounted V7 filesystems. Trailing unwritten
sectors of sparse mkfs output are padded to the superblock's declared size.

The focused binary is `$MAME_UNIX/z8001unix`. The 2 KiB firmware is
`tests/build/z8001unix/roms/z8001unix/unix.rom`; it contains no kernel/FPU payload.
MAME reports `NO GOOD DUMP KNOWN` for this locally built firmware.
`build_rom.py` also produces the standalone loader, disk boot block and disk
files. Firmware and the primary block use host builds of the shared native
asz8k/ldz8 sources, without GNU Z8000 tools. The standalone loader uses PCC
and the shared s.out assembler/linker. These are host bootstrap builds; the native
compiler subsequently runs inside Unix.

## Run

For the 120,000-sector complete native userland image (create the CHD once, then run it):

```sh
"$CHDMAN" createhd -i tests/build/z8001unix/full-userland.img \
  -o tests/build/z8001unix/full-userland.chd -chs 120000,1,1 -ss 512 -c none
"$MAME_UNIX/z8001unix" z8001unix -window \
  -rompath tests/build/z8001unix/roms \
  -hard tests/build/z8001unix/full-userland.chd
```

The local `full-userland.chd` is already prepared from the full native image. Skip
`createhd` when reusing it; choose a new output filename for a fresh disk.

For another disk, set the cylinder count to its byte size divided by 512;
one head and one sector preserve LBA numbering. At the `: ` prompt press Return
for `/unix`, or type `hd(0,0)/unix` explicitly. A backup kernel can be selected
as `hd(0,0)/ounix`. A bare `/unix` is not a valid device-qualified pathname.
Backspace and Delete erase characters at the boot prompt. The CHD is writable;
changes persist. Swap is separate and cleared on reset. `-ram 320k` exercises
low-memory swapping (this MAME version uses a RAM slot option).

The standalone emulator can boot the identical firmware and raw disk:

```sh
cd v7z8000/usr/sys/build
./test_driver -b ../../../../tests/build/z8001unix/roms/z8001unix/unix.rom \
  -d ../../../../tests/build/z8001unix/full-userland.img -T 66667 -c 4000000000 \
  -i 'cc -i /usr/src/hello.c -o /tmp/hello\n/tmp/hello\n' \
  -x 'Hello from native C'
```

The standalone test harness supplies Return to `/boot`, then sends `-i` input
at the shell prompt. Without `-b`, the existing direct-load regression path
remains available.

## Automated checks

The runner makes a disposable writable CHD, selects the default kernel at the
loader prompt, types shell commands and requires expected guest output. Python
prepares media and launches MAME; the native compilation below runs inside Unix.
Use expected output that does not occur merely in the echoed command line.

```sh
python3 mame/test.py "$MAME_UNIX/z8001unix" --chdman "$CHDMAN" \
  --input 'cc -i /usr/src/hello.c -o /tmp/hello
/tmp/hello
' --expect 'Hello from native C' --seconds 1200 \
  --output tests/build/z8001unix/branch-native
```

The executable rebuilt in the permanent `z8001_unix` worktree passed this
check: the disk-loaded guest compiled, linked and ran `/tmp/hello`, printing
`Hello from native C`. Logs are in `tests/build/z8001unix/branch-native/`.

For other existing regression disks, first run `install_boot.py SOURCE NEW-DISK`
and pass `--disk NEW-DISK` to the runner. Build their source images with the
corresponding kernel regression targets first.

| Source image | Input | Expected output |
|---|---|---|
| `tests/build/fpe/hd.img` | `runner` | `EPU RESULT 0` |
| `tests/build/memory/hd.img` | `pagesi` | `pages: passed` |
| `tests/build/memory/hd.img` | `textn` | `text: passed` |
| `tests/build/memory/hd.img` with `--ram 320k` | `memoryi 12` | `memory: passed` |

End input commands with a literal newline. Keep separate `--output` directories;
console and MAME logs are retained there. `--seconds` is the emulated timeout;
allow 1800 seconds for the low-memory workload. These disk-boot tests passed in
MAME, including native compilation, page faults/stack growth, shared text,
320 KiB swapping and FPU tests in both executable layouts. The standalone
emulator also passed disk-loaded FPU tests at the board-rate clock. A corrupted
kernel header was rejected at the MAME loader prompt, and a missing primary
signature stopped the standalone emulator in firmware.

`--save-disk path.img` exports the modified disk after a successful test and
refuses overwrite. Use `--settle 60` for periodic Unix buffer flushing before
MAME exits; this is a test convenience, not an orderly shutdown protocol.

The full native disk passed `ls /bin`, `pwd`, a shell pipeline and native
compile/link/execute in MAME after installing the disk bootstrap. Logs are in
`tests/build/z8001unix/full-userland-default/`. Use `ls /bin` for the installed
command list; [userland coverage](../toolchain/userland.md) records functionality
and platform-specific limitations. Installed commands are not all independently
validated in MAME.
