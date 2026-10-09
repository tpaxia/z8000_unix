#!/usr/bin/env python3
"""Package a native-built V7 userland disk with original multiuser startup."""
from pathlib import Path
import argparse
import sys
import subprocess
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/native-cc'))
from build import image
from selfhost import Filesystem
WORK = ROOT / 'tests/build/multiuser'


def build(source=None, destination=None, extra_files=None, extra_modes=None, extra_owners=None):
    source = Path(source or ROOT / 'tests/build/userland-native-sout/hd.img')
    WORK.mkdir(parents=True, exist_ok=True)
    fs = Filesystem(source)
    files, modes, owners = {}, {}, {}

    def walk(number, path):
        if path == 'tmp' or path == 'usr/src':
            return
        offset = ((number + 15) // 8) * 512 + ((number + 15) % 8) * 64
        mode = int.from_bytes(fs.disk[offset:offset + 2], 'big')
        modes[path] = mode
        owners[path] = tuple(int.from_bytes(fs.disk[offset+i:offset+i+2], 'big') for i in (4, 6))
        if mode & 0o170000 == 0o040000:
            data = fs.data(number)
            for i in range(0, len(data), 16):
                child = int.from_bytes(data[i:i+2], 'big')
                name = data[i+2:i+16].split(b'\0')[0].decode()
                if child and name not in ('.', '..'):
                    walk(child, path + '/' + name if path else name)
        elif mode & 0o170000 == 0o100000:
            dest = WORK / 'files' / path
            dest.parent.mkdir(parents=True, exist_ok=True)
            dest.write_bytes(fs.data(number))
            files[path] = dest
    walk(2, '')
    # These programs have already been built by native make inside Unix.
    for target, old in [('etc/init', 'etc/init.v7'), ('etc/getty', 'bin/getty'),
                        ('etc/update', 'bin/update'), ('etc/cron', 'bin/cron')]:
        if old in files:
            files[target] = files.pop(old)
        modes[target] = 0o755
        owners[target] = (0, 0)
    for name in ('rc', 'ttys', 'passwd', 'group', 'motd'):
        files['etc/' + name] = ROOT / 'v7z8000/etc' / name
        modes['etc/' + name] = 0o644
        owners['etc/' + name] = (0, 0)
    files['.profile'] = ROOT / 'v7z8000/etc/root.profile'
    modes['.profile'] = 0o644
    # Empty accounting/configuration files must exist before original login/cron.
    empty = WORK / 'empty'; empty.write_bytes(b'')
    for path in ('etc/utmp', 'usr/adm/wtmp', 'usr/lib/crontab',
                 'usr/spool/mail/.keep', 'usr/tmp/.keep', 'usr/sys/.keep'):
        files[path] = empty
        modes[path] = 0o644
        owners[path] = (0, 0)
    for path in ('bin/su', 'bin/passwd', 'bin/newgrp'):
        modes[path] = 0o4755
        owners[path] = (0, 0)
    # Pair inspection tools with the currently running kernel.
    files['unix'] = ROOT / 'v7z8000/usr/sys/build/handler.sout'
    files.update(extra_files or {})
    modes.update(extra_modes or {})
    owners.update(extra_owners or {})
    destination = Path(destination or WORK / 'hd.img')
    destination.parent.mkdir(parents=True, exist_ok=True)
    image(files, destination, blocks=32000, inodes=2048, modes=modes, owners=owners)
    # Keep panic dumps outside the filesystem (8 MiB RAM plus 4 MiB swap).
    with destination.open('ab') as disk:
        disk.truncate(32000*512 + 16*1024*1024)
    print('Multiuser disk:', destination)
    return destination


def rebuild_startup(source=None):
    """Compile the original startup programs under Unix, then export s.out files."""
    files, commands = {}, []
    programs = ('init', 'getty', 'login', 'update', 'cron', 'su', 'passwd', 'ps')
    for name in programs:
        files['usr/src/startup/' + name + '.c'] = ROOT / 'v7z8000/usr/src/cmd' / (name + '.c')
        commands.append('0 - /bin/cc -O -i -s /usr/src/startup/' + name + '.c -o /tmp/new' + name)
    # Use the current bootstrap ABI, as in the native toolchain seed.
    for name in ('libc.a', 'crt0.b'):
        files['lib/' + name] = ROOT / 'tests/build/sout-cc' / name
    plan = WORK / 'startup-plan'; WORK.mkdir(parents=True, exist_ok=True)
    plan.write_text('\n'.join(commands) + '\n')
    files['usr/lib/startup-plan'] = plan
    disk = build(source, WORK / 'rebuild.img', extra_files=files)
    system = ROOT / 'v7z8000/usr/sys/build'
    with (WORK / 'rebuild.log').open('wb') as log:
        result = subprocess.run([str(system / 'test_driver'), '-7', '-d', str(disk),
            '-i', 'runner /usr/lib/startup-plan /tmp\n', '-x', 'NATIVE CC DONE',
            '-c', '200000000000', '-o', str(WORK / 'rebuilt.img')],
            cwd=system, stdout=log, stderr=subprocess.STDOUT, timeout=1200)
    output = (WORK / 'rebuild.log').read_bytes()
    if result.returncode or b'NATIVE CC PASS' not in output or b'FAILED command' in output:
        raise RuntimeError('Native startup rebuild failed: ' + str(WORK / 'rebuild.log'))
    fs = Filesystem(WORK / 'rebuilt.img')
    exported = {}
    for name in programs:
        target = ('etc/' if name in ('init', 'getty', 'update', 'cron') else 'bin/') + name
        path = WORK / 'native' / target; path.parent.mkdir(parents=True, exist_ok=True)
        data = fs.read('/tmp/new' + name)
        if data[:2] != b'\xe7\x11': raise ValueError('Not split s.out: ' + name)
        path.write_bytes(data); exported[target] = path
    for name in ('libc.a', 'crt0.b'):
        exported['lib/' + name] = files['lib/' + name]
    return exported


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path)
    parser.add_argument('--output', type=Path)
    parser.add_argument("--rebuild-startup", action="store_true", help="recompile startup programs inside Unix")
    args = parser.parse_args()
    files = rebuild_startup(args.source) if args.rebuild_startup else None
    build(args.source, args.output, extra_files=files)


if __name__ == '__main__':
    main()
