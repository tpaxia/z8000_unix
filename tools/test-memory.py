#!/usr/bin/env python3
"""Test V7 resource maps and fork storage at real emulated RAM limits."""
from pathlib import Path
import re
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
tools = root / 'tools'; pcc = root / 'PCC-z8000/z8000'
kernel = root / 'v7z8000/usr/sys'
build = Path(sys.argv[1]).resolve(); work = root / 'tests/build/memory'
work.mkdir(parents=True, exist_ok=True)
def run(args, **kw):
    r = subprocess.run(list(map(str, args)), capture_output=True, **kw)
    if r.returncode:
        sys.stdout.buffer.write(r.stdout + r.stderr); r.check_returncode()
    return r.stdout
run(['make', '-C', tools, 'libv7.a', 'libc/crt0.b', 'sh', 'init', 'cat', 'echo', 'v7mkfs'])
headers = '#define malloc rmalloc\n#define mfree rmfree\n#define time maptime\n'
headers += ''.join(f'#include "{kernel}/h/{h}.h"\n' for h in ['param', 'systm', 'map', 'proc', 'dir', 'user', 'text'])
headers += '#include "' + str(kernel / 'machine/mmu.h') + '"\n'
headers += '#undef u\nstruct user u;\n'
allocator = (kernel / 'sys/malloc.c').read_text()
allocator = allocator[allocator.index('/*'):]
paged = (kernel / 'machine/paged.c').read_text()
helpers = paged[paged.index('/* Physical frames'):paged.index('/*\n * sureg()')]
init = paged[paged.index('mmuinit()'):paged.index('/* Copy the saved continuation')]
(work / 'maps.c').write_text(headers + allocator + helpers + init + (tools / 'maptest.c').read_text())
(work / 'stackregs.az8').write_bytes((tools / 'stackregs.az8').read_bytes())
run([pcc / 'az8/az8', '-o', 'stackregs.b', 'stackregs.az8'], cwd=work)
for name, src in [('maps', work / 'maps.c'), ('memory', tools / 'memtest.c'), ('pages', tools / 'pagetest.c'), ('text', tools / 'texttest.c'), ('swapfork', tools / 'swapfork.c')]:
    pre = run(['cpp', '-nostdinc', '-undef', '-Dz8000', '-Dz8002',
               '-I' + str(root / 'v7z8000/usr/include'), src])
    (work / (name + '.az8')).write_bytes(run([pcc / 'cz8/cz8'], input=pre))
    run([pcc / 'az8/az8', '-o', name + '.b', name + '.az8'], cwd=work)
    for layout in ['n', 'i']:
        run([pcc / 'ldz8', '-x', *(['-i'] if layout == 'i' else []),
             tools / 'libc/crt0.b', work / (name + '.b'),
             *([work / 'stackregs.b'] if name == 'pages' else []), tools / 'libv7.a',
             '-o', work / (name + layout)])
(work / 'huge.az8').write_text('.text\n.zerow 24000\n.bss\n.comm _big,48000\n')
run([pcc / 'az8/az8', '-o', 'huge.b', 'huge.az8'], cwd=work)
run([pcc / 'ldz8', '-x', '-i', tools / 'libc/crt0.b', work / 'memory.b',
     work / 'huge.b', tools / 'libv7.a', '-o', work / 'huge'])
files = '\n'.join(f'{n}{l} ---755 0 0 {work}/{n}{l}' for n in ['maps', 'memory', 'pages', 'text'] for l in ['n', 'i'])
payload = 'abC123' * 500
(work / 'workspace').write_text("x='" + payload + "'\necho \"$x\" > /tmp/word\ncat > /tmp/here <<'END'\n" + payload * 2 + "\nEND\npagesn workspace\n")
(work / 'proto').write_text(f'''boot
4000 128
d--755 0 0
bin d--755 0 0
sh ---755 0 0 {tools}/sh
cat ---755 0 0 {tools}/cat
echo ---755 0 0 {tools}/echo
{files}
huge ---755 0 0 {work}/huge
$
dev d--755 0 0
console c--644 0 0 0 0
tty c--644 0 0 2 0
$
etc d--755 0 0
init ---755 0 0 {tools}/init
$
tmp d--777 0 0
workspace ---644 0 0 {work}/workspace
$
$
''')
run([tools / 'v7mkfs', work / 'hd.img', work / 'proto'])
def guest(label, ram, command, verdict, reject=False, swap=0):
    r = subprocess.run([str(build / 'test_driver'), '-c', '3000000000' if not reject else '1000000',
        '-R', str(ram), '-S', str(swap), '-d', str(work / 'hd.img'), '-i', command + '\\n',
        '-w', verdict, '-I', 'exit\\n', '-x', verdict], cwd=build, capture_output=True, timeout=90)
    output = r.stdout + r.stderr
    (work / (label + '.log')).write_bytes(output)
    ok = (r.returncode != 0 and b'insufficient memory' in output) if reject else (
        r.returncode == 0 and b': FAIL' not in output and b'Absent RAM accesses: 0' in output)
    if not ok:
        sys.stdout.buffer.write(output); raise SystemExit(label + ': failed')
    print(label + ': passed', flush=True)
nproc = int(re.search(r'#define\s+NPROC\s+(\d+)', (kernel / 'h/param.h').read_text())[1])
for layout in ['n', 'i']:
    guest('maps-' + layout, 8192, 'maps' + layout, 'maps: passed')
    guest('pages-' + layout, 8192, 'pages' + layout, 'pages: passed')
    for ram in [320, 322, 384]:
        guest(f'memory-{layout}-{ram}', ram, f'memory{layout} {nproc-4 if layout == "i" and ram == 384 else -1}', 'memory: passed')
    guest('memory-' + layout + '-8192', 8192, f'memory{layout} {nproc - 4}', 'memory: passed')
guest('text-lifecycle', 8192, 'textn', 'text: passed')
guest('too-small', 136, '', 'unused', reject=True)
guest('too-small-init', 200, '', 'unused', reject=True)
for ram in [320, 322]:
    guest(f'exec-denied-{ram}', ram, 'memoryn deny', 'execmem: passed')
guest('exec-layout-reuse', 320, 'memoryn flip 8', 'execmem: passed')
guest('shell-workspace', 8192, 'sh /tmp/workspace', 'workspace:passed')
assert b'Unmapped accesses: 0' in (work / 'shell-workspace.log').read_bytes()

for layout in ['n', 'i']:
    for ram in [320, 322]:
        label = f'swap-{layout}-{ram}'
        guest(label, ram, f'memory{layout} {nproc-4}', 'memory: passed', swap=4096)
        log = (work / (label+'.log')).read_text()
        counts = re.search(r'Swap sectors: (\d+) read, (\d+) written', log)
        assert counts and min(map(int, counts.groups())) > 0, log

        if layout == 'i':
            shared = re.search(r'Shared text peak mappings: (\d+)', log)
            assert shared and int(shared[1]) > 1, log

guest('swap-full', 320, 'memoryn -1', 'memory: passed', swap=16)

# Boot the large probe directly as /bin/sh: each private image fits in 64 KiB
# of available RAM, but two copies do not. This forces direct-to-swap fork.
proto = (work / 'proto').read_text().replace(str(tools / 'sh'), str(work / 'swapforkn'))
(work / 'fork-proto').write_text(proto)
run([tools / 'v7mkfs', work / 'fork.img', work / 'fork-proto'])
r = subprocess.run([str(build/'test_driver'), '-c', '2000000000', '-R', '256',
    '-S', '4096', '-d', str(work/'fork.img'), '-i', '', '-x', 'swapfork: passed'],
    cwd=build, capture_output=True, timeout=90)
(work/'fork-direct.log').write_bytes(r.stdout+r.stderr)
if r.returncode or b'swapfork: FAILED' in r.stdout:
    sys.stdout.buffer.write(r.stdout+r.stderr); raise SystemExit('direct swap fork failed')
print('direct swap fork: passed', flush=True)
