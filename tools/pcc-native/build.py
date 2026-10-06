from pathlib import Path
import subprocess,sys,struct,json
sys.dont_write_bytecode=True
if any(arg != '--no-compact' for arg in sys.argv[1:]):
 raise SystemExit('usage: build.py [--no-compact]')
root=Path(__file__).resolve().parents[2];w=root/'tests/build/native-pcc';pcc=root/'PCC-z8000/z8000';tools=root/'tools'
def run(c,**kw):
 r=subprocess.run(list(map(str,c)),capture_output=True,**kw)
 if r.returncode: print((r.stdout+r.stderr).decode(errors='replace'),flush=True)
 r.check_returncode()
 return r
run(['make','-C',pcc/'cz8'])
run(['make','-C',pcc/'az8'])
run(['make','-C',pcc/'test','../ldz8'])
run(['make','-C',tools,'libv7.a','libc/crt0.b','v7mkfs','sh','init'])
run([sys.executable,Path(__file__).with_name('prepare.py'),w])
if '--no-compact' in sys.argv[1:]:
 compact=lambda assembly: assembly
else:
 run(['make','-C',pcc/'test','../oz8'])
 def compact(assembly): return run([pcc/'oz8'],input=assembly.encode()).stdout.decode()
names=run(['ar','t',tools/'libv7.a']).stdout.decode().split()
paths=[]
for n in names:
 if n in ['setjmp.b','syscalls.b']:p=tools/'libc'/n
 elif n=='arith.b':p=tools/n
 else:p=tools/'libv7'/n
 paths.append(p.with_suffix('.az8'))
run(['make','-C',tools,*[str(p.relative_to(tools)) for p in paths]])
lib=w/'lib';lib.mkdir(exist_ok=True)
for src in paths:
 (lib/src.name).write_text(compact(src.read_text()));run([pcc/'az8/az8','-o',src.stem+'.b',src.name],cwd=lib)
(w/'libv7.a').unlink(missing_ok=True)
run(['ar','cr',w/'libv7.a',*[lib/n for n in names]])

report={}
for phase, sources in [('front','cgram xdefs scan pftn trees optim code local comm1 frontglue'),('back','reader local2 order match allo comm2 table backglue')]:
 d=w/('target-'+phase);d.mkdir(exist_ok=True);objects=[]
 for name in sources.split():
  pre=run(['cpp','-nostdinc','-undef','-Dz8000','-Dz8002','-DBUG1','-DBUG2','-DBUG3','-DBUG4','-I'+str(w),'-I'+str(root/'v7z8000/usr/include'),w/(name+'.c')])
  compiled=run([pcc/'cz8/cz8'],input=pre.stdout)
  (d/(name+'.log')).write_bytes(pre.stderr+compiled.stderr)
  (d/(name+'.az8')).write_text(compact(compiled.stdout.decode()))
  run([pcc/'az8/az8','-o',name+'.b',name+'.az8'],cwd=d)
  objects.append(d/(name+'.b'))
 run([pcc/'ldz8','-i','-x',tools/'libc/crt0.b',*objects,w/'libv7.a','-o',d/phase])
 header=struct.unpack('>8H',(d/phase).read_bytes()[:16])
 report[phase]={'text':header[1],'data':header[2],'bss':header[3]}
 print(phase,report[phase],flush=True)
(w/'sizes.json').write_text(json.dumps(report,indent=2)+'\n')
