#!/usr/bin/env python3
"""Native make archive-symbol lookup, file classification and prof symbols."""
from pathlib import Path
import shutil
import struct
import subprocess
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/native-cc'))
from build import image,compile_c,run
from selfhost import Filesystem
WORK=ROOT/'tests/build/sout-consumers'
SYS=ROOT/'v7z8000/usr/sys/build'


def main():
    WORK.mkdir(parents=True,exist_ok=True)
    files={}
    native=Filesystem(ROOT/'tests/build/native-environment-sout/hd.img')
    # Require the corrected native make, not the previous a.out reader.
    assert native.read('/usr/src/make/files.c')==(ROOT/'v7z8000/usr/src/cmd/make/files.c').read_bytes()
    assert native.read('/usr/src/make/object.b')[:2]==b'\xe7\x07'
    for name in ('make','ar','cp','runner'):
        path=WORK/name;path.write_bytes(native.read('/bin/'+name));files['bin/'+name]=path
    for source in [ROOT/'tools/sout-utils'/n for n in ('object.c','object.h')]+[
            ROOT/'tools/asz8k/src'/n for n in ('soutfmt.c','soutfmt.h')]+[
            ROOT/'v7z8000/usr/src/cmd'/n for n in ('file.c','prof.c')]:
        files['usr/src/test/'+source.name]=source
    for name,text in [('main.c','main() { return answer()!=1; }\n'),
                      ('answer.c','answer() { return 1; }\n'),
                      ('extra.c','extra() { return 3; }\n')]:
        path=WORK/name;path.write_text(text);files['usr/src/test/'+name]=path
    compile_c(WORK/'extra.c',WORK/'extra.b');files['usr/src/test/extra.b']=WORK/'extra.b'
    obsolete=WORK/'old0407';obsolete.write_bytes(struct.pack('>8H',0o407,2,0,0,12,0,0,0)+bytes.fromhex('9e08')+struct.pack('>8sHH',b'entry',0o42,0))
    files['usr/src/test/old0407']=obsolete
    badmake=WORK/'old.mk';badmake.write_text('query: old.a((entry))\n\t/bin/cp main.b query\n')
    files['usr/src/test/old.mk']=badmake
    shutil.copyfile(ROOT/'tools/asz8k/src/asz8k.pd',WORK/'asz8k.pd')
    shutil.copyfile(ROOT/'tools/asz8k/tests/soutseg.8ks',WORK/'seg.8ks')
    run([ROOT/'tests/build/asz8k-host/asz8k','-zs','seg.8ks'],cwd=WORK)
    files['usr/src/test/seg.so']=WORK/'seg.so'
    # The histogram is a V7 resource; keep its original 16-bit pointer layout.
    mon=WORK/'mon.out';mon.write_bytes(struct.pack('>3H',0,1024,0)+struct.pack('>256H',*([1]*256)))
    files['usr/src/test/mon.out']=mon
    recipe=WORK/'makefile';recipe.write_text('''all: target queryseg queryextra
target: main.b liba.a((_answer))
	/bin/cc -i main.b liba.a -o target
queryseg: mix.a((entry))
	/bin/cp main.b queryseg
queryextra: mix.a((_extra))
	/bin/cp main.b queryextra
''');files['usr/src/test/makefile']=recipe
    plan=WORK/'plan';plan.write_text('\n'.join([
        '0 - /bin/cc -O -i -DSOUT file.c -o file',
        '0 - /bin/cc -O -c -DSOUT prof.c object.c soutfmt.c',
        '0 - /bin/cc -i prof.b object.b soutfmt.b -o prof',
        '0 - /bin/cc -O -c main.c answer.c',
        '0 - /bin/ar rc liba.a answer.b','0 - /bin/ar rc mix.a seg.so extra.b',
        '0 - /bin/make all','0 - ./target','0 - /bin/make all',
        '0 types ./file target answer.b seg.so extra.b',
        '0 profile ./prof -l target',
        '0 - /bin/ar rc old.a old0407',
        '1 oldmake /bin/make -f old.mk',
        '0 oldprof ./prof -l old0407',
    ])+'\n');files['tmp/plan']=plan
    image(files,WORK/'hd.img',blocks=10000,inodes=900,sout=True)
    with (WORK/'native.log').open('wb') as log:
        result=subprocess.run([str(SYS/'test_driver'),'-c','200000000000',
            '-d',str(WORK/'hd.img'),'-o',str(WORK/'next.img'),
            '-i','runner /tmp/plan /usr/src/test\\n','-w','NATIVE CC DONE',
            '-I','exit\\n','-x','NATIVE CC DONE'],cwd=SYS,
            stdout=log,stderr=subprocess.STDOUT,timeout=900)
    log=(WORK/'native.log').read_bytes()
    assert result.returncode==0 and b'NATIVE CC PASS\r\n' in log,WORK/'native.log'
    # MMU stack warnings are successful user stores that request pre-growth;
    # prof's formatted output can trigger one. A failed growth/exec still
    # fails the runner. They are not kernel-stack corruption diagnostics.
    assert b'Absent RAM accesses: 0' in log
    fs=Filesystem(WORK/'next.img')
    types=fs.read('/usr/src/test/types')
    assert b'target:\tseparate executable not stripped\n' in types,types
    assert b'answer.b:\tobject not stripped\n' in types,types
    assert b'seg.so:\tsegmented object not stripped\n' in types,types
    assert b'extra.b:\tobject not stripped\n' in types,types
    assert b'old0407: bad format' in log
    assert fs.read('/usr/src/test/oldprof')==b''
    profile=fs.read('/usr/src/test/profile')
    assert b'main' in profile,profile
    assert fs.read('/usr/src/test/queryseg')==fs.read('/usr/src/test/main.b')
    assert fs.read('/usr/src/test/queryextra')==fs.read('/usr/src/test/main.b')
    print('PASS native make symbol dependencies across NONSEG/SEG s.out, file/prof and obsolete-format rejection')


if __name__=='__main__':main()
