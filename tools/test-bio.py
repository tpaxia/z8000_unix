#!/usr/bin/env python3
"""Test actual V7 cache/HD queue code and sync/reboot persistence."""
from pathlib import Path
import subprocess
import sys
root=Path(__file__).resolve().parents[1]
tools=root/'tools';pcc=root/'PCC-z8000/z8000';kernel=root/'v7z8000/usr/sys'
build=Path(sys.argv[1]).resolve();work=root/'tests/build/bio';work.mkdir(parents=True,exist_ok=True)
(work / 'asz8k.pd').write_bytes((tools / 'asz8k/src/asz8k.pd').read_bytes())
def run(args,**kw):
    r=subprocess.run(list(map(str,args)),capture_output=True,**kw)
    if r.returncode: sys.stdout.buffer.write(r.stdout+r.stderr);r.check_returncode()
    return r.stdout
run(['make','-C',tools,'libv7.a','libc/crt0.b','sh','init','v7mkfs'])
headers='#define time kernel_time\n'+''.join(f'#include "{kernel}/h/{h}.h"\n' for h in ['param','systm','dir','user','buf','conf','proc','seg'])
headers+='#undef time\n#undef u\nextern struct user u;\nextern char buffers[NBUF][BSIZE];\n'
def body(p): return '\n'.join(l for l in p.read_text().splitlines() if not l.startswith('#include'))+'\n'
main=(kernel/'sys/main.c').read_text()
binit=main[main.index('binit()\n{'):main.index('struct buf buf[NBUF];')]
paged=(kernel/'machine/paged.c').read_text()
mapping=paged[paged.index('/* Physical frames'):paged.index('static struct memspace memory[NPROC];')+len('static struct memspace memory[NPROC];')]
mapping+=paged[paged.index('/* Validate every covered'):paged.index('/* Grow from the actual')]
mapping+=paged[paged.index('/* Opaque B_PHYS descriptor:'):paged.index('/* V7 core layout:')]
(work/'cache.c').write_text(headers+mapping+body(kernel/'sys/bio.c')+body(kernel/'sys/physio.c')+body(kernel/'dev/hd.c')+binit+(tools/'biotest.c').read_text())
for name,src in [('cache',work/'cache.c'),('persist',tools/'biopersist.c')]:
    pre=run(['cpp','-nostdinc','-undef','-Dz8000','-Dz8002','-I'+str(root/'v7z8000/usr/include'),src])
    (work/(name+'.az8')).write_bytes(run([pcc/'cz8/cz8'],input=pre))
    run([tools.parent / 'tests/build/asz8k-host/asz8k', '-c','-o',name+'.b',name+'.az8'],cwd=work)
    run([tools.parent / 'tests/build/ldz8-host/ldz8','-x',tools/'libc/crt0.b',work/(name+'.b'),tools/'libv7.a','-o',work/name])
(work/'proto').write_text(f'''boot
1600 96
d--755 0 0
bin d--755 0 0
sh ---755 0 0 {tools}/sh
cache ---755 0 0 {work}/cache
persist ---755 0 0 {work}/persist
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
for name,command,expected,disk,extra in [
    ('cache','cache','bio: passed',work/'hd.img',[]),
    ('write','persist write','persist: written',work/'hd.img',['-o',work/'saved.img']),
    ('reboot','persist read','persist: verified',work/'saved.img',[])]:
    r=subprocess.run(list(map(str,[build/'test_driver','-c','600000000','-d',disk,
        '-i',command+'\\n','-w',expected,'-I','exit\\n','-x',expected,*extra])),
        cwd=build,capture_output=True,timeout=60)
    (work/(name+'.log')).write_bytes(r.stdout+r.stderr)
    if r.returncode or b'bio: FAIL' in r.stdout:
        sys.stdout.buffer.write(r.stdout+r.stderr);raise SystemExit(name+': failed')
    print(name+': passed',flush=True)
