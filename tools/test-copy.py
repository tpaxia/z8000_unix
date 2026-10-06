#!/usr/bin/env python3
"""Execute real kernel copy policy with injectable machine helpers on Z8000.

This tests dispatch/accounting, not hardware protection or fault recovery.
The tested functions are extracted afresh; no duplicate policy implementation.
"""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
tools = root / 'tools'
pcc = root / 'PCC-z8000/z8000'
build = Path(sys.argv[1]).resolve()
work = root / 'tests/build/copy'
work.mkdir(parents=True, exist_ok=True)

def run(args, **kw):
    result = subprocess.run(list(map(str, args)), capture_output=True, **kw)
    if result.returncode:
        sys.stdout.buffer.write(result.stdout + result.stderr)
        result.check_returncode()
    return result.stdout

kernel = root / 'v7z8000/usr/sys'
subr = (kernel / 'sys/subr.c').read_text()
rdwri = (kernel / 'sys/rdwri.c').read_text()
headers = ''.join(f'#include "{kernel}/h/{h}.h"\n'
                  for h in ['param', 'dir', 'user', 'buf'])
headers += '#undef u\nstruct user u;\n'
source = headers + subr[subr.index('passc(c)'):subr.index('/*\n * Routine which sets')]
source += rdwri[rdwri.index('iomove(cp, n, flag)'):]
source += (tools / 'copytest.c').read_text()
(work / 'policy.c').write_text(source)
run(['make', '-C', tools, 'libv7.a', 'libc/crt0.b', 'sh', 'init', 'v7mkfs'])
pre = run(['cpp', '-nostdinc', '-undef', '-Dz8000', '-Dz8002', work / 'policy.c'])
(work / 'policy.az8').write_bytes(run([pcc / 'cz8/cz8'], input=pre))
run([pcc / 'az8/az8', '-o', 'policy.b', 'policy.az8'], cwd=work)
for name, flags in [('copyn', []), ('copyi', ['-i'])]:
    run([pcc / 'ldz8', *flags, '-x', tools / 'libc/crt0.b', work / 'policy.b',
         tools / 'libv7.a', '-o', work / name])
(work / 'proto').write_text(f'''boot
800 64
d--755 0 0
bin d--755 0 0
sh ---755 0 0 {tools}/sh
copyn ---755 0 0 {work}/copyn
copyi ---755 0 0 {work}/copyi
$
dev d--755 0 0
console c--644 0 0 0 0
tty c--644 0 0 2 0
$
etc d--755 0 0
init ---755 0 0 {tools}/init
$
$
''')
run([tools / 'v7mkfs', work / 'hd.img', work / 'proto'])
for name in ['copyn', 'copyi']:
    result = subprocess.run(
        [str(build / 'test_driver'), '-c', '400000000', '-d', str(work / 'hd.img'),
         '-i', name + '\\nexit\\n', '-x', 'copy: 655 cases, 0 failures'],
        cwd=build, capture_output=True, timeout=60)
    (work / (name + '.log')).write_bytes(result.stdout + result.stderr)
    if result.returncode or b'copy: FAIL' in result.stdout:
        sys.stdout.buffer.write(result.stdout + result.stderr)
        raise SystemExit(f'{name}: failed')
    print(f'{name}: 655 copy-policy cases passed')
