#!/usr/bin/env python3
"""Build and run V7 kernel inspection tools inside Unix, then inspect output."""
from pathlib import Path
import subprocess, sys, argparse
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/native-cc'))
from build import image,compile_c,run
from selfhost import Filesystem
WORK=ROOT/'tests/build/inspection';WORK.mkdir(parents=True,exist_ok=True)
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('build',type=Path,nargs='?',default=ROOT/'v7z8000/usr/sys/build')
parser.add_argument('--from-stage',type=int,default=1,help='restage sources, retaining tools completed before this stage')
args=parser.parse_args()
BUILD=args.build.resolve()
if not 1<=args.from_stage<=6:parser.error('stage must be 1..6')
RT=ROOT/'tests/build/sout-cc'
compile_c(ROOT/'tools/native-cc/runner.c',WORK/'runner.b')
compile_c(ROOT/'tools/native-cc/normal.c',WORK/'normal.b')
run([ROOT/'tests/build/ldz8-host/ldz8','-x',RT/'crt0.b',WORK/'normal.b',RT/'libc.a','-o',WORK/'normal'])
run([ROOT/'tests/build/ldz8-host/ldz8','-x',RT/'crt0.b',WORK/'runner.b',RT/'libc.a','-o',WORK/'runner'])
files={'bin/normal':WORK/'normal','bin/runner':WORK/'runner','unix':BUILD/'handler.sout',
       'usr/src/probe.c':ROOT/'tools/inspection-probe.c',
       'usr/src/pack.c':ROOT/'tools/native-system/pack.c',
       'tmp/vectors':BUILD/'kernel.bin'}
commands=[]
for name in ('ps','pstat','dmesg','iostat'):
 files['usr/src/'+name+'.c']=ROOT/'v7z8000/usr/src/cmd'/(name+'.c')
 commands.append('0 - /bin/cc -O -i /usr/src/'+name+'.c -o /bin/'+name)
commands += ['0 - /bin/cc -O -i /usr/src/probe.c -o /bin/probe',
             '0 - /bin/probe','0 uarea.txt /bin/probe u',
             '0 ps.txt /bin/ps axl','0 pstat.txt /bin/normal /bin/pstat -aipxtf',
             '0 dmesg.txt /bin/dmesg','0 iostat.txt /bin/normal /bin/iostat -t',
             '0 bio.txt /bin/normal /bin/iostat -b','0 states.txt /bin/normal /bin/iostat -i',
             '0 - /bin/cc -O -i /usr/src/pack.c -o /bin/pack',
             '0 - /bin/pack kernel /unix /tmp/vectors /tmp/bootunix']
stages=[[c] for c in commands[:5]]+[commands[5:]]
for i,stage in enumerate(stages):
 plan=WORK/('p%03d'%i);plan.write_text('\n'.join(stage)+'\n')
 files['tmp/'+plan.name]=plan
if args.from_stage>1:
 oldfs=Filesystem(WORK/'disk.img')
 for name in ('ps','pstat','dmesg','iostat','probe')[:args.from_stage-1]:
  cached=WORK/('cached-'+name);cached.write_bytes(oldfs.read('/bin/'+name));files['bin/'+name]=cached
image(files,WORK/'disk.img')
for i,stage in list(enumerate(stages))[args.from_stage-1:]:
 print('START inspection stage',i+1,flush=True)
 with (WORK/('stage%d.log'%i)).open('wb') as log:
  r=subprocess.run([str(BUILD/'test_driver'),'-d',str(WORK/'disk.img'),'-c','60000000000',
      '-i','runner /tmp/p%03d /tmp\\n'%i,'-x','NATIVE CC DONE','-o',str(WORK/'next.img')],
      cwd=BUILD,stdout=log,stderr=subprocess.STDOUT,timeout=600)
 output=(WORK/('stage%d.log'%i)).read_bytes()
 if r.returncode or b'NATIVE CC PASS' not in output or b'FAILED command' in output:
  raise SystemExit('Inspection trial failed: '+str(WORK/('stage%d.log'%i)))
 (WORK/'next.img').replace(WORK/'disk.img')
(WORK/'saved.img').write_bytes((WORK/'disk.img').read_bytes())
fs=Filesystem(WORK/'saved.img')
checks={'ps.txt':[b'swapper',b'PID',b'ps'],
        'pstat.txt':[b'active inodes',b'text segments',b'processes',b'1 console',b'open files'],
        'dmesg.txt':[b'Z8000 Unix'], 'uarea.txt':[b'uids',b'comm',b'saved registers'],
        'iostat.txt':[b'HD',b'SW',b'tin'], 'states.txt':[b'user',b'system',b'idle']}
for name,markers in checks.items():
 data=fs.read('/tmp/'+name);(WORK/name).write_bytes(data)
 assert all(m in data for m in markers),(name,data)
 assert b'No namelist' not in data and b'Cannot read' not in data,(name,data)
 assert data.strip(),name
print('PASS native ps, pstat (all tables/u-area), dmesg, iostat (disk/TTY/CPU/buffer cache)')
assert b'inspection memory: passed' in output
print('PASS memory reads/writes, page boundary, limits, bad minor, copy fault and non-root rejection')
bootunix=bytearray((BUILD/'handler.sout').read_bytes())
bootunix[14:18]=(0x1f0).to_bytes(4,'big')
bootunix[40:552]=(BUILD/'kernel.bin').read_bytes().ljust(512,b'\0')
assert fs.read('/tmp/bootunix')==bootunix
print('PASS native boot pack preserves the matching kernel symbol table')

# Use the natively built tools under real memory pressure. The fixture is a
# target C workload, not a substitute implementation of an inspection tool.
compile_c(ROOT/'tools/inspection-pressure.c',WORK/'pressure.b')
run([ROOT/'tests/build/ldz8-host/ldz8','-i','-x',RT/'crt0.b',WORK/'pressure.b',RT/'libc.a','-o',WORK/'pressure'])
pressurefiles={'bin/pressure':WORK/'pressure','unix':BUILD/'handler.sout'}
for name in ('ps','pstat','dmesg','iostat'):
 exe=WORK/name;exe.write_bytes(fs.read('/bin/'+name));pressurefiles['bin/'+name]=exe
image(pressurefiles,WORK/'pressure.img')
with (WORK/'pressure.log').open('wb') as log:
 r=subprocess.run([str(BUILD/'test_driver'),'-d',str(WORK/'pressure.img'),'-R','512',
     '-c','15000000000','-i','pressure\\n','-x','inspection swap: passed'],
     cwd=BUILD,stdout=log,stderr=subprocess.STDOUT,timeout=600)
output=(WORK/'pressure.log').read_bytes()
assert r.returncode==0 and b'inspection swap: passed' in output,output[-4000:]
print('PASS native ps with resident and swapped processes on 512 KiB RAM')
