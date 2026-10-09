#!/usr/bin/env python3
"""Build V7 adb/savecore inside Unix and test native tracing and crash recovery."""
from pathlib import Path
import argparse, subprocess, sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/native-cc'))
from build import image, compile_c, run
from selfhost import Filesystem
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('build',type=Path,nargs='?',default=ROOT/'v7z8000/usr/sys/build')
p.add_argument('--from-stage',type=int,default=1)
p.add_argument('--rebuild',nargs='+',choices='access command expr findfn format input opset main message output pcs print runpcs setup sym object soutfmt savecore'.split())
a=p.parse_args();BUILD=a.build.resolve()
WORK=ROOT/'tests/build/adb';WORK.mkdir(parents=True,exist_ok=True)
RT=ROOT/'tests/build/sout-cc';LD=ROOT/'tests/build/ldz8-host/ldz8'
NAMES='access command expr findfn format input opset main message output pcs print runpcs setup sym object soutfmt savecore'.split()
files={'unix':BUILD/'handler.sout'}
for path in (ROOT/'v7z8000/usr/src/cmd/adb').glob('*'):
 if path.is_file():files['usr/src/adb/'+path.name]=path
for path in (ROOT/'tools/sout-utils').glob('object.*'):files['usr/src/objutils/'+path.name]=path
for name in ('soutfmt.c','soutfmt.h'):files['usr/src/objutils/'+name]=ROOT/'tools/asz8k/src'/name
files['usr/src/savecore.c']=ROOT/'tools/savecore.c'
for name in ('runner','normal'):
 compile_c(ROOT/'tools/native-cc'/(name+'.c'),WORK/(name+'.b'))
 run([LD,'-x',RT/'crt0.b',WORK/(name+'.b'),RT/'libc.a','-o',WORK/name])
 files['bin/'+name]=WORK/name
empty=WORK/'keep';empty.write_bytes(b'');files['usr/sys/.keep']=empty
commands=[]
for name in NAMES:
 src='/usr/src/objutils/'+name+'.c' if name in ('object','soutfmt') else (
     '/usr/src/savecore.c' if name=='savecore' else '/usr/src/adb/'+name+'.c')
 commands.append('0 - /bin/cc -O -Dz8000 -Dz8002 -I/usr/src/adb -I/usr/src/objutils -c '+src)
commands.append('0 - /bin/cc -i -s '+' '.join('/tmp/'+n+'.b' for n in NAMES[:-1])+' -o /bin/adb')
commands.append('0 - /bin/cc -i -s /tmp/savecore.b -o /bin/savecore')
if not 1<=a.from_stage<=len(commands)+1:p.error('invalid stage')
if a.from_stage>1 or a.rebuild:
 fs=Filesystem(WORK/'disk.img')
 for name in NAMES:
  try:data=fs.read('/tmp/'+name+'.b')
  except Exception:continue
  path=WORK/(name+'.b');path.write_bytes(data);files['tmp/'+name+'.b']=path
 for name in ('adb','savecore'):
  try:data=fs.read('/bin/'+name)
  except Exception:continue
  path=WORK/name;path.write_bytes(data);files['bin/'+name]=path
for i,command in enumerate(commands):
 plan=WORK/('p%03d'%i);plan.write_text(command+'\n');files['tmp/'+plan.name]=plan
image(files,WORK/'disk.img',blocks=24000)
def trial(name,disk,text,expect,extra=(),cycles=60000000000,status=0):
 with (WORK/(name+'.log')).open('wb') as log:
  result=subprocess.run([str(BUILD/'test_driver'),'-d',str(disk),'-c',str(cycles),
   '-i',text,'-x',expect,*map(str,extra)],cwd=BUILD,stdout=log,stderr=subprocess.STDOUT,timeout=900)
 output=(WORK/(name+'.log')).read_bytes()
 assert result.returncode==status and expect.encode() in output,output[-5000:]
 return output
for i,command in list(enumerate(commands))[a.from_stage-1:]:
 if a.rebuild and i<len(NAMES) and NAMES[i] not in a.rebuild:continue
 print('START native adb stage',i+1,NAMES[i] if i<len(NAMES) else 'link',flush=True)
 output=trial('stage%d'%i,WORK/'disk.img','runner /tmp/p%03d /tmp\\n'%i,'NATIVE CC DONE',['-o',WORK/'next.img'])
 assert b'NATIVE CC PASS' in output and b'FAILED command' not in output
 (WORK/'next.img').replace(WORK/'disk.img')
fs=Filesystem(WORK/'disk.img')
for name in ('adb','savecore'):(WORK/name).write_bytes(fs.read('/bin/'+name))
print('PASS adb and savecore compiled/linked inside Unix',flush=True)
