#!/usr/bin/env python3
"""Exercise native V7 adb tracing, process cores and kernel disk-dump recovery."""
from pathlib import Path
import argparse, struct, subprocess, sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/native-cc'))
from build import image, compile_c, run
from selfhost import Filesystem
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('build',type=Path,nargs='?',default=ROOT/'v7z8000/usr/sys/build')
p.add_argument('--adb',type=Path,default=ROOT/'tests/build/adb/adb')
p.add_argument('--savecore',type=Path,default=ROOT/'tests/build/adb/savecore')
a=p.parse_args()
BUILD=a.build.resolve();WORK=ROOT/'tests/build/adb-runtime';WORK.mkdir(parents=True,exist_ok=True)
RT=ROOT/'tests/build/sout-cc';LD=ROOT/'tests/build/ldz8-host/ldz8'
def executable(name,source,split=False):
 compile_c(source,WORK/(name+'.b'))
 run([LD,*(['-i'] if split else []),RT/'crt0.b',WORK/(name+'.b'),RT/'libc.a','-o',WORK/name])
 return WORK/name
def trial(name,disk,text,expect,extra=(),status=0):
 with (WORK/(name+'.log')).open('wb') as log:
  result=subprocess.run([str(BUILD/'test_driver'),'-d',str(disk),'-c','15000000000','-i',text,'-x',expect,*map(str,extra)],cwd=BUILD,stdout=log,stderr=subprocess.STDOUT,timeout=600)
 output=(WORK/(name+'.log')).read_bytes()
 assert result.returncode==status and expect.encode() in output,output[-6000:]
 return output
source=WORK/'target.c';source.write_text('''#include <stdio.h>
#include <signal.h>
int marker=0x1234;
long wide=0x12345678L;
float single=1.5;
double doub=2.5;
/* Manual chapter 6: LD immediate, ADD register, RET; Appendix C: reserved 9F. */
unsigned decoder[]={0x2100,0x1234,0x8110,0x9e08,0x9f00};
char letters[]="ABCD";
inner(value, crash) int value,crash; {
 marker=value;
 if(crash)kill(getpid(),SIGQUIT);
 puts("adb-target-ok");
}
main(argc,argv) int argc;char **argv; {
 int pid,status;
 if(argc==1){inner(0x4567,0);return marker!=0x4567;}
 pid=fork();if(pid<0)return 1;
 if(pid==0){inner(0x5678,1);exit(2);}
 if(wait(&status)!=pid || !(status&0200))return 3;
 puts("adb-core-ready");return 0;
}
''')
hd=(ROOT/'v7z8000/usr/sys/dev/hd.c').read_text()
constants='\n'.join(line for line in hd.splitlines() if line.startswith('#define '))
probe=WORK/'dumpprobe.c'
probe.write_text('#include <sys/param.h>\n'+constants+'\n'+hd[hd.index('\nhddump('):]+(ROOT/'tools/dumpprobe.c').read_text())
probeexe=executable('dumpprobe',probe)
files={'bin/adb':a.adb,'bin/target':executable('target',source),
       'bin/dumpprobe':probeexe,'bin/targeti':executable('targeti',source,True),'unix':BUILD/'handler.sout'}
commands=WORK/'live';commands.write_text('inner:b\n:r\nmarker/x\nletters/s\n0x5555>r10\n<r10=x\n$r\n:s\n:c\n$q\n');files['tmp/live']=commands
cores=WORK/'inspect';cores.write_text('marker/x\nwide/X\nsingle/f\ndoub/F\nletters/s\nletters+1/s\ndecoder,4/i\n$r\n$c\n$m\n$q\n');files['tmp/inspect']=cores
obsolete=WORK/'obsolete';obsolete.write_bytes(struct.pack('>H',0o411)+bytes(38));files['tmp/obsolete']=obsolete
short=WORK/'short';short.write_bytes(bytes(4096));files['tmp/short']=short
cycle=WORK/'cycle';cycle.write_text('<fp/w <fp\n$c\n$q\n');files['tmp/cycle']=cycle
image(files,WORK/'disk.img',blocks=24000)
trial('polling',WORK/'disk.img','dumpprobe\\n','dump polling: passed')
print('PASS polled dump transfers: bounded busy/DRQ/completion waits and read/write errors',flush=True)
output=trial('live',WORK/'disk.img','cat /tmp/live | adb /bin/target\necho ADB-LIVE-DONE\\n','ADB-LIVE-DONE')
assert b'ABCD' in output and b'5555' in output and b'adb-target-ok' in output,output[-6000:]
assert b'single-step' in output,output[-6000:]
for name in ('target','targeti'):
 output=trial('core-'+name,WORK/'disk.img',name+' core\ncat /tmp/inspect | adb /bin/'+name+' /core\ncat /tmp/cycle | adb -w /bin/'+name+' /core\necho ADB-CORE-DONE\\n','ADB-CORE-DONE')
 assert b'adb-core-ready' in output and b'5678' in output and b'ABCD' in output,output[-6000:]
 assert b'_main(' in output and b'_kill(' in output,output[-6000:]
 assert b'12345678' in output and b'+1.5' in output and b'+2.5' in output,output[-6000:]
 assert b'BCD' in output and b'ld\tr0,#1234' in output and b'add\tr0,r1' in output and b'.word\t#9f00' in output,output[-6000:]
 assert b'invalid frame chain' in b' '.join(output.split()),output[-6000:]
print('PASS adb: one-shot breakpoint continuation, registers, byte order and combined/split process cores',flush=True)
output=trial('malformed',WORK/'disk.img','adb /tmp/obsolete\nadb /bin/target /tmp/short\necho MALFORMED-DONE\\n','MALFORMED-DONE')
flat=b' '.join(output.split())
assert b'invalid NONSEG s.out executable' in flat and b'bad core magic number' in flat,output[-6000:]
# Capture RAM/swap by the kernel's masked, polled panic writer, then recover
# through native Unix raw-disk I/O on a fresh boot. No emulator -K/-W captures.
source=WORK/'panic.c';source.write_text('#include <stdio.h>\nmain(){puts("disk-dump-ready");fflush(stdout);for(;;)pause();}\n')
panexe=executable('panic',source)
plan=WORK/'recover-plan';plan.write_text('0 - /bin/savecore /dev/rhd /usr/sys\n0 ps.txt /bin/ps axlk /unix /usr/sys/core /usr/sys/swap\n')
kcommands=WORK/'kernel';kcommands.write_text('physmem/d\nproc,4/x\n$m\n$r\n$c\n$q\n')
files.update({'bin/panic':panexe,'bin/savecore':a.savecore,
 'bin/pressure':ROOT/'tests/build/inspection/pressure',
 'bin/ps':ROOT/'tests/build/inspection/ps','bin/runner':ROOT/'tests/build/adb/runner',
 'tmp/recover-plan':plan,'tmp/kernel':kcommands})
empty=WORK/'keep';empty.write_bytes(b'');files['usr/sys/.keep']=empty
# Recovery must also protect existing files whose previous modes were public.
files['usr/sys/core']=files['usr/sys/swap']=empty
image(files,WORK/'panic-fs.img',blocks=24000)
reserved=WORK/'reserved.img'
# Each trial uses its own image; remove only this script's previous output.
if reserved.exists():reserved.unlink()
run(['python3',ROOT/'tools/crash-dump.py','reserve',WORK/'panic-fs.img',reserved,'--ram-kib','512'])
kernel=(BUILD/'handler.sout').read_bytes();start=40+struct.unpack_from('>I',kernel,2)[0];size=struct.unpack_from('>H',kernel,12)[0]
symbols={n.rstrip(b'\0'):v for v,t,s,n in struct.iter_unpack('>IBB8s',kernel[start:start+size])}
output=trial('panic',reserved,'pressure dump\\n','panic: kernel access fault',
 ['-R','512','-w','inspection dump: ready','-I','','-F','k:%x'%symbols[b'_time'],'-o',WORK/'dumped.img'],status=1)
assert b'panic dump: 1024 RAM, 8192 swap sectors saved' in output,output[-6000:]
data=(WORK/'dumped.img').read_bytes();base=struct.unpack_from('>I',data,514)[0]*512
assert struct.unpack_from('>4I',data,base)==(0x5a384b44,1,512*1024,4096*1024)
ctxoff=65536+symbols[b'_kcrash']
def context(data,base,kind):
 core=data[base+512:base+512+512*1024]
 ctx=struct.unpack_from('>32H',core,ctxoff)
 assert ctx[:3]==(0x4b43,1,kind),ctx
 assert ctx[3]>0 and (ctx[3]+2)*2048<=len(core),ctx
 assert ctx[20]&0x4000 and ctx[21]==0x8100,ctx
 assert 0xf000<=ctx[17]<0xfffe and 0xf000<=ctx[19]<=0xffff,ctx
 return core,ctx
raw,ctx=context(data,base,2)
assert ctx[24]&1 and ctx[28]==1 and ctx[22]==ctx[29],ctx
frame=ctx[3]*2048+ctx[19]-34-0xf000
words=struct.unpack_from('>17H',raw,frame)
assert tuple(ctx[4:17])==words[:13] and tuple(ctx[20:22])==words[14:16],(ctx,words)
assert ctx[30]==words[16],(ctx,words)
output=trial('recover',WORK/'dumped.img','runner /tmp/recover-plan /tmp\ncat /tmp/kernel | adb -k /unix /usr/sys/core\necho DISK-RECOVERY-DONE\\n','DISK-RECOVERY-DONE',['-o',WORK/'recovered.img'])
assert b'NATIVE CC PASS' in output and b'FAILED command' not in output,output[-6000:]
assert b'256' in output and b'core' in output,output[-6000:]
assert b'_clock(' in output and b'_nviwrap(' in output and b'no saved kernel' not in output,output[-6000:]
fs=Filesystem(WORK/'recovered.img');core=fs.read('/usr/sys/core');swap=fs.read('/usr/sys/swap')
for path in ('/usr/sys/core','/usr/sys/swap'):
 number=2
 for component in path.strip('/').split('/'):
  directory=fs.data(number)
  number=next(int.from_bytes(directory[i:i+2],'big') for i in range(0,len(directory),16)
   if directory[i+2:i+16].split(b'\0')[0].decode()==component)
 offset=((number+15)//8)*512+((number+15)%8)*64
 assert int.from_bytes(fs.disk[offset:offset+2],'big')&0o777==0o600
assert core==data[base+512:base+512+len(core)] and len(core)==512*1024
assert swap==data[base+512+len(core):base+512+len(core)+len(swap)] and len(swap)==4096*1024
report=fs.read('/tmp/ps.txt');assert b'pressure' in report and b'runner' not in report,report
rows=[line.split() for line in report.splitlines()[1:]]
pressure=[row for row in rows if row[-2:]==[b'pressure',b'dump']]
assert len(pressure)==11 and any(int(row[0],8)&1 for row in pressure) and any(not int(row[0],8)&1 for row in pressure),report
assert any(row[1]==b'T' for row in pressure) and b'<defunct>' in report,report
print('PASS kernel-written panic RAM/swap dump; native savecore, ps k and adb -k after reboot',flush=True)
# A tiny swap device fails V7 exec argument reservation and calls panic
# directly. This tests the assembly snapshot without a pre-existing trap frame.
output=trial('explicit-panic',reserved,'','panic: Out of swap',
 ['-R','512','-S','4','-o',WORK/'explicit.img'],status=1)
assert b'panic dump: 1024 RAM, 8 swap sectors saved' in output,output[-6000:]
explicit=(WORK/'explicit.img').read_bytes()
assert struct.unpack_from('>4I',explicit,base)==(0x5a384b44,1,512*1024,4096)
raw,explicitctx=context(explicit,base,1)
output=trial('explicit-recover',WORK/'explicit.img',
 'savecore /dev/rhd /usr/sys\ncat /tmp/kernel | adb -k /unix /usr/sys/core\necho EXPLICIT-DONE\\n',
 'EXPLICIT-DONE')
assert b'_exec(' in output and b'_scwrap(' in output and b'no saved kernel' not in output,output[-6000:]
print('PASS kernel register snapshots: original access-fault frame and direct panic caller; native adb registers and stack walks',flush=True)
# Context-free RAM remains inspectable; corrupt version or physical stack
# mappings must be rejected rather than creating fabricated registers.
for name,word,value in [('noctx',2,0),('badctx',1,2),('badstack',3,256)]:
 damaged=bytearray(core);struct.pack_into('>H',damaged,ctxoff+word*2,value)
 path=WORK/name;path.write_bytes(damaged);files['tmp/'+name]=path
image(files,WORK/'contexts.img',blocks=24000)
output=trial('context-validation',WORK/'contexts.img',
 'cat /tmp/kernel | adb -k /unix /tmp/noctx\n'
 'adb -k /unix /tmp/badctx\nadb -k /unix /tmp/badstack\necho CONTEXTS-DONE\\n',
 'CONTEXTS-DONE')
flat=b' '.join(output.split())
assert b'no saved kernel register context' in flat and b'256' in output,output[-6000:]
assert flat.count(b'invalid kernel register context')>=2,output[-6000:]
print('PASS context-free RAM inspection and rejection of invalid context version/stack mapping',flush=True)
output=trial('panic-error',reserved,'panic\\n','panic: kernel access fault',
 ['-R','512','-w','disk-dump-ready','-I','','-F','k:%x'%symbols[b'_time'],
  '-E','r:4','-o',WORK/'failed.img'],status=1)
assert b'panic dump: I/O error; incomplete' in output,output[-6000:]
assert (WORK/'failed.img').read_bytes()[base:base+512]==bytes(512)
output=trial('reject-incomplete',WORK/'failed.img','savecore /dev/rhd /usr/sys\necho INCOMPLETE-DONE\\n','INCOMPLETE-DONE')
assert b'no complete crash dump' in output,output[-6000:]
print('PASS failed panic swap I/O leaves an invalid commit; native savecore rejects it',flush=True)
output=trial('short-kernel',WORK/'disk.img','adb -k /unix /tmp/short\necho SHORT-KERNEL-DONE\\n','SHORT-KERNEL-DONE')
assert b'incomplete kernel RAM dump' in b' '.join(output.split()),output[-6000:]
print('PASS adb rejects obsolete executables and malformed process/kernel dumps',flush=True)
