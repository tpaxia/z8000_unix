# Multiuser startup

The runtime image uses original V7 `init`, `getty`, `login`, `update` and
`cron`. Their shared C sources are unchanged. Compiler regression disks keep
`tools/init.c`, the small console-shell fixture.

After the [full native userland rebuild](native-rebuild.md#full-native-userland),
build the kernel and package a runtime disk:

```sh
cmake --build v7z8000/usr/sys/build --target kernel test_driver
python3 tools/multiuser.py --rebuild-startup
python3 tools/test-multiuser.py
```

The startup rebuild runs `/bin/cc` inside Unix; Python stages the sources,
exports the resulting s.out programs and constructs the filesystem. It uses
the current bootstrap libc/CRT ABI. The rest of the installed programs come
from `tests/build/userland-native-sout/hd.img`. No GNU Z8000 tools are involved.
The runtime disk is `tests/build/multiuser/hd.img`; tests use separate images.
It includes a 16 MiB reserved tail outside the filesystem for kernel crash dumps.
Omitting `--rebuild-startup` packages the startup programs already installed
in the full native userland image. Build fixture source trees and temporary
files are omitted from the runtime image; the compiler and headers remain.

For a standalone boot:

```sh
cd v7z8000/usr/sys/build
./test_driver -7 -T 66667 -d ../../../../tests/build/multiuser/hd.img -i $'\004' -c 2000000000 -x 'login: '
```

This scripted run sends Ctrl-D and stops at the login prompt. For interactive
use, install disk boot and launch MAME using
[the MAME procedure](mame.md), substituting the runtime disk above and omitting
`--console-profile`: this image already contains `/.profile`.
Always launch MAME with `-window`.

## Login and configuration

V7 init first runs the single-user shell. **Press Ctrl-D** at its empty prompt
to continue: init runs `/etc/rc`, reads `/etc/ttys`, then starts getty.
The original interactive shell's `exit` command returns to its command loop;
Ctrl-D closes the shell. The same Ctrl-D logs out a multiuser shell, after
which init clears its utmp entry, appends a wtmp logout and respawns getty.
Sending SIGHUP to process 1 returns to V7's shutdown/single-user sequence;
SIGINT rereads `/etc/ttys`.

The configuration sources are in `v7z8000/etc/`:

- `ttys`: `14console` enables the one console with original getty table `4`.
- `rc`: clears temporary files and session state, starts update and cron,
  and prints the date. The single-root emulated machine needs no `/usr` mount.
- `passwd` and `group`: root plus daemon/system accounts. Root has an empty
  initial password; run `passwd` after login to set it. Service accounts use
  an unusable password and are not interactive login accounts.
- `motd` and `root.profile`: login text and Backspace configuration.

Original getty/login use `#` to erase and `@` to kill input. The root login
profile changes shell erase to Backspace. User profiles can do the same.
Passwords use V7 DES crypt and its eight-character limit.
`su`, `passwd` and `newgrp` are installed setuid root; image creation preserves
set-ID bits and account ownership rather than discarding them.

`/etc/utmp` and `/usr/adm/wtmp` exist before login. Cron reads
`/usr/lib/crontab` and runs as uid 1, as in V7; the initial crontab is empty.
The machine supplies one terminal, so multiple simultaneous terminal logins
require additional device lines and entries in `/etc/ttys`.

The console transport retains eight-bit raw I/O. V7 getty supplies software
parity, so the standalone harness's `-7` models a seven-bit display and strips
parity only in its console presentation. MAME's terminal display does likewise.
The kernel opens no process-0 console descriptors: init and its children open
the terminal, allowing the last session close to reset terminal ownership and
its process group, as in original V7.

The session test builds its credential probe natively and checks bad passwords,
password changes and relogin, setuid su, terminal closure/ownership/process
groups, utmp/wtmp, and one instance of each daemon. It also checks SIGHUP
shutdown and the single-user/multiuser restart sequence. It uses a test account only
in disposable test images; that account is not installed in the runtime disk.
