#!/usr/bin/env python3
"""Exercise syscall numbers and exec environments in combined and split I/D."""
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
tools = root / 'tools'
pcc = root / 'PCC-z8000/z8000'
build = Path(sys.argv[1]).resolve()
work = root / 'tests/build/abi'
work.mkdir(parents=True, exist_ok=True)
def run(args, **kw):
    r = subprocess.run(list(map(str, args)), capture_output=True, **kw)
    if r.returncode:
        sys.stdout.buffer.write(r.stdout + r.stderr)
        r.check_returncode()
    return r.stdout
run(['make', '-C', tools, 'libv7.a', 'libc/crt0.b', 'sh', 'init', 'v7mkfs'])
pre = run(['cpp', '-nostdinc', '-undef', '-Dz8000', '-Dz8002',
           '-I' + str(root / 'v7z8000/usr/include'), tools / 'abitest.c'])
(work / 'abi.az8').write_bytes(run([pcc / 'cz8/cz8'], input=pre))
run([pcc / 'az8/az8', '-o', 'abi.b', 'abi.az8'], cwd=work)
asm = '.text\n.globl _errno\n'
for name, number, nargs in [('rawex11', 11, 3), ('rawmask', 60, 1),
                             ('rawroot', 61, 1), ('oldphys', 52, 0)]:
    asm += f'.globl _{name}\n_{name}:\n'
    for arg in range(1, nargs + 1):
        asm += f'ld r{arg},{arg * 2}(r15)\n'
    asm += f'sc #{number}\ncp r0,#0xffff\njr ne,.L{name}\nld _errno,r1\n.L{name}:\nret\n'
(work / 'raw.az8').write_text(asm)
run([pcc / 'az8/az8', '-o', 'raw.b', 'raw.az8'], cwd=work)
(work / 'empty').write_bytes(b'')
for layout in ['combined', 'split']:
    run([pcc / 'ldz8', '-x', *(['-i'] if layout == 'split' else []),
         tools / 'libc/crt0.b', work / 'abi.b', work / 'raw.b', tools / 'libv7.a',
         '-o', work / 'abitest'])
    (work / 'proto').write_text(f'''boot
4000 128
d--755 0 0
bin d--755 0 0
sh ---755 0 0 {tools}/sh
abitest ---755 0 0 {work}/abitest
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
jail d--755 0 0
inside ---644 0 0 {work}/empty
$
$
''')
    run([tools / 'v7mkfs', work / 'hd.img', work / 'proto'])
    r = subprocess.run([str(build / 'test_driver'), '-c', '1200000000',
        '-d', str(work / 'hd.img'), '-P', str(work / (layout + '.tsv')), '-i', '/bin/abitest\\n', '-w', 'abi: passed',
        '-I', 'exit\\n', '-x', 'abi: passed'], cwd=build, capture_output=True, timeout=60)
    (work / (layout + '.log')).write_bytes(r.stdout + r.stderr)
    if r.returncode or b'abi: FAIL' in r.stdout:
        sys.stdout.buffer.write(r.stdout + r.stderr)
        raise SystemExit(layout + ': ABI regression failed')
    profile = (work / (layout + '.tsv')).read_text()
    assert '\t/bin/abitest\t' in profile, 'execve profiler record missing'
    assert '\t/etc/init\t' in profile, 'boot exec profiler record missing'
    print(layout + ': ABI passed', flush=True)
