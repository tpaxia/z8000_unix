#!/usr/bin/env python3
"""Native mkfs prototype boot images use s.out; old headers are rejected."""
from pathlib import Path
import struct
import subprocess
import sys
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/native-cc'))
from build import image, run
from selfhost import Filesystem
WORK = ROOT/'tests/build/mkfs-sout'
WORK.mkdir(parents=True, exist_ok=True)
AS = ROOT/'tests/build/asz8k-host/asz8k'
LD = ROOT/'tests/build/ldz8-host/ldz8'
(WORK/'tiny.az8').write_text('.text\n.globl entry\nentry:\nhalt\n')
run([AS,'-c','-o','tiny.so','tiny.az8'],cwd=WORK)
run([LD,'-s','tiny.so','-o','tiny'],cwd=WORK)
files = {'usr/src/test/mkfs.c':ROOT/'v7z8000/usr/src/cmd/mkfs.c',
         'usr/src/test/tiny':WORK/'tiny'}
fs = Filesystem(ROOT/'tests/build/userland-native-sout/hd.img')
for name in ('runner','sh'):
    p=WORK/name;p.write_bytes(fs.read('/bin/'+name));files['bin/'+name]=p
for magic in (0o405,0o407,0o410,0o411):
    name='old%o'%magic
    p=WORK/name;p.write_bytes(struct.pack('>8H',magic,2,0,0,0,0,0,0)+bytes.fromhex('9e08'))
    files['usr/src/test/'+name]=p
for name in ['tiny','old405','old407','old410','old411']:
    p=WORK/(name+'.proto');p.write_text('/usr/src/test/'+name+'\n128 16\nd--777 0 0\n$\n')
    files['usr/src/test/'+name+'.proto']=p
commands=['0 - /bin/cc -O -i mkfs.c -o mkfs']
commands += ['0 '+name+'.log ./mkfs '+name+'.fs '+name+'.proto' for name in ['tiny','old405','old407','old410','old411']]
p=WORK/'plan';p.write_text('\n'.join(commands)+'\n');files['tmp/plan']=p
image(files,WORK/'hd.img',blocks=10000,inodes=900)
SYS=ROOT/'v7z8000/usr/sys/build'
with (WORK/'native.log').open('wb') as log:
    result=subprocess.run([str(SYS/'test_driver'),'-c','200000000000',
        '-d',str(WORK/'hd.img'),'-o',str(WORK/'next.img'),
        '-i','runner /tmp/plan /usr/src/test\\n','-w','NATIVE CC DONE',
        '-I','exit\\n','-x','NATIVE CC DONE'],cwd=SYS,stdout=log,stderr=subprocess.STDOUT,timeout=900)
assert result.returncode==0 and b'NATIVE CC PASS\r\n' in (WORK/'native.log').read_bytes()
fs=Filesystem(WORK/'next.img')
assert fs.read('/usr/src/test/tiny.fs')[:256]==(WORK/'tiny').read_bytes()[40:296]
assert b'bad format' not in fs.read('/usr/src/test/tiny.log')
for magic in (0o405,0o407,0o410,0o411):
    assert b'bad format' in fs.read('/usr/src/test/old%o.log'%magic)
    assert fs.read('/usr/src/test/old%o.fs'%magic)[:512]==bytes(512)
print('PASS native mkfs: s.out boot payload and four obsolete-header rejections')
