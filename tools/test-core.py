#!/usr/bin/env python3
"""V7 core files, register snapshots, permission failures and swapping."""
from pathlib import Path
import re
import subprocess
import sys
root=Path(__file__).resolve().parents[1]
tools=root/'tools';pcc=root/'PCC-z8000/z8000'
build=Path(sys.argv[1]).resolve();work=root/'tests/build/core';work.mkdir(parents=True,exist_ok=True)
(work / 'asz8k.pd').write_bytes((tools / 'asz8k/src/asz8k.pd').read_bytes())
def run(args,**kw):
    r=subprocess.run(list(map(str,args)),capture_output=True,**kw)
    if r.returncode:
        sys.stdout.buffer.write(r.stdout+r.stderr);r.check_returncode()
    return r.stdout
run(['make','-C',tools,'libv7.a','libc/crt0.b','sh','init','v7mkfs'])
pre=run(['cpp','-nostdinc','-undef','-Dz8000','-Dz8002','-I'+str(root/'v7z8000/usr/include'),tools/'coretest.c'])
(work/'core.az8').write_bytes(run([pcc/'cz8/cz8'],input=pre))
run([tools.parent / 'tests/build/asz8k-host/asz8k', '-c','-o','core.b','core.az8'],cwd=work)
(work/'regs.az8').write_bytes((tools/'coreregs.az8').read_bytes())
run([tools.parent / 'tests/build/asz8k-host/asz8k', '-c','-o','regs.b','regs.az8'],cwd=work)
for layout in ['n','i']:
    run([tools.parent / 'tests/build/ldz8-host/ldz8','-x',*(['-i'] if layout=='i' else []),tools/'libc/crt0.b',work/'core.b',work/'regs.b',tools/'libv7.a','-o',work/('core'+layout)])
kernel=root/'v7z8000/usr/sys'
headers='#define access coreaccess\n#define time kernel_time\n'+''.join(f'#include "{kernel}/h/{h}.h"\n' for h in ['param','systm','dir','user','inode'])
headers+='#undef u\nextern struct user u;\n'
policy=(kernel/'sys/sig.c').read_text().split('\ncore()\n',1)[1].split('/*\n * find the signal',1)[0]
(work/'policy.c').write_text(headers+'core()\n'+policy+(tools/'corepolicy.c').read_text())
pre=run(['cpp','-nostdinc','-undef','-Dz8000','-Dz8002','-I'+str(root/'v7z8000/usr/include'),work/'policy.c'])
(work/'policy.az8').write_bytes(run([pcc/'cz8/cz8'],input=pre))
run([tools.parent / 'tests/build/asz8k-host/asz8k', '-c','-o','policy.b','policy.az8'],cwd=work)
run([tools.parent / 'tests/build/ldz8-host/ldz8','-x',tools/'libc/crt0.b',work/'policy.b',tools/'libv7.a','-o',work/'policy'])
(work/'proto').write_text(f'''boot
1600 96
d--755 0 0
bin d--755 0 0
sh ---755 0 0 {tools}/sh
coren ---755 0 0 {work}/coren
corei ---755 0 0 {work}/corei
policy ---755 0 0 {work}/policy
$
dev d--755 0 0
console c--644 0 0 0 0
tty c--644 0 0 2 0
$
etc d--755 0 0
init ---755 0 0 {tools}/init
$
tmp d--777 0 0
directory d--777 0 0
$
$
$
''')
run([tools/'v7mkfs',work/'hd.img',work/'proto'])
for layout in ['n','i']:
    for ram,mode in [(8192,''),(320,''),(8192,' full')]:
        label=f'core-{layout}-{ram}'+mode.replace(' ','-')
        r=subprocess.run(list(map(str,[build/'test_driver','-c','3000000000','-R',ram,'-S',4096,'-d',work/'hd.img',
            '-i','core'+layout+mode+'\\n','-w','core: passed','-I','exit\\n','-x','core: passed'])),cwd=build,capture_output=True,timeout=90)
        output=r.stdout+r.stderr;(work/(label+'.log')).write_bytes(output)
        if r.returncode or b'core: FAIL' in output:
            sys.stdout.buffer.write(output);raise SystemExit(label+': failed')
        assert b'Absent RAM accesses: 0' in output
        if ram==320:
            counts=re.search(rb'Swap sectors: (\d+) read, (\d+) written',output)
            assert counts and min(map(int,counts.groups()))>0
        print(label+': passed',flush=True)

r=subprocess.run(list(map(str,[build/'test_driver','-c','400000000','-d',work/'hd.img',
    '-i','policy\\n','-w','corepolicy: passed','-I','exit\\n','-x','corepolicy: passed'])),cwd=build,capture_output=True,timeout=60)
(work/'policy.log').write_bytes(r.stdout+r.stderr)
if r.returncode or b'corepolicy: FAIL' in r.stdout:
    sys.stdout.buffer.write(r.stdout+r.stderr);raise SystemExit('corepolicy: failed')
print('corepolicy: passed',flush=True)
