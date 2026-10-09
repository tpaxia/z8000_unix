# Prepare and recover kernel crash dumps

The emulated configuration can write RAM and swap to a reserved disk tail.
Read the [dump contract](../kernel/devices-and-io.md#kernel-written-crash-dumps)
for sizing, completion and failure behavior. The disk must be offline while
preparing it. Keep the matching unstripped `/unix` alongside each recovered dump.

For an existing filesystem image and a 512 KiB machine with the default
4 MiB swap unit:

```sh
python3 tools/crash-dump.py reserve root.img root-with-dump.img --ram-kib 512
```

The output must be a new file. The tool pads unwritten sectors to the filesystem
boundary, then reserves the crash area; it does not enlarge the filesystem.
For MAME, install the disk bootstrap before reserving the tail and create the
CHD with the full resulting sector count. Use a supported MAME RAM setting
and reserve at least that amount. Always run MAME with `-window`.

Boot this disk at the intended RAM/swap sizes. The kernel announces
`Crash dump: hd0 block ...` when the area is large enough. After a panic, preserve
the modified root disk (`test_driver -o` for the standalone emulator). A successful
capture prints `panic dump: ... sectors saved`.

Reboot with enough filesystem space to hold the recovered RAM and swap files.
Create `/usr/sys` if absent, then run inside Unix:

```sh
savecore /dev/rhd /usr/sys
ps axlk /unix /usr/sys/core /usr/sys/swap
adb -k /unix /usr/sys/core
```

Inside adb, `$r` displays the saved kernel registers and `$c` walks the saved
kernel stack. Use the exact namelist from the crashed kernel.

`savecore` defaults to those device/directory arguments. It creates `core` and
`swap` with mode 0600 and overwrites existing files of those names; use a separate
directory when retaining previous captures. It rejects an absent or invalid
completion record. Recovery does not clear the disk record. Archive the matching
kernel namelist before changing `/unix`.

For host-side extraction from a saved raw disk, use a new output directory:

```sh
python3 tools/crash-dump.py extract root-after-panic.img recovered
```

Both recovery paths read the same kernel-written record. Emulator `-K`/`-W`
captures remain available for debugging independently of the disk writer.

## Regression

After preparing the native compiler seed and inspection tools:

```sh
cmake --build v7z8000/usr/sys/build --target test-inspection test-adb
```

`test-adb` compiles and links adb and savecore inside Unix. Runtime trials cover
one-shot breakpoint continuation, register changes, both process-core layouts,
and a real kernel panic under low-memory pressure followed by a fresh boot and
native disk recovery. Resident, swapped, stopped and zombie processes are checked.
Recovered files are compared with the on-disk payload; `ps k` and `adb -k` inspect
that saved state. Recovery checks mode 0600 even for existing output files.
Access-fault records are compared with the original saved trap frame. A direct
`panic()` from exhausted swap checks the caller snapshot and syscall boundary
unwinding. Both paths display registers and stacks through native adb. Context-free
RAM remains inspectable; corrupt context versions and stack mappings are rejected.
An injected swap-read error checks an incomplete dump and its
rejection by savecore. No emulator RAM/swap capture supplies these recovery tests.
The real polled transfer routine also runs against a controller fixture covering
busy, missing-DRQ and completion timeouts, read/write errors and successful transfers.
