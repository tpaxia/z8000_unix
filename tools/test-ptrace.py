#!/usr/bin/env python3
"""V7 ptrace stop/read/write/continue/exit on the target ABI."""
from pathlib import Path
import re
import struct
import subprocess
import sys
root=Path(__file__).resolve().parents[1]
tools=root/'tools';pcc=root/'PCC-z8000/z8000'
build=Path(sys.argv[1]).resolve();work=root/'tests/build/trace';work.mkdir(parents=True,exist_ok=True)
def run(args,**kw):
    r=subprocess.run(list(map(str,args)),capture_output=True,**kw)
    if r.returncode:
        sys.stdout.buffer.write(r.stdout+r.stderr);r.check_returncode()
    return r.stdout
run(['make','-C',tools,'libv7.a','libc/crt0.b','sh','init','v7mkfs'])
(work/'regs.az8').write_bytes((tools/'traceregs.az8').read_bytes())
run([pcc/'az8/az8','-o','regs.b','regs.az8'],cwd=work)
(work/'target.c').write_text('int token= -1; main(argc) { return(argc>1 ? token!=-1 || traceval()!=7 : token!=0x1234 || traceval()!=9); }\n')
pre=run(['cpp','-nostdinc','-undef',work/'target.c'])
(work/'target.az8').write_bytes(run([pcc/'cz8/cz8'],input=pre))
run([pcc/'az8/az8','-o','target.b','target.az8'],cwd=work)
def link(name,obj,layout):
    run([pcc/'ldz8','-x',*(['-i'] if layout=='i' else []),tools/'libc/crt0.b',obj,work/'regs.b',tools/'libv7.a','-o',work/name])
addresses=[]
for layout in ['n','i']:
    link('target'+layout,work/'target.b',layout)
    binary=(work/('target'+layout)).read_bytes(); h=struct.unpack('>8H',binary[:16])
    offset=16+h[1]+h[2]+h[6]+h[7]
    symbols={name.rstrip(b'\0'):value for name,kind,value in struct.iter_unpack('>8sHH',binary[offset:offset+h[4]])}
    addresses.append((symbols[b'_token'],symbols[b'_traceva']+2))
(work/'traceaddr.h').write_text('int targetdata[2]={%d,%d}, targetword[2]={%d,%d};\n' % (addresses[0][0],addresses[1][0],addresses[0][1],addresses[1][1]))
pre=run(['cpp','-nostdinc','-undef','-Dz8000','-Dz8002','-I'+str(work),'-I'+str(root/'v7z8000/usr/include'),tools/'tracetest.c'])
(work/'trace.az8').write_bytes(run([pcc/'cz8/cz8'],input=pre))
run([pcc/'az8/az8','-o','trace.b','trace.az8'],cwd=work)
for layout in ['n','i']: link('trace'+layout,work/'trace.b',layout)
(work/'proto').write_text(f'''boot
1600 96
d--755 0 0
bin d--755 0 0
sh ---755 0 0 {tools}/sh
tracen ---755 0 0 {work}/tracen
tracei ---755 0 0 {work}/tracei
targetn ---755 0 0 {work}/targetn
targeti ---755 0 0 {work}/targeti
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
    for ram,mode in [(8192,''),(320,'')]:
        label=f'trace-{layout}-{ram}'+mode.replace(' ','-')
        r=subprocess.run(list(map(str,[build/'test_driver','-c','3000000000','-R',ram,'-S',4096,'-d',work/'hd.img',
            '-i','trace'+layout+mode+'\\n','-w','trace: passed','-I','exit\\n','-x','trace: passed'])),cwd=build,capture_output=True,timeout=90)
        output=r.stdout+r.stderr;(work/(label+'.log')).write_bytes(output)
        if r.returncode or b'trace: FAIL' in output:
            sys.stdout.buffer.write(output);raise SystemExit(label+': failed')
        assert b'Absent RAM accesses: 0' in output
        if ram==320:
            counts=re.search(rb'Swap sectors: (\d+) read, (\d+) written',output)
            assert counts and min(map(int,counts.groups()))>0
        print(label+': passed',flush=True)

