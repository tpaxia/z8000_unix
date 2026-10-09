#!/usr/bin/env python3
"""Panic polling/saved disk contents and native aggregate-return re-entry."""
from pathlib import Path
import re
import subprocess
import sys
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools/native-cc'))
from build import image, compile_c, run
from selfhost import Filesystem
WORK = ROOT/'tests/build/kernel-gaps'
WORK.mkdir(parents=True, exist_ok=True)
BUILD = Path(sys.argv[1]).resolve() if len(sys.argv)>1 else ROOT/'v7z8000/usr/sys/build'
KERNEL = ROOT/'v7z8000/usr/sys'
LD = ROOT/'tests/build/ldz8-host/ldz8'
RUNTIME = ROOT/'tests/build/sout-cc'
run(['make','-C',ROOT/'tools/sout-utils'])

def executable(name, source):
    obj = WORK/(name+'.b')
    compile_c(source, obj)
    output=WORK/name
    run([LD, '-x', RUNTIME/'crt0.b', obj, RUNTIME/'libc.a', '-o', output])
    return output

# Trigger a real panic in the clock handler after a partial file write, without
# a normal sync/close. The one-shot test MMU fault is outside user-copy fixups.
panic=WORK/'panic.c'
panic.write_text('''#include <stdio.h>
main() {
 int fd,i; char data[128];
 for(i=0;i<128;i++)data[i]='A'+i%26;
 fd=creat("/tmp/survivor",0600);
 if(fd<0 || write(fd,data,128)!=128)return 1;
 puts("panic-test-ready");fflush(stdout);
 for(;;)pause();
}
''')
panexe=executable('panic',panic)
image({'bin/panic':panexe},WORK/'panic.img')
names=run([ROOT/'tests/build/sout-utils-host/nm',BUILD/'handler.sout']).stdout.decode()
timeaddr=int(re.search(r'^(\d+) B _time$',names,re.M)[1],8)
r=subprocess.run([str(BUILD/'test_driver'),'-d',str(WORK/'panic.img'),
    '-c','200000000','-i','panic\\n','-w','panic-test-ready','-I','',
    '-F','k:%x'%timeaddr,'-o',str(WORK/'panic-saved.img')],
    cwd=BUILD,capture_output=True,timeout=60)
out=r.stdout+r.stderr;(WORK/'panic.log').write_bytes(out)
assert b'panic: kernel access fault' in out and b'panic flush:' in out, out[-6000:]
assert b'timeout;' not in out and b'recursive panic' not in out, out[-6000:]
assert re.search(rb'panic flush: \d+ writes, \d+ skipped, 0 errors',out),out[-6000:]
fs=Filesystem(WORK/'panic-saved.img')
assert fs.read('/tmp/survivor')==bytes(ord('A')+i%26 for i in range(128))
print('PASS real kernel panic preserves an unclosed file on the saved disk',flush=True)

# This is the actual kernel implementation, with a bounded synthetic driver.
source=(KERNEL/'sys/panic.c').read_text()
source=source.replace('"../h/', '"'+str(KERNEL/'h')+'/')
probe=WORK/'probe.c'; probe.write_text(source+(ROOT/'tools/panicprobe.c').read_text())
probeexe=executable('probe',probe)
runner=executable('runner',ROOT/'tools/native-cc/runner.c')
sedobjects=[]
for name in ('sed0','sed1'):
    obj=WORK/(name+'.b')
    compile_c(ROOT/'v7z8000/usr/src/cmd/sed'/(name+'.c'),obj)
    sedobjects.append(obj)
run([LD,'-i','-x',RUNTIME/'crt0.b',*sedobjects,RUNTIME/'libc.a','-o',WORK/'sed'])
hook=WORK/'hook.sed'
hook.write_text('/ldir.*@r9,@r8,r0/c\\\n'
    '\tld\tr0,#128\\\n\tldir\t@r9,@r8,r0\\\n'
    '\tpush\t@sp,r8\\\n\tpush\t@sp,r9\\\n'
    '\tcall\t_copyhoo\\\n\tpop\tr9,@sp\\\n\tpop\tr8,@sp\\\n'
    '\tld\tr0,#128\\\n\tldir\t@r9,@r8,r0\n')
plan=WORK/'plan'
plan.write_text('0 - /bin/probe\n'
                '0 - /bin/cc -S /tmp/aggregate.c\n'
                '0 hook.az8 /bin/sed -f /tmp/hook.sed /tmp/aggregate.az8\n'
                '0 - /bin/asz8k -c -o /tmp/hook.b /tmp/hook.az8\n'
                '0 - /bin/cc /tmp/hook.b -o /tmp/aggn\n'
                '0 - /tmp/aggn\n'
                '0 - /bin/cc -i /tmp/hook.b -o /tmp/aggi\n'
                '0 - /tmp/aggi\n'
                '0 - /bin/cc -O -i /tmp/wide.c -o /tmp/wide\n'
                '0 - /tmp/wide\n')
image({'bin/probe':probeexe,'bin/runner':runner,'bin/sed':WORK/'sed',
       'tmp/plan':plan,'tmp/hook.sed':hook,
       'tmp/aggregate.c':ROOT/'tools/native-cc/aggregate-signal.c',
       'tmp/wide.c':ROOT/'PCC-z8000/z8000/test/regress/aggregate_wide.c'},WORK/'native.img')
r=subprocess.run([str(BUILD/'test_driver'),'-d',str(WORK/'native.img'),
    '-c','60000000000','-i','runner\\n','-x','NATIVE CC DONE','-o',str(WORK/'native-saved.img')],
    cwd=BUILD,capture_output=True,timeout=600)
out=r.stdout+r.stderr;(WORK/'native.log').write_bytes(out)
assert r.returncode==0 and b'panicprobe: passed' in out and b'FAILED' not in out, out[-6000:]
assert out.count(b'aggregate: passed') >= 2 and b'NATIVE CC PASS' in out, out[-6000:]
print('PASS panic polling: cache/inodes/superblock, locked objects, errors and timeouts',flush=True)
print('PASS native combined/split aggregate returns under signal re-entry',flush=True)

print('PASS native 5 KiB aggregate returns: direct and indirect, every word checked',flush=True)
