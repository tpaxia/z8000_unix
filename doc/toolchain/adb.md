# V7 adb on Z8000

The debugger retains V7's command language, expressions, maps, symbol lookup
and process-control framework. Its machine layer reads linked NONSEG s.out
executables, uses the public Z8000 register/core layout and handles big-endian
memory. Both e707 combined-space and e711 split-I/D programs are supported.
SEG user execution remains unsupported.

## Processes and core files

`adb executable [core]` reads the executable's global symbols. Process cores
use the [u-area/data/stack layout](../kernel/processes-and-exec.md#core-dumps),
with exact file-length validation. `/` reads data; `?` reads instructions.
`$r` shows R0–R15, FCW, PC segment and PC offset. Register expressions accept
`r0` through `r15`, `fcw`, `seg`, `pc`, and aliases `sp`, `fp`, `ps`.
For example, `0x5555>r10` writes R10 and `<r10=x` displays it.

`$c` walks saved R13 frames and resolves global function names. It has no
argument-count or local-variable debug records; frameless assembly routines
can omit callers from the displayed chain. A non-increasing frame chain is
rejected. `$f` displays the saved software EPU registers as raw words.
Memory formats `f` and `F` interpret the port's IEEE single and double values.

`address:b`, `:r` and `:c` provide one-shot software breakpoints using SC 255.
On a hit, adb restores the original instruction and resets PC to its address;
continuation executes that instruction normally. The breakpoint is consumed
and must be explicitly set again. Breakpoint counts other than one are rejected.
`:s` reports that hardware single-step support is required, without starting
or resuming a process. This matches the machine's unsupported ptrace request 9.

The instruction display covers the base CPU instruction families in NONSEG and
SEG mode: arithmetic, logical/bit operations, all load/addressing forms, stack
operations, branches, rotates/shifts, string operations, I/O and privileged
control instructions. SEG direct/indexed addresses consume their short or long
encoding; indirect and base addresses use register pairs. Port addresses and
port registers retain their word format in either mode.

`0$z` selects NONSEG decoding; `1$z` selects SEG decoding; `$z` reports the
current mode. This affects `/i` and `?i` without changing memory maps. Saved
kernel FCW initializes the mode; `$r` uses the saved FCW when displaying PC.
For mapped assembly routines that switch modes, select the mode explicitly at
the boundary. SEG executable loading and SEG user process control remain unsupported.

The CPU-defined EPA templates display as `epu`, including transfer direction,
register/memory operands, counts and the raw EPU operation fields. The decoder
does not assign mnemonics to implementation-specific EPU operations. Reserved
first words remain `.word` and advance one word; invalid extension words already
read are included in that raw display. Truncated instructions report a map/read
error. CALR displays its signed encoded displacement because historical assembler
dialects disagree with the manual's target calculation.

## Kernel RAM images

`adb -k /unix /usr/sys/core` uses the matching kernel namelist and saved physical
RAM. `/` initially maps kernel data offsets below `0xe000` to physical bank 1;
`?` maps instructions in `/unix`. The RAM length must match the saved `physmem`.
Use V7's `/m` map command for other physical regions. For example,
`0/m 0 0x80000 0` maps a 512 KiB RAM image directly by physical byte address.

Kernel-written panic dumps contain a versioned 64-byte `_kcrash` record.
`$r` displays R0–R15, FCW and PC; `$c` starts from the saved R13. The recorded
UPAGE frame maps the interrupted kernel stack at `0xf000`–`0xffff`. Stack walks
stop at user interrupt/syscall boundaries and follow the interrupted PC across
kernel interrupt wrappers. Frameless assembly still limits unwinding.

For an access fault, registers come from the original hardware/assembly trap
frame. PC names the faulting instruction's first word from the MMU latch;
`trap_pc` separately retains the hardware return PC. Registers reflect the
completed instruction, including its side effects. A direct `panic()` saves
its caller's registers before the C prologue, with PC at the return site and
SP above the call's return address. The first context survives recursive panic.

Older or pre-panic emulator RAM captures can lack a context; globals and explicit
frame walks remain available, but `$r` cannot fabricate registers. Invalid
context versions or stack mappings are rejected. Kernel floating registers
and process-control commands remain unavailable. Process and swap interpretation is
provided by [`ps k`](../kernel/devices-and-io.md), rather than automatic adb
process-context selection. See [crash recovery](../development/crash-dumps.md)
for kernel-written disk dumps and the native recovery utility.
