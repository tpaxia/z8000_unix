#!/usr/bin/env python3
"""Set-ID exec, tracing, signal dispositions, CPU startup and core policy."""
from pathlib import Path
import re
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
tools = root / 'tools'
pcc = root / 'PCC-z8000/z8000'
build = Path(sys.argv[1]).resolve()
work = root / 'tests/build/exec'
work.mkdir(parents=True, exist_ok=True)
def run(args, **kw):
    r = subprocess.run(list(map(str, args)), capture_output=True, **kw)
    if r.returncode:
        sys.stdout.buffer.write(r.stdout + r.stderr)
        r.check_returncode()
    return r.stdout
run(['make', '-C', tools, 'libv7.a', 'libc/crt0.b', 'sh', 'init', 'v7mkfs'])
for name, source in [('exec', 'exectest.c'), ('args', 'argtest.c')]:
    pre = run(['cpp', '-nostdinc', '-undef', '-Dz8000', '-Dz8002',
               '-I' + str(root / 'v7z8000/usr/include'), tools / source])
    (work / (name + '.az8')).write_bytes(run([pcc / 'cz8/cz8'], input=pre))
    run([pcc / 'az8/az8', '-o', name + '.b', name + '.az8'], cwd=work)
(work / 'bad').write_bytes(b'not an executable\n')
for layout in ['combined', 'split']:
    run([pcc / 'ldz8', '-x', *(['-i'] if layout == 'split' else []),
         tools / 'libc/crt0.b', work / 'exec.b', tools / 'libv7.a', '-o', work / 'exectest'])
    run([pcc / 'ldz8', '-x', *(['-i'] if layout == 'split' else []),
         tools / 'libc/crt0.b', work / 'args.b', tools / 'libv7.a', '-o', work / 'argtest'])
    # mkfs represents the set-ID bits as the second and third mode characters.
    (work / 'proto').write_text(f'''boot
2400 128
d--755 0 0
bin d--755 0 0
sh ---755 0 0 {tools}/sh
arga ---755 0 0 {work}/argtest
argb ---755 0 0 {work}/argtest
argc ---755 0 0 {work}/argtest
exectest ---755 0 0 {work}/exectest
plain ---755 11 21 {work}/exectest
suid -u-755 11 21 {work}/exectest
sgid --g755 11 21 {work}/exectest
both -ug755 11 21 {work}/exectest
root -u-755 0 21 {work}/exectest
bad -ug755 11 21 {work}/bad
denied -ug700 11 21 {work}/exectest
$
dev d--755 0 0
console c--644 0 0 0 0
tty c--644 0 0 2 0
$
etc d--755 0 0
init ---755 0 0 {tools}/init
$
tmp d--777 0 0
$
$
''')
    run([tools / 'v7mkfs', work / 'hd.img', work / 'proto'])
    for ram in [8192, 320]:
        r = subprocess.run(list(map(str, [build / 'test_driver', '-c', '3000000000',
            '-R', ram, '-S', 4096, '-d', work / 'hd.img', '-i', '/bin/exectest\\n',
            '-w', 'exec: passed', '-I', 'exit\\n', '-x', 'exec: passed'])),
            cwd=build, capture_output=True, timeout=90)
        output = r.stdout + r.stderr
        label = f'{layout}-{ram}'
        (work / (label + '.log')).write_bytes(output)
        if r.returncode or b'exec: FAIL' in output:
            sys.stdout.buffer.write(output)
            raise SystemExit(label + ': exec regression failed')
        assert b'Absent RAM accesses: 0' in output
        print(label + ': exec passed', flush=True)
        r = subprocess.run(list(map(str, [build / 'test_driver', '-c', '3000000000',
            '-R', ram, '-S', 4096, '-D', 50000, '-d', work / 'hd.img', '-i', '/bin/arga\\n',
            '-w', 'args: passed', '-I', 'exit\\n', '-x', 'args: passed'])),
            cwd=build, capture_output=True, timeout=90)
        output = r.stdout + r.stderr
        (work / (label + '-args.log')).write_bytes(output)
        if r.returncode or b'args: FAIL' in output:
            sys.stdout.buffer.write(output)
            raise SystemExit(label + ': argument staging regression failed')
        assert b'Absent RAM accesses: 0' in output
        sectors = re.search(rb'Swap sectors: (\d+) read, (\d+) written', output)
        assert sectors and min(map(int, sectors.groups())) > 0
        print(label + ': swap-backed arguments passed', flush=True)

    # Only one reservation fits: a leak on any failed exec prevents the next.
    r = subprocess.run(list(map(str, [build / 'test_driver', '-c', '3000000000',
        '-R', 8192, '-S', 6, '-d', work / 'hd.img', '-i', '/bin/arga serial\\n',
        '-w', 'args: passed', '-I', 'exit\\n', '-x', 'args: passed'])),
        cwd=build, capture_output=True, timeout=90)
    output = r.stdout + r.stderr
    (work / (layout + '-args-reuse.log')).write_bytes(output)
    if r.returncode or b'args: FAIL' in output:
        sys.stdout.buffer.write(output)
        raise SystemExit(layout + ': argument reservation reuse failed')
    print(layout + ': single-reservation reuse passed', flush=True)

# V7 reserves ten argument blocks even for init's null argv.
for size in [0, 5]:
    r = subprocess.run(list(map(str, [build / 'test_driver', '-c', '10000000',
        '-S', size, '-d', work / 'hd.img', '-i', '', '-x', 'panic: Out of swap'])),
        cwd=build, capture_output=True, timeout=30)
    output = r.stdout + r.stderr
    (work / ('swap-short-' + str(size) + '.log')).write_bytes(output)
    assert b'panic: Out of swap' in output, output
    print('exec reservation exhaustion: ' + str(size) + ' KiB passed')
