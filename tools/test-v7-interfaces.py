#!/usr/bin/env python3
"""Run filesystem convergence and common TTY ioctl contract tests on target."""
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[1]
tools=root/'tools'; pcc=root/'PCC-z8000/z8000'
build=Path(sys.argv[1]).resolve(); work=root/'tests/build/v7-interfaces'
work.mkdir(parents=True,exist_ok=True)
(work / 'asz8k.pd').write_bytes((tools / 'asz8k/src/asz8k.pd').read_bytes())
def run(args,**kw):
    r=subprocess.run(list(map(str,args)),capture_output=True,**kw)
    if r.returncode:
        sys.stdout.buffer.write(r.stdout+r.stderr);r.check_returncode()
    return r.stdout
run(['make','-C',tools,'libv7.a','libc/crt0.b','sh','init','v7mkfs'])
kernel=root/'v7z8000/usr/sys'
headers=''.join(f'#include "{kernel}/h/{h}.h"\n' for h in ['param','dir','user','tty','conf'])
headers+='#undef u\nstruct user u;\n'
source=(kernel/'dev/tty.c').read_text()
source=source[source.index('ttioccomm(com, tp, addr, dev)'):source.index('/*\n * Wait for output to drain')]
(work/'ttioc.c').write_text(headers+source+(tools/'ttioc-test.c').read_text())
for name,src in [('v7fs',tools/'v7fstest.c'),('ttioc',work/'ttioc.c')]:
    pre=run(['cpp','-nostdinc','-undef','-Dz8000','-Dz8002','-I'+str(root/'v7z8000/usr/include'),src])
    (work/(name+'.az8')).write_bytes(run([pcc/'cz8/cz8'],input=pre))
    run([tools.parent / 'tests/build/asz8k-host/asz8k', '-c','-o',name+'.b',name+'.az8'],cwd=work)
# Syscall 56 should reach the original disabled-multiplexor EINVAL stub.
(work/'probe.az8').write_text('''.text
.globl _mpxprob
_mpxprob:
sc #56
cp r0,#0xffff
jr ne,.Lbad
cp r1,#22
jr ne,.Lbad
clr r0
ret
.Lbad:
ld r0,#1
ret
''')
run([tools.parent / 'tests/build/asz8k-host/asz8k', '-c','-o','probe.b','probe.az8'],cwd=work)
for name in ['v7fs','ttioc']:
    run([tools.parent / 'tests/build/ldz8-host/ldz8','-x',tools/'libc/crt0.b',work/(name+'.b'),work/'probe.b',tools/'libv7.a','-o',work/name])
(work/'empty').write_bytes(b'')
entries='\n'.join(f'n{i:04d} ---644 0 0 {work}/empty' for i in range(4100))
(work/'proto').write_text(f'''boot
8000 4200
d--755 0 0
bin d--755 0 0
sh ---755 0 0 {tools}/sh
v7fs ---755 0 0 {work}/v7fs
ttioc ---755 0 0 {work}/ttioc
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
large d--755 0 0
{entries}
$
$
''')
run([tools/'v7mkfs',work/'hd.img',work/'proto'])
for name in ['v7fs','ttioc']:
    r=subprocess.run([str(build/'test_driver'),'-c','800000000','-d',str(work/'hd.img'),
        '-i',name+'\\n','-w',name+': passed','-I','exit\\n','-x',name+': passed'],
        cwd=build,capture_output=True,timeout=60)
    (work/(name+'.log')).write_bytes(r.stdout+r.stderr)
    if r.returncode or b': FAIL' in r.stdout:
        sys.stdout.buffer.write(r.stdout+r.stderr);raise SystemExit(name+': failed')
    print(name+': passed',flush=True)
