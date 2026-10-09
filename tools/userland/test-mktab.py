#!/usr/bin/env python3
"""Native terminal-resource conversion with the unchanged V7 nroff reader."""
from pathlib import Path
import struct
import shutil
import subprocess
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/native-cc'))
from build import image,compile_c,run
from selfhost import Filesystem
WORK=ROOT/'tests/build/sout-terminal'
SYS=ROOT/'v7z8000/usr/sys/build'


def main():
    WORK.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(ROOT/'tools/asz8k/src/asz8k.pd',WORK/'asz8k.pd')
    source=ROOT/'v7z8000/usr/src/cmd/troff/term/tab300.c'
    compile_c(source,WORK/'tab300.b')
    run([ROOT/'tests/build/ldz8-host/ldz8','-z','-r','tab300.b','-o','table.so'],cwd=WORK)
    host=ROOT/'tests/build/sout-utils-host/mktab'
    run(['cc','-std=gnu89','-O2','-w','-I'+str(ROOT/'tools/sout-utils'),
         '-I'+str(ROOT/'tools/asz8k/src'),ROOT/'tools/userland/mktab.c',
         ROOT/'tools/sout-utils/object.c',ROOT/'tools/asz8k/src/soutfmt.c','-o',host])
    run([host,'table.so','wanttab'],cwd=WORK)
    data=(WORK/'wanttab').read_bytes()
    assert struct.unpack_from('>H',data)[0]==0o411
    assert len(data)==16+struct.unpack_from('>H',data,4)[0]
    files={}
    for path in [ROOT/'tools/sout-utils'/n for n in ('object.c','object.h')]+[
            ROOT/'tools/asz8k/src'/n for n in ('soutfmt.c','soutfmt.h')]+[ROOT/'tools/userland/mktab.c']:
        files['usr/src/test/'+path.name]=path
    for name in ('table.so','wanttab'):files['usr/src/test/'+name]=WORK/name
    files['usr/lib/term/tab300']=WORK/'wanttab'
    old=Filesystem(ROOT/'tests/build/userland-native-sout/hd.img')
    for name in ('runner','normal','nroff'):
        path=WORK/name;path.write_bytes(old.read('/bin/'+name));files['bin/'+name]=path
    files['bin/cp']=ROOT/'tests/build/native-environment-sout/native/bin/cp'
    files['bin/cmp']=ROOT/'tests/build/native-environment-sout/native/bin/cmp'
    for path in (ROOT/'v7z8000/usr/lib/tmac').rglob('*'):
        if path.is_file():files['usr/lib/tmac/'+str(path.relative_to(ROOT/'v7z8000/usr/lib/tmac'))]=path
    text=WORK/'text';text.write_text('Terminal resource test\n');files['tmp/text']=text
    bad=WORK/'bad.so';bad.write_bytes((WORK/'table.so').read_bytes()[:-1]);files['usr/src/test/bad.so']=bad
    plan=WORK/'plan';plan.write_text('\n'.join([
        '0 - /bin/cc -O -i mktab.c object.c soutfmt.c -o mktab',
        '0 - ./mktab table.so tab300','0 - /bin/cmp tab300 wanttab',
        '0 - /bin/cp tab300 /usr/lib/term/tab300',
        '0 output /bin/normal /bin/nroff -T300 /tmp/text',
        '1 - ./mktab bad.so badtab','1 - ./mktab table.so table.so',
    ])+'\n');files['tmp/plan']=plan
    image(files,WORK/'hd.img',blocks=8000,inodes=700,sout=True)
    with (WORK/'native.log').open('wb') as log:
        result=subprocess.run([str(SYS/'test_driver'),'-c','100000000000',
            '-d',str(WORK/'hd.img'),'-o',str(WORK/'next.img'),
            '-i','runner /tmp/plan /usr/src/test\\n','-w','NATIVE CC DONE',
            '-I','exit\\n','-x','NATIVE CC DONE'],cwd=SYS,
            stdout=log,stderr=subprocess.STDOUT,timeout=600)
    log=(WORK/'native.log').read_bytes()
    assert result.returncode==0 and b'NATIVE CC PASS\r\n' in log,WORK/'native.log'
    assert b'Absent RAM accesses: 0' in log and b'stack warnings: 0' in log
    native=Filesystem(WORK/'next.img')
    assert native.read('/usr/src/test/tab300')==data
    assert native.read('/usr/src/test/table.so')==(WORK/'table.so').read_bytes()
    assert b'Terminal resource test' in native.read('/usr/src/test/output')
    print('PASS identical host/native terminal resource, unchanged nroff and rejected malformed input')


if __name__=='__main__':main()
