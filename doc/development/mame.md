# Build and run Z8001-unix in MAME

Build the ordinary `emulated` kernel and a root filesystem using
[bootstrap](bootstrap.md). The driver and bootstrap tools are in `mame/`.
The tested MAME base is tpaxia/mame revision `e1b99a60ff5`. It includes the Z8001
CPU fixes, first-word instruction-fetch interface and normal/system output used
by this machine. Compatibility with arbitrary upstream releases is not claimed.
See the [machine reference](../platforms/z8001-unix.md#boot) for the boot sequence,
historical V7 sources and current limits.

## Build and install

Use an isolated MAME checkout/worktree. `build.py` copies the driver into that
tree and adds its `mame.lst` entry; it does not change CPU sources.

```sh
cmake --build v7z8000/usr/sys/build --target kernel test_driver
python3 mame/build_rom.py
python3 mame/build.py /path/to/isolated/mame -j 8
python3 mame/install_boot.py tests/build/native-cc/hd.img \
  tests/build/z8001unix/boot-hd.img
```

The last command uses the native compiler disk built by the bootstrap procedure.
It creates a **new copy**, adds `/boot`, `/unix` and `/fpe` through the V7 free
block/inode lists, then installs sector zero's `/boot` block list. The source
image is unchanged. The destination must not exist and the source must not
already contain these three filenames. Allow roughly 140 free disk blocks;
the tiny kernel regression root image does not have room for these files.
To verify preservation of existing files and free-list accounting:

```sh
python3 mame/check_disk.py tests/build/native-cc/hd.img \
  tests/build/z8001unix/boot-hd.img
```

Input images must be offline, unmounted V7 filesystems. Trailing unwritten
sectors of sparse mkfs output are padded to the superblock's declared size.

The focused binary is `/path/to/isolated/mame/z8001unix`. The 2 KiB firmware is
`tests/build/z8001unix/roms/z8001unix/unix.rom`; it contains no kernel/FPU payload.
MAME reports `NO GOOD DUMP KNOWN` for this locally built firmware.
`build_rom.py` also produces the standalone loader, disk boot block and disk
files; it cross-builds bootstrap artifacts, not programs at guest runtime.

## Run

For the 6,000-sector native compiler image:

```sh
/path/to/chdman createhd -i tests/build/z8001unix/boot-hd.img \
  -o tests/build/z8001unix/root.chd -chs 6000,1,1 -ss 512 -c none
/path/to/isolated/mame/z8001unix z8001unix \
  -rompath tests/build/z8001unix/roms \
  -hard tests/build/z8001unix/root.chd
```

For another disk, set the cylinder count to its byte size divided by 512;
one head and one sector preserve LBA numbering. At the `: ` prompt press Return
for `/unix`, or type `hd(0,0)/ounix` to select a backup kernel. The CHD is writable;
changes persist. Swap is separate and cleared on reset. `-ram 320k` exercises
low-memory swapping (this MAME version uses a RAM slot option).

The standalone emulator can boot the identical firmware and raw disk:

```sh
cd v7z8000/usr/sys/build
./test_driver -b ../../../../tests/build/z8001unix/roms/z8001unix/unix.rom \
  -d ../../../../tests/build/z8001unix/boot-hd.img -T 66667 -c 500000000 \
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
python3 mame/test.py /path/to/isolated/mame/z8001unix --chdman /path/to/chdman \
  --input 'cc -i /usr/src/hello.c -o /tmp/hello
/tmp/hello
' --expect 'Hello from native C' --seconds 1200 \
  --output tests/build/z8001unix/diskboot-native
```

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
