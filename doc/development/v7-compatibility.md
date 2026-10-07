# Compatibility with Original Unix V7

The checked-in `v7unix/` tree is the pristine reference; `v7z8000/` is the port.
The policy is to retain original shared code unchanged wherever possible and
isolate required CPU, MMU and device differences behind documented interfaces.
See [current status](../status.md) for overall completeness and the
[restoration record](../history/v7-restoration.md) for audit evidence and tests.

## Shared code restored

Restoration covers signal exit status, user-copy policy and error accounting,
filesystem/device-close interfaces, common TTY control flow, ordinary buffer
cache, resource maps, shared-text lifecycle, process/swapper policy, raw physio,
core policy, ptrace requests 0–8, exec credentials, accounting, profiling and
residency locking. Exec arguments now use original V7 swap-backed staging;
the earlier serialized kernel argument buffer has been removed.

Examples recorded as byte-identical include alloc, prim, pipe, sys3, sys4, fio,
nami and common partab, along with several original headers. These are examples of restored files, not a complete source inventory.
Kernel ABI headers are exported to user space and checked by the build:

```sh
python3 tools/export-headers.py --check
```

## Required adaptations and remaining departures

| Area | Current disposition |
|---|---|
| CPU ABI | Z8000 register/trap conventions, context labels, signals and EPU state |
| Executables | Port a.out layout and NONSEG 0407/0411 loading; no full SEG support |
| Disk representation | Big-endian three-byte inode address conversion |
| Memory | 2 KiB physical allocation units behind 64-byte V7 accounting clicks; separate u-area and section extents |
| Swapping | Original sched policy with machine transfer services, extent reservations and progress safeguards |
| Faults | Fault-safe user access and conservative backout; arbitrary instruction restart absent |
| Tracing | Requests 0–8; request 9 requires hardware support |
| Startup | Small console init, with original multiuser startup not integrated |
| Optional facilities | Disabled multiplexor stubs, no active channel device; no bus-map implementation |
| Panic | Ordinary update/flushing restored elsewhere; panic-specific flushing remains deferred |
| Memory device | Only minor-2 EOF/rathole behavior; physical/kernel-memory minors reject open |

There are no compatibility aliases for earlier port syscall-number mistakes.
Rebuild libc, programs and kernel together after ABI changes; see
[user startup and migration](../kernel/processes-and-exec.md#user-program-startup).
The structure-return investigation documents an inherited ABI limitation and
[a proposed repair](../toolchain/structure-return-abi.md), not an implemented change.

## Userland source preservation and execution coverage

The essential-userland audit covers 158 top-level original command units,
762 files: none missing and 722 byte-identical. Changes are confined to ar,
make archive handling, pstat, shell files, yacc configuration, dc's free-list
termination, lint alignment and nm/prof/strip/file format handling. These are
source-preservation counts, not counts of working installed commands.

```sh
python3 tools/native-cc/userland.py --audit
```

The inventory is written to `tests/build/userland/audit.json`. The
[native development reference](../toolchain/native-development.md) lists the
45 unchanged command sources built and tested by that workload. Libc mknod/stime
wrappers and shared brk/sbrk bookkeeping supply missing port support without
changing those commands. Native ar/make use portable archives intentionally;
original V7 binary archives are not the target format.
The [full userland inventory](../toolchain/userland.md) additionally builds
160 command executables, seven games, 12 libraries and 12 terminal tables, while recording
the PDP-11 implementations and machine integrations that remain unfinished.
