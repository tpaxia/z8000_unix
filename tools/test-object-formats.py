#!/usr/bin/env python3
"""Accept both NONSEG s.out layouts and reject complete obsolete executables."""
from pathlib import Path
import json
import struct
import subprocess
import sys
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/native-cc'))
from build import image, compile_c, run
WORK = ROOT/'tests/build/format-reject'
SYS = Path(sys.argv[1]).resolve() if len(sys.argv)>1 else ROOT/'v7z8000/usr/sys/build'
LD = ROOT/'tests/build/ldz8-host/ldz8'
RUNTIME = ROOT/'tools'
WORK.mkdir(parents=True,exist_ok=True)
run(['make','-C',RUNTIME,'libv7.a','libc/crt0.b','v7mkfs'])
source = WORK/'probe.c'; source.write_text('main() { return 42; }\n')
compile_c(source,WORK/'probe.b')
compile_c(ROOT/'tools/format-reject.c',WORK/'check.b')
run([LD,'-i','-s',RUNTIME/'libc/crt0.b',WORK/'check.b',RUNTIME/'libv7.a','-o',WORK/'check'])
files = {'bin/runner':ROOT/'tests/build/native-cc-sout/runner', 'bin/check':WORK/'check'}
modes = {}
for layout,flags in [('combined',[]),('split',['-i'])]:
    target = WORK/layout
    run([LD,*flags,'-s',RUNTIME/'libc/crt0.b',WORK/'probe.b',RUNTIME/'libv7.a','-o',target])
    raw = target.read_bytes(); t,d,b = struct.unpack_from('>3H',raw,28)
    files['tmp/'+layout]=target; modes['tmp/'+layout]=0o755
    for magic in (0o407,0o410,0o411,0o405):
        name = ('old' if layout=='combined' else 'sid')+'%04o'%magic
        fixture = WORK/name
        fixture.write_bytes(struct.pack('>8H',magic,t,d,b,0,0,0,0)+raw[40:40+t+d])
        files['tmp/'+name]=fixture; modes['tmp/'+name]=0o755
plan = WORK/'plan'; plan.write_text('0 - /bin/check\n'); files['tmp/plan']=plan
image(files,WORK/'hd.img',blocks=10000,inodes=600,modes=modes)
with (WORK/'native.log').open('wb') as log:
    result=subprocess.run(list(map(str,[SYS/'test_driver','-c','3000000000','-d',WORK/'hd.img',
        '-i','runner /tmp/plan /tmp\\n','-w','NATIVE CC DONE','-I','exit\\n','-x','NATIVE CC DONE'])),
        cwd=SYS,stdout=log,stderr=subprocess.STDOUT,timeout=90)
data=(WORK/'native.log').read_bytes()
assert result.returncode==0 and b'NATIVE CC PASS\r\n' in data,WORK/'native.log'
assert b'Absent RAM accesses: 0' in data and b'Unmapped accesses: 0' in data,WORK/'native.log'
report={'accepted_layouts':2,'obsolete_exec_rejections':8,'errno':'ENOEXEC'}
(WORK/'results.json').write_text(json.dumps(report,indent=2)+'\n')
print('PASS',report)
