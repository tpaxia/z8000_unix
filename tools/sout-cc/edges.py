#!/usr/bin/env python3
"""Exercise compiler controls and SEG assembly with the self-rebuilt tools."""
from pathlib import Path
import shutil
import struct
import subprocess
import sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[2]
WORK=ROOT/'tests/build/sout-cc'
SYS=ROOT/'v7z8000/usr/sys/build'
sys.path.insert(0,str(ROOT/'tools/native-cc'))
from build import image,run
from selfhost import Filesystem


def main():
    fs=Filesystem(WORK/'hd.img')
    files={}
    edge=WORK/'edges'; edge.mkdir(exist_ok=True)
    for target in ('bin/cc','bin/asz8k','bin/ldz8','bin/runner','bin/sh',
                   'lib/crt0.b','lib/libc.a','lib/front','lib/back','lib/oz8'):
        path=edge/target; path.parent.mkdir(parents=True,exist_ok=True)
        path.write_bytes(fs.read('/'+target)); files[target]=path
    files['usr/lib/asz8k.pd']=ROOT/'tools/asz8k/src/asz8k.pd'
    files['usr/src/test/asz8k.pd']=files['usr/lib/asz8k.pd']
    files['usr/src/test/hello.c']=ROOT/'tools/native-cc/hello.c'
    for name,source in {'branches.az8':ROOT/'tools/sout-cc/branches.az8',
                        'seg.8ks':ROOT/'tools/asz8k/tests/soutseg.8ks',
                        'ext.8ks':ROOT/'tools/ldz8/tests/ext.8ks'}.items():
        files['usr/src/test/'+name]=source
        shutil.copyfile(source,edge/name)
    shutil.copyfile(files['usr/lib/asz8k.pd'],edge/'asz8k.pd')
    assembler=ROOT/'tests/build/asz8k-host/asz8k'
    linker=ROOT/'tests/build/ldz8-host/ldz8'
    run([assembler,'-zc','branches.az8'],cwd=edge)
    branch=(edge/'branches.so').read_bytes()
    # CPU manual JR word bounds: +254 and -256 stay short. The branches
    # outside those bounds become JP, using the assembler's existing encoder.
    assert struct.unpack_from('>H',branch,40)[0]==0xe87f
    assert struct.unpack_from('>2H',branch,40+256)==(0x5e08,516)
    assert struct.unpack_from('>H',branch,40+776)[0]==0xe880
    assert struct.unpack_from('>2H',branch,40+1034)==(0x5e08,778)
    for name in ('seg.8ks','ext.8ks'): run([assembler,'-zs',name],cwd=edge)
    run([linker,'-z','-C','3','-D','5','-e','entry','seg.so','ext.so','-o','segexec'],cwd=edge)
    plan=edge/'plan'
    plan.write_text('\n'.join([
        '0 - /bin/cc -z -i -R 0 hello.c -o hello','0 - ./hello',
        '1 - /bin/cc -i -R 1 hello.c -o bad',
        '1 - /bin/cc -p hello.c','1 - /bin/cc -f hello.c',
        '0 pre.i /bin/cc -E hello.c',
        '0 - /bin/asz8k -zc branches.az8',
        '0 - /bin/cc -i branches.az8 -o branches','0 - ./branches',
        '0 - /bin/asz8k -zs seg.8ks','0 - /bin/asz8k -zs ext.8ks',
        '0 - /bin/ldz8 -z -C 3 -D 5 -e entry seg.so ext.so -o segexec',
    ])+'\n')
    files['tmp/plan']=plan
    image(files,edge/'hd.img',blocks=6000,inodes=512)
    logpath=edge/'native.log'
    with logpath.open('wb') as log:
        result=subprocess.run([str(SYS/'test_driver'),'-c','200000000000',
            '-d',str(edge/'hd.img'),'-o',str(edge/'next.img'),
            '-i','runner /tmp/plan /usr/src/test\\n','-w','NATIVE CC DONE',
            '-I','exit\\n','-x','NATIVE CC DONE'],cwd=SYS,
            stdout=log,stderr=subprocess.STDOUT,timeout=900)
    data=logpath.read_bytes()
    assert result.returncode==0 and b'NATIVE CC PASS\r\n' in data,logpath
    native=Filesystem(edge/'next.img')
    for name in ('branches.so','seg.so','ext.so','segexec'):
        assert native.read('/usr/src/test/'+name)==(edge/name).read_bytes(),name
    assert b'Hello from native C' in native.read('/usr/src/test/pre.i')
    assert b'Absent RAM accesses: 0' in data
    print('PASS self-rebuilt cc controls, JR boundary relaxation and identical SEG objects/links')


if __name__=='__main__': main()
