#!/usr/bin/env python3
"""Exercise address-wrap rejection and real SEGTRAP helper recovery."""
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
tools = root / 'tools'
pcc = root / 'PCC-z8000/z8000'
build = Path(sys.argv[1]).resolve()
work = root / 'tests/build/fault'
work.mkdir(parents=True, exist_ok=True)
(work / 'asz8k.pd').write_bytes((tools / 'asz8k/src/asz8k.pd').read_bytes())

def run(args, **kw):
    r = subprocess.run(list(map(str, args)), capture_output=True, **kw)
    if r.returncode:
        sys.stdout.buffer.write(r.stdout + r.stderr)
        r.check_returncode()
    return r.stdout

run(['make','-C',tools,'libv7.a','libc/crt0.b','sh','init','v7mkfs'])
pre = run(['cpp','-nostdinc','-undef','-I'+str(root/'v7z8000/usr/include'),tools/'faulttest.c'])
(work/'fault.az8').write_bytes(run([pcc/'cz8/cz8'],input=pre))
(work/'regs.az8').write_bytes((tools/'faultregs.az8').read_bytes())
(work/'pad.az8').write_text('.text\n.zerow 20000\n.bss\n.comm _execpad,40000\n')
for name in ['fault','regs','pad']:
    run([tools.parent / 'tests/build/asz8k-host/asz8k', '-c','-o',name+'.b',name+'.az8'],cwd=work)
for name, flags, extra in [('faultn',[],[]),('faulti',['-i'],[]),('faultbig',['-i'],[work/'pad.b'])]:
    run([tools.parent / 'tests/build/ldz8-host/ldz8',*flags,'-x',tools/'libc/crt0.b',work/'fault.b',work/'regs.b',
         *extra,tools/'libv7.a','-o',work/name])
files='\n'.join(f'{n} ---755 0 0 {work/n}' for n in ['faultn','faulti','faultbig'])
(work/'proto').write_text(f'''boot
1200 64
d--755 0 0
bin d--755 0 0
sh ---755 0 0 {tools}/sh
{files}
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
run([tools/'v7mkfs',work/'hd.img',work/'proto'])
z8002=(build/'kernel-selection.txt').read_text().splitlines()[0]=='z8002-mmu'
for name in ['faultn','faulti']:
    for mode, kind in [('bounds',None),('write','r'),('writebyte','r'),('read','w'),
                       ('readbyte','w'),('writepart','r'),('writebytepart','r'),
                       ('readpart','w'),('readbytepart','w'),('path','r'),('argv','r'),('epu','r'),
                       ('signal','w'),('exec','w'),('user','u')]:
        args=[build/'test_driver','-c',('1800000000' if z8002 else ('1500000000' if mode=='exec' else '400000000')),'-d',work/'hd.img',
              '-i',name+' '+mode+'\\n','-x','fault: passed']
        if kind:
            args+=['-w','fault: ready','-I','go\\nexit\\n','-F',kind+(':'+('f000' if mode=='signal' else '9000'))]
        else:
            args+=['-w','fault: passed','-I','exit\\n']
        r=subprocess.run(list(map(str,args)),cwd=build,capture_output=True,timeout=60)
        (work/(name+'-'+mode+'.log')).write_bytes(r.stdout+r.stderr)
        if r.returncode or b'fault: FAIL' in r.stdout or (kind and b'MMU denied accesses: 0' in r.stdout):
            sys.stdout.buffer.write(r.stdout+r.stderr)
            raise SystemExit(name+' '+mode+': failed')
        print(name+' '+mode+': passed',flush=True)
