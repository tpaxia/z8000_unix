#!/usr/bin/env python3
"""Manual-derived Z8000 decoder vectors, host bounds sweep and native adb trial.

Authority: ~/Projects/documents/z8000.md, chapter 6 and Appendix C. Words here
are literal manual encodings, independent of the assembler and emulator decoder.
"""
import argparse, re, struct, subprocess, sys
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
sys.dont_write_bytecode=True
WORK=ROOT/'tests/build/adb-decoder'
# Each vector ends immediately after the instruction; a NOP is appended as an
# independent boundary check. Relative addresses are substituted at actual PC.
VECTORS=[]
def v(words,text,seg=0): VECTORS.append((seg,words,text))
# Ch.6 arithmetic/logical byte, word, long: R, IM, IR, DA, X; ADC/SBC.
for i,name in enumerate(('add','sub','or','and','xor','cp')):
 for byte in (0,1):
  h=2*i+1-byte;suffix='b' if byte else '';dst='rh2' if byte else 'r2';src='rh4' if byte else 'r4'
  v([((h+0x80)<<8)|0x42],f'{name}{suffix} {dst},{src}')
  v([(h<<8)|2,0x1234],f'{name}{suffix} {dst},#{"0034" if byte else "1234"}')
  v([(h<<8)|0x42],f'{name}{suffix} {dst},@r4')
  v([((h+0x40)<<8)|2,0x1234],f'{name}{suffix} {dst},011064')
  v([((h+0x40)<<8)|0x42,0x1234],f'{name}{suffix} {dst},011064(r4)')
  v([((h+0x40)<<8)|0x42,0x0156],f'{name}{suffix} {dst},0001:0056(r4)',1)
  v([((h+0x40)<<8)|2,0x8100,0x1234],f'{name}{suffix} {dst},0001:1234',1)
for h,name in ((0x10,'cpl'),(0x12,'subl'),(0x14,'ldl'),(0x16,'addl'),(0x18,'multl'),(0x19,'mult'),(0x1a,'divl'),(0x1b,'div')):
 dst='rq0' if h in (0x18,0x1a) else 'rr0';src='r4' if h in (0x19,0x1b) else 'rr4'
 v([((h+0x80)<<8)|0x40],f'{name} {dst},{src}')
 words=[h<<8,0x1234]+([] if h in (0x19,0x1b) else [0x5678])
 v(words,f'{name} {dst},#'+('1234' if len(words)==2 else '12345678'))
 v([(h<<8)|0x40],f'{name} {dst},@r4')
 v([((h+0x40)<<8),0x8100,0x1234],f'{name} {dst},0001:1234',1)
for h,name in ((0xb4,'adcb'),(0xb5,'adc'),(0xb6,'sbcb'),(0xb7,'sbc')):
 v([(h<<8)|0x42],f'{name} {"rh2,rh4" if h%2==0 else "r2,r4"}')
# Ch.6 LD/LDR/LDA/LDAR, including BA and BX; immediate byte in opcode.
for h,name,dst in ((0x20,'ldb','rh2'),(0x21,'ld','r2')):
 v([(h<<8)|2,0x1234],f'{name} {dst},#{"0034" if h==0x20 else "1234"}')
 v([(h<<8)|0x42],f'{name} {dst},@rr4',1)
 v([((h+0x40)<<8)|2,0x017f],f'{name} {dst},0001:007f',1)
 v([((h+0x80)<<8)|0x42],f'{name} {dst},{"rh4" if h==0x20 else "r4"}')
v([0xca12],'ldb rl2,#0012')
for h,name in ((0x2e,'ldb'),(0x2f,'ld'),(0x1d,'ldl')):
 src='rr2' if h==0x1d else ('rh2' if h==0x2e else 'r2')
 v([(h<<8)|0x42],f'{name} @r4,{src}')
 v([((h+0x40)<<8)|2,0x8100,0x1234],f'{name} 0001:1234,{src}',1)
for h,name,dst in ((0x30,'ldb','rh2'),(0x31,'ld','r2'),(0x35,'ldl','rr2')):
 v([(h<<8)|0x42,0xfffe],f'{name} {dst},r4(#fffe)')
 v([(h<<8)|2,0xfffe],f'{"ldrl" if h==0x35 else "ldrb" if h==0x30 else "ldr"} {dst},{{pc2}}')
for h,name,src in ((0x32,'ldb','rh2'),(0x33,'ld','r2'),(0x37,'ldl','rr2')):
 v([(h<<8)|0x42,0x1234],f'{name} rr4(#1234),{src}',1)
 v([(h<<8)|2,0xfffc],f'{"ldrl" if h==0x37 else "ldrb" if h==0x32 else "ldr"} {{pc0}},{src}')
for h,name,dst in ((0x70,'ldb','rh2'),(0x71,'ld','r2'),(0x75,'ldl','rr2'),(0x74,'lda','rr2')):
 v([(h<<8)|0x42,0x0600],f'{name} {dst},rr4(r6)',1)
for h,name,src in ((0x72,'ldb','rh2'),(0x73,'ld','r2'),(0x77,'ldl','rr2')):
 v([(h<<8)|0x42,0x0600],f'{name} rr4(r6),{src}',1)
v([0x7602,0x8100,0x1234],'lda rr2,0001:1234',1)
v([0x3442,0xfffe],'lda rr2,rr4(#fffe)',1)
v([0x3402,0xfffe],'ldar rr2,{pc2}',1)
v([0xbd29],'ldk r2,#0009')
# Ch.6 unary/bit operations and immediate loads to memory.
for b,name in ((0,'com'),(1,'cp'),(2,'neg'),(4,'test'),(5,'ld'),(6,'tset'),(8,'clr')):
 for byte in (0,1):
  h=0x0d-byte;suffix='b' if byte else '';tail=',#1234' if b in (1,5) else ''
  if byte and tail:tail=',#0034'
  v([(h<<8)|0x40|b]+([0x1234] if tail else []),f'{name}{suffix} @r4{tail}')
  v([((h+0x40)<<8)|b,0x8100,0x1234]+([0x1234] if tail else []),f'{name}{suffix} 0001:1234{tail}',1)
  if not tail:v([((h+0x80)<<8)|0x40|b],f'{name}{suffix} {"rh4" if byte else "r4"}')
for h,name in ((0x22,'resb'),(0x23,'res'),(0x24,'setb'),(0x25,'set'),(0x26,'bitb'),(0x27,'bit'),(0x28,'incb'),(0x29,'inc'),(0x2a,'decb'),(0x2b,'dec')):
 v([((h+0x80)<<8)|0x43],f'{name} {"rh4" if h%2==0 else "r4"},#{"0004" if h>=0x28 else "0003"}')
 v([((h+0x40)<<8)|3,0x8100,0x1234],f'{name} 0001:1234,#{"0004" if h>=0x28 else "0003"}',1)
 if h<0x28:v([(h<<8)|3,0x0400],f'{name} {"rh4" if h%2==0 else "r4"},r3')
v([0x9c28],'testl rr2');v([0x1c48],'testl @rr4',1);v([0x5c08,0x0156],'testl 0001:0056',1)
# Ch.6 PUSH/POP/LDM: destination first, extension order count before address.
for h,name in ((0x11,'pushl'),(0x13,'push'),(0x15,'popl'),(0x17,'pop')):
 pop=h in (0x15,0x17);reg='rr2' if h in (0x11,0x15) else 'r2'
 v([(h<<8)|0x42],f'{name} @r2,@r4' if pop else f'{name} @r4,@r2')
 v([((h+0x80)<<8)|0x42],f'{name} {reg},@r4' if pop else f'{name} @r4,{reg}')
 v([((h+0x40)<<8)|0x42,0x8100,0x1234],f'{name} 0001:1234(r2),@rr4' if pop else f'{name} @rr4,0001:1234(r2)',1)
v([0x0d49,0x1234],'push @rr4,#1234',1)
v([0x1c41,0x020f],'ldm r2,@r4,#16')
v([0x5c09,0x020f,0x8100,0x1234],'ldm 0001:1234,r2,#16',1)
v([0x5c01,0x0200,0x017f],'ldm r2,0001:007f,#1',1)
# Ch.6 branches, byte/word loop counters, all condition codes.
CC=('f','lt','le','ule','ov','mi','eq','c','t','ge','gt','ugt','nov','pl','ne','nc')
for c,name in enumerate(CC):
 v([0xe000+(c<<8)+0xff],f'jr {name},{{pc0}}')
 v([0x5e00+c,0x8100,0x1234],f'jp {name},0001:1234',1)
 v([0x9e00+c],f'ret {name}')
 v([0xaf40+c],f'tcc {name},r4')
v([0x1f40],'call @rr4',1);v([0x5f00,0x8100,0x1234],'call 0001:1234',1)
v([0xdfff],'calr disp -1');v([0xd800],'calr disp -2048');v([0xd7ff],'calr disp 2047')
v([0xf481],'djnz r4,{pc0}');v([0xfc01],'dbjnz rl4,{pc0}')
# Ch.6 exchange, extensions, decimal, rotate/shift (byte sign is eight bits).
for h,name in ((0x2c,'exb'),(0x2d,'ex')):
 v([((h+0x80)<<8)|0x42],f'{name} {"rh2,rh4" if h%2==0 else "r2,r4"}')
 v([((h+0x40)<<8)|2,0x017f],f'{name} {"rh2" if h%2==0 else "r2"},0001:007f',1)
v([0xb040],'dab rh4');v([0xb140],'extsb r4');v([0xb147],'extsl rq4');v([0xb14a],'exts rr4')
v([0xbc42],'rrdb rh2,rh4');v([0xbe42],'rldb rh2,rh4')
for h in (0xb2,0xb3):
 for b,name in ((0,'rl'),(4,'rr'),(8,'rlc'),(12,'rrc')):
  for n in (1,2):v([(h<<8)|0x40|b|((n-1)<<1)],f'{name}{"b" if h==0xb2 else ""} {"rh4" if h==0xb2 else "r4"},#{n}')
 for b,name in ((1,'sll'),(9,'sla')):
  for w,shift in ((2,name),(0xfffe,'srl' if b==1 else 'sra')):
   v([(h<<8)|0x40|b,w],f'{shift}{"b" if h==0xb2 else ""} {"rh4" if h==0xb2 else "r4"},#2')
 for b,name in ((3,'sdl'),(11,'sda')):
  v([(h<<8)|0x40|b,0x0200],f'{name}{"b" if h==0xb2 else ""} {"rh4" if h==0xb2 else "r4"},r2')
for b,name in ((5,'slll'),(13,'slal')):v([0xb340|b,2],f'{name} rr4,#2')
for b,name in ((7,'sdll'),(15,'sdal')):v([0xb340|b,0x0200],f'{name} rr4,r2')
# Ch.6 strings: count and destination in extension word, not opcode.
for h in (0xba,0xbb):
 suffix='b' if h==0xba else ''
 for b,n in ((1,'ldi'),(9,'ldd')):
  for c in (8,0):v([(h<<8)|0x40|b,0x0620|c],f'{n}{"r" if c==0 else ""}{suffix} @rr2,@rr4,r6',1)
 for b,name in ((0,'cpi'),(2,'cpsi'),(4,'cpir'),(6,'cpsir'),(8,'cpd'),(10,'cpsd'),(12,'cpdr'),(14,'cpsdr')):
  dst='@rr2' if b&2 else ('rh2' if h==0xba else 'r2')
  v([(h<<8)|0x40|b,0x0626],f'{name}{suffix} {dst},@rr4,r6,eq',1)
for b,name in ((0,'trib'),(2,'trtib'),(4,'trirb'),(6,'trtirb'),(8,'trdb'),(10,'trtdb'),(12,'trdrb'),(14,'trtdrb')):
 v([0xb840|b,0x0620|(14 if b in (6,14) else 0)],f'{name} @rr4,@rr2,r6',1)
# Ch.6 privileged CPU control: use detailed entries, not Appendix C typos.
for w,name in ((0x7a00,'halt'),(0x7b00,'iret'),(0x7b08,'mset'),(0x7b09,'mres'),(0x7b0a,'mbit'),(0x8d07,'nop')):v([w],name)
v([0x7b4d],'mreq r4');v([0x7f2a],'sc #002a')
for c,name in ((2,'fcw'),(3,'refresh'),(4,'psapseg'),(5,'psapoff'),(6,'nspseg'),(7,'nspoff')):
 v([0x7d40|c],f'ldctl r4,{name}',1);v([0x7d48|c],f'ldctl {name},r4',1)
v([0x7d45],'ldctl r4,psap');v([0x7d4f],'ldctl nsp,r4')
v([0x8c41],'ldctlb rh4,flags');v([0x8c49],'ldctlb flags,rh4')
for b,name in ((1,'setflg'),(3,'resflg'),(5,'comflg')):v([0x8df0|b],f'{name} c,z,s,v')
for b,name in ((0,'di'),(4,'ei')):
 for c,arg in ((0,'#0'),(1,'nvi'),(2,'vi'),(3,'vi,nvi')):v([0x7c00+b+c],f'{name} {arg}')
v([0x3940],'ldps @rr4',1);v([0x7900,0x8100,0x1234],'ldps 0001:1234',1)
# Ch.6 I/O: ports always use word registers, even in SEG.
for h,name in ((0x3c,'inb'),(0x3d,'in'),(0x3e,'outb'),(0x3f,'out')):
 arg='rh2' if h%2==0 else 'r2';v([(h<<8)|0x42],f'{name} @r4,{arg}' if h>=0x3e else f'{name} {arg},@r4',1)
for h in (0x3a,0x3b):
 for b,name in ((4,'in'),(5,'sin'),(6,'out'),(7,'sout')):
  arg='rh4' if h==0x3a else 'r4';name+= 'b' if h==0x3a else ''
  v([(h<<8)|0x40|b,0x1234],f'{name} #1234,{arg}' if b&2 else f'{name} {arg},#1234',1)
 for b in (0,1,2,3,8,9,10,11):
  for c in (0,8):
   name=('s' if b&1 else '')+ ('otdr' if b&8 else 'otir') if b&2 and c==0 else None
   if name is None:name=('s' if b&1 else '')+ ('outd' if b&8 else 'outi') if b&2 else ('s' if b&1 else '')+('indr' if b&8 else 'inir') if c==0 else ('s' if b&1 else '')+('ind' if b&8 else 'ini')
   name+='b' if h==0x3a else ''
   args='@r2,@rr4,r6' if b&2 else '@rr2,@r4,r6'
   v([(h<<8)|0x40|b,0x0620|c],f'{name} {args}',1)
# Ch.6.8 EPA templates; arbitrary EPU bits remain visible.
v([0x0f44,0x1203],'epu epu,@rr4,#4,#1203,#0',1)
v([0x0f4c,0x1203],'epu @rr4,epu,#4,#1203,#0',1)
v([0x4f04,0x1203,0x8100,0x1234],'epu epu,0001:1234,#4,#1203,#0',1)
v([0x4f0c,0x1203,0x017f],'epu 0001:007f,epu,#4,#1203,#0',1)
v([0x8f00,0x1203],'epu r2,epu,#4,#1203,#0000')
v([0x8f08,0x1203],'epu epu,r2,#4,#1203,#0008')
v([0x8e00,0x1020],'epu flags,epu,#1020,#0000')
v([0x8e08,0x0020],'epu epu,flags,#0020,#0008')
v([0x8e04,0x1203],'epu internal,#1203,#0004')
# Appendix C reserved opcodes; zero IR fields have no defined memory operand.
for w in (0x3600,0x7800,0x9d00,0x9f00,0xb900,0xbf00,0x1f00,0x2f00,0x7d40):v([w],f'.word #{w:04x}')

def expected(text,pc):
 return text.format(pc0='0'+format(pc&65535,'o'),pc2='0'+format((pc+2)&65535,'o'))
def normal(text):return ' '.join(text.split())

def host():
 # Run the real source with memory/output adapters, without copying decoding
 # logic. On a host int is wider: nextword is still exactly a 16-bit word.
 source=(ROOT/'v7z8000/usr/src/cmd/adb/opset.c').read_text()
 adapter='''#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#define L_INT long
#define INT int
#define DSYM 1
#define ISYM 2
static unsigned words[4];static int quiet;
long inkdot(int);int chkget(long,int);int prints(char *);int printc(int);
long leng(unsigned);int psymoff(long,int,char *);
'''
 tail='''
long inkdot(int n){return dot+n;}
int chkget(long addr,int space){int n=(addr-dot)/2;
 if(n<0||n>3){fprintf(stderr,"decoder overread\\n");exit(1);}return words[n];}
long leng(unsigned w){return w;}
int prints(char *s){if(!quiet)fputs(s,stdout);return 0;}
int printc(int c){if(!quiet)putchar(c);return 0;}
int psymoff(long v,int t,char *s){if(!quiet)printf("0%lo%s",v,s);return 0;}
int main(int argc,char **argv){unsigned w;int i;
 if(argc==2){quiet=1;freopen("/dev/null","w",stdout);
 for(adbseg=0;adbseg<2;adbseg++)for(w=0;w<65536;w++){
  words[0]=w;words[1]=0;words[2]=0;words[3]=0;dot=0x1000;
  printins(0,0,w);if(dotinc<2||dotinc>8||dotinc%2)return 1;
 }return 0;}
 while(scanf("%d %lx %x %x %x %x",&adbseg,&dot,&words[0],&words[1],&words[2],&words[3])==6){
  printins(0,0,words[0]);printf("|%d\\n",dotinc);
 }return 0;}
'''
 path=WORK/'probe.c';path.write_text(adapter+source.replace('#include "defs.h"','')+tail)
 exe=WORK/'probe'
 subprocess.run(['cc','-std=gnu89','-Wno-implicit-int','-Wno-return-type','-Wno-deprecated-non-prototype',str(path),'-o',str(exe)],check=True)
 data=''.join(f'{seg} 1000 '+ ' '.join(f'{w:x}' for w in words+[0]*(4-len(words)))+'\n' for seg,words,text in VECTORS)
 out=subprocess.run([str(exe)],input=data,text=True,capture_output=True,check=True).stdout.splitlines()
 assert len(out)==len(VECTORS)
 for i,((seg,words,text),line) in enumerate(zip(VECTORS,out)):
  display,length=line.rsplit('|',1)
  assert int(length)==len(words)*2 and normal(display)==normal(expected(text,0x1000)),(i,seg,words,display,length,text)
 subprocess.run([str(exe),'bounds'],check=True)
 print(f'PASS {len(VECTORS)} manual decoder vectors and 131072 opcode/mode bounds checks',flush=True)

def native(build,adb):
 sys.path.insert(0,str(ROOT/'tools/native-cc'))
 from build import compile_c,image,run
 from selfhost import Filesystem
 offsets=[];words=[];commands=['0$s']
 for i,(seg,instruction,text) in enumerate(VECTORS):
  offsets.append(len(words)*2);words+=instruction+[0x8d07]
  loc=f'vector+{offsets[-1]}'
  commands.extend([f'{seg}$z',f'{loc}/"V{i:03d}"n',f'{loc},2/i'])
 # Restrict the data map to three words of a four-word SEG EPA instruction.
 # The final offset must fail through adb's ordinary map/read error path.
 trunc=next(i for i,(seg,instruction,text) in enumerate(VECTORS) if instruction==[0x4f04,0x1203,0x8100,0x1234])
 loc=f'vector+{offsets[trunc]}'
 commands+=['2$z','1$z',f'{loc}/m {loc} {loc}+6 4096+{loc}',f'{loc}/i','$q']
 src=WORK/'vector.c';src.write_text('#include <signal.h>\nunsigned vector[]={'+','.join(hex(w) for w in words)+'};\nmain(){kill(getpid(),SIGQUIT);return 1;}\n')
 compile_c(src,WORK/'vector.b')
 rt=ROOT/'tests/build/sout-cc';ld=ROOT/'tests/build/ldz8-host/ldz8'
 run([ld,'-i',rt/'crt0.b',WORK/'vector.b',rt/'libc.a','-o',WORK/'vector'])
 script=WORK/'inspect';script.write_text('\n'.join(commands)+'\n')
 image({'bin/adb':adb,'bin/vector':WORK/'vector','tmp/inspect':script},WORK/'disk.img',blocks=24000)
 with (WORK/'native.log').open('wb') as log:
  r=subprocess.run([str(build/'test_driver'),'-d',str(WORK/'disk.img'),'-c','15000000000',
   '-i','vector\ncat /tmp/inspect | adb /bin/vector /core\necho DECODER-DONE\\n','-x','DECODER-DONE','-o',str(WORK/'saved.img')],cwd=build,stdout=log,stderr=subprocess.STDOUT,timeout=600)
 output=(WORK/'native.log').read_bytes();assert r.returncode==0,output[-6000:]
 # Read the actual executable symbol value so PC-relative vectors compare
 # addresses independently of compiler layout. /i reads the process data map.
 obj=(WORK/'vector').read_bytes();start=40+struct.unpack_from('>I',obj,2)[0];size=struct.unpack_from('>H',obj,12)[0]
 symbols={name.rstrip(b'\0'):v for v,t,s,name in struct.iter_unpack('>IBB8s',obj[start:start+size])}
 console=output[:output.index(b'Result:')].decode(errors='replace') if b'Result:' in output else output.decode(errors='replace').split('Console output:')[0]
 pieces=re.split(r'V(\d{3})\r?\n',console);assert (len(pieces)-1)//2==len(VECTORS),console[-6000:]
 for i,(seg,words,text) in enumerate(VECTORS):
  block=pieces[2*i+2].split('instruction mode:')[0]
  assert normal(expected(text,symbols[b'_vector']+offsets[i])) in normal(block),(i,words,text,block)
  assert re.search(r'\bnop\b',block),(i,'wrong instruction boundary',block)
 assert b'data address not found' in b' '.join(output.split()) and b'use 0$z' in output,output[-6000:]
 print(f'PASS native adb decodes {len(VECTORS)} manual vectors and following NOP boundaries inside Unix',flush=True)

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--native',action='store_true')
 p.add_argument('--build',type=Path,default=ROOT/'v7z8000/usr/sys/build');p.add_argument('--adb',type=Path,default=ROOT/'tests/build/adb/adb')
 a=p.parse_args();WORK.mkdir(parents=True,exist_ok=True);host()
 if a.native:native(a.build.resolve(),a.adb.resolve())
