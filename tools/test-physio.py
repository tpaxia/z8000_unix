#!/usr/bin/env python3
"""Raw disk syscalls with real mappings, context switches and swapping."""
from pathlib import Path
import re
import subprocess
import sys
root=Path(__file__).resolve().parents[1]
tools=root/'tools';pcc=root/'PCC-z8000/z8000'
build=Path(sys.argv[1]).resolve();work=root/'tests/build/physio';work.mkdir(parents=True,exist_ok=True)
(work / 'asz8k.pd').write_bytes((tools / 'asz8k/src/asz8k.pd').read_bytes())
def run(args,**kw):
    r=subprocess.run(list(map(str,args)),capture_output=True,**kw)
    if r.returncode:
        sys.stdout.buffer.write(r.stderr);r.check_returncode()
    return r.stdout
run(['make','-C',tools,'libv7.a','libc/crt0.b','sh','init','v7mkfs'])
pre=run(['cpp','-nostdinc','-undef','-Dz8000','-Dz8002','-I'+str(root/'v7z8000/usr/include'),tools/'rawtest.c'])
(work/'raw.az8').write_bytes(run([pcc/'cz8/cz8'],input=pre))
run([tools.parent / 'tests/build/asz8k-host/asz8k', '-c','-o','raw.b','raw.az8'],cwd=work)
for layout in ['n','i']:
    run([tools.parent / 'tests/build/ldz8-host/ldz8','-x',*(['-i'] if layout=='i' else []),tools/'libc/crt0.b',work/'raw.b',tools/'libv7.a','-o',work/('raw'+layout)])
(work/'proto').write_text(f'''boot
1600 96
d--755 0 0
bin d--755 0 0
sh ---755 0 0 {tools}/sh
rawn ---755 0 0 {work}/rawn
rawi ---755 0 0 {work}/rawi
$
dev d--755 0 0
console c--644 0 0 0 0
tty c--644 0 0 2 0
rhd c--600 0 0 3 0
rswap c--600 0 0 3 1
rbad c--600 0 0 3 2
$
etc d--755 0 0
init ---755 0 0 {tools}/init
$
tmp d--777 0 0
$
$
''')
run([tools/'v7mkfs',work/'hd.img',work/'proto'])
with (work/'hd.img').open('r+b') as f: f.truncate(1616*512)
for layout in ['n','i']:
    for ram in [8192,320]:
        label=f'raw-{layout}-{ram}'
        r=subprocess.run(list(map(str,[build/'test_driver','-c','3000000000','-R',ram,'-S',4096,'-d',work/'hd.img',
            '-i','raw'+layout+'\\n','-w','raw: passed','-I','exit\\n','-x','raw: passed'])),cwd=build,capture_output=True,timeout=90)
        output=r.stdout+r.stderr;(work/(label+'.log')).write_bytes(output)
        if r.returncode or b'raw: FAIL' in output:
            sys.stdout.buffer.write(output);raise SystemExit(label+': failed')
        assert b'Absent RAM accesses: 0' in output
        if ram==320:
            counts=re.search(rb'Swap sectors: (\d+) read, (\d+) written',output)
            assert counts and min(map(int,counts.groups()))>0
        print(label+': passed',flush=True)
