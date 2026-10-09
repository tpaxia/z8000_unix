#!/usr/bin/env python3
"""Stage sources, run native make, and disk-boot the native kernel and loader.

Python constructs the trial filesystem and monitors the emulator. Every target
compile, assembly, link, source adaptation and boot installation runs in V7.
"""
import argparse
import fcntl
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import time
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/native-cc'))
from build import image
from selfhost import Filesystem
HERE = Path(__file__).resolve().parent
WORK = ROOT/'tests/build/native-system'
SYS = ROOT/'v7z8000/usr/sys'
BUILD = SYS/'build'


def recipes(selection):
    asm = selection['asm'].split(';')
    sources = selection['c'].split(';')
    objects = ['machine/'+Path(s).stem+'.b' for s in asm]+[str(Path(s).with_suffix('.b')) for s in sources]+['arith.b','csv.b']
    headers = sorted('h/'+s.name for s in (SYS/'h').glob('*.h'))
    make = 'CC=/bin/cc\nAS=/bin/asz8k\nLD=/bin/ldz8\nCFLAGS=-O -Dz8000 -Dz8002 -I../h\n'
    make += 'all: handler.sout kernel.bin rom.bin fpe.bin unix\n'
    for s, obj in zip(asm, objects):
        az8 = str(Path(obj).with_suffix('.az8'))
        make += f'{obj}: {s}\n\t/bin/cp {s} {az8}\n\t$(AS) -c -o {obj} {az8}\n'
    for s in sources:
        p = Path(s); obj = str(p.with_suffix('.b'))
        # Native cc writes the object in its working directory.
        local = sorted(str(h.relative_to(SYS)) for h in (SYS/p.parent).glob('*.h'))
        make += f'{obj}: {s} '+' '.join(headers+local)+f'\n\tcd {p.parent}; $(CC) $(CFLAGS) -c {p.name}\n'
    for name in ('arith','csv'):
        make += f'{name}.b: {name}.az8\n\t$(AS) -c -o {name}.b {name}.az8\n'
    make += 'handler.sout: '+' '.join(objects)+'\n\t$(LD) -i -x '+' '.join(objects)+' -o handler.sout\n'
    for name, source, flags in [('rom',selection['rom'],'-C 0'),('kernel',selection['traps'],'-C 1 -M 512')]:
        make += f'{name}.so: {source}\n\t$(AS) -gs -o {name}.so {source}\n'
        make += f'{name}.bin: {name}.so\n\t$(LD) -b {flags} {name}.so -o {name}.bin\n'
    make += '''pack: pack.c
	$(CC) -O -i pack.c -o pack
fpe/core.s: fpe/fpe.z8k fpe.sed pack
	./pack fpecut fpe/fpe.z8k fpe/cut.s
	/bin/sed -f fpe.sed fpe/cut.s > fpe/core.s
fpe/core.so: fpe/core.s
	$(AS) -gs -o fpe/core.so fpe/core.s
fpe/unix.so: fpe/unix.s
	$(AS) -gs -o fpe/unix.so fpe/unix.s
fpe.bin: fpe/core.so fpe/unix.so
	$(LD) -b -C 127 -M 61440 fpe/unix.so fpe/core.so -o fpe.bin
unix: handler.sout kernel.bin pack
	./pack kernel handler.sout kernel.bin unix
'''
    boot = '''CC=/bin/cc
AS=/bin/asz8k
LD=/bin/ldz8
PACK=../sys/pack
all: boot block.bin unix.rom
boot.b: boot.c saio.h
	$(CC) -O -I. -c boot.c
syswrap.b: syswrap.c SYS.c saio.h
	$(CC) -O -I. -c syswrap.c
prf.c: fullprf.c $(PACK)
	$(PACK) prfcut fullprf.c prf.c
prf.b: prf.c
	$(CC) -O -I. -c prf.c
l3.b: l3.c
	$(CC) -O -Dinterdata -c l3.c
start.b: start.az8
	$(AS) -c -o start.b start.az8
boot: start.b boot.b syswrap.b prf.b l3.b
	$(LD) -s start.b boot.b syswrap.b prf.b l3.b /lib/libc.a -o boot
rom.s: board.s ../sys/machine/emurom.s $(PACK)
	$(PACK) handoff board.s ../sys/machine/emurom.s rom.s
rom.so: rom.s
	$(AS) -gs -o rom.so rom.s
rom.bin: rom.so
	$(LD) -b -C 0 rom.so -o rom.bin
unix.rom: rom.bin $(PACK)
	$(PACK) pad rom.bin unix.rom 2048 255
block.so: block.s
	$(AS) -gs -o block.so block.s
block.bin: block.so
	$(LD) -b -C 3 -T 65024 block.so -o block.bin
install: all ../sys/unix ../sys/fpe.bin
	/bin/cp boot /boot
	/bin/cp ../sys/unix /unix
	/bin/cp ../sys/fpe.bin /fpe
	/bin/rm -f /dev/hd0
	/bin/mknod /dev/hd0 b 1 0
	$(PACK) install block.bin /boot /dev/hd0
'''
    return make, boot, objects


def setup():
    subprocess.run(['cmake','-S',str(SYS),'-B',str(BUILD),'-DKERNEL_CONFIG=emulated'],check=True)
    selection = dict(line.split('=',1) for line in (BUILD/'native-sources.txt').read_text().splitlines())
    kmake, bmake, objects = recipes(selection)
    files = {}; modes = {}
    # Only verified native tools/runtime seed this build. No kernel or boot
    # objects, generated assembly, translated FPE source or executables enter it.
    native = ROOT/'tests/build/native-environment-sout/native'
    for source in native.rglob('*'):
        if source.is_file(): files[str(source.relative_to(native))] = source
    fs = Filesystem(ROOT/'tests/build/userland-native-sout/hd.img')
    for name in ('sh','sed','cp','rm','mknod','sync','ls','cmp'):
        source = WORK/'seed'/name; source.parent.mkdir(exist_ok=True)
        source.write_bytes(fs.read('/bin/'+name));files['bin/'+name]=source;modes['bin/'+name]=0o755
    for name in ('runner','check'):
        source = WORK/'seed'/name;source.write_bytes(fs.read('/bin/'+name));files['bin/'+name]=source;modes['bin/'+name]=0o755
    required = set(selection['asm'].split(';')+selection['c'].split(';')+[selection['rom'],selection['traps'],'fpe/fpe.z8k','fpe/unix.s'])
    for directory in ('h','sys','machine','dev','conf'):
        required.update(str(p.relative_to(SYS)) for p in (SYS/directory).glob('*.h'))
    for name in required: files['usr/src/sys/'+name]=SYS/name
    for name in ('arith','csv'):files['usr/src/sys/'+name+'.az8']=ROOT/'PCC-z8000/z8000/lib'/(name+'.az8')
    for source in (ROOT/'v7z8000/usr/src/cmd/cpp').iterdir():
        if source.is_file():files['usr/src/cpp/'+source.name]=source
    files['usr/src/cpp/cppprobe.c']=ROOT/'tools/native-cc/cppprobe.c'
    files['usr/lib/yaccpar']=ROOT/'PCC-z8000/z8000/yacc/yaccpar'
    files['usr/src/sys/pack.c']=HERE/'pack.c';files['usr/src/sys/fpe.sed']=HERE/'fpe.sed'
    for name in ('boot.c','start.az8','block.s'):files['usr/src/boot/'+name]=ROOT/'mame/boot'/name
    files['usr/src/boot/board.s']=ROOT/'mame/boot/rom.s'
    original=ROOT/'v7unix/usr/src/cmd/standalone'
    for src,dst in [('SYS.c','SYS.c'),('prf.c','fullprf.c'),('saio.h','saio.h')]:files['usr/src/boot/'+dst]=original/src
    files['usr/src/boot/l3.c']=ROOT/'v7z8000/usr/src/libc/gen/l3.c'
    def stage(target, text):
        path=WORK/'staged'/target;path.parent.mkdir(parents=True,exist_ok=True);path.write_text(text);files[target]=path
    stage('usr/src/sys/makefile',kmake);stage('usr/src/boot/makefile',bmake)
    stage('usr/src/boot/syswrap.c','#include <sys/param.h>\nstatic ino_t dlook();\n#include "SYS.c"\n')
    stage('tmp/smoke.c','''#include <stdio.h>
main() { double x; long y; x=1.5; y=100000L;
if(x+2.5!=4.0||y*3L!=300000L)return 1;
puts("NATIVE SYSTEM BOOT PASS");return 0; }
''')
    stage('usr/src/cpp/makefile', '''all: cpp
cpp.b: cpp.c
	/bin/cc -O -Dunix=1 -c cpp.c
y.tab.c: cpy.y yylex.c
	/bin/yacc cpy.y
y.tab.b: y.tab.c yylex.c
	/bin/cc -O -Dunix=1 -c y.tab.c
cpp: cpp.b y.tab.b
	/bin/cc -i cpp.b y.tab.b -o cpp
install: cpp
	/bin/cp cpp /lib/cpp
	/bin/cc -O -i cppprobe.c -o cppprobe
	./cppprobe
''')
    steps=[]
    def step(name,directory,command):
        plan='tmp/p%03d'%len(steps);stage(plan,'0 - '+command+'\n')
        steps.append(dict(name=name,directory=directory,plan='/'+plan))
    step('cpp','/usr/src/cpp','/bin/make install')
    plan_cpp=WORK/'staged/tmp/p000'
    plan_cpp.write_text(plan_cpp.read_text()+'1 - /bin/cc -E -DCPPFAIL cppprobe.c\n')
    step('pack','/usr/src/sys','/bin/make pack')
    for obj in objects:step(obj.replace('/','-'),'/usr/src/sys','/bin/make '+obj)
    step('kernel','/usr/src/sys','/bin/make all')
    step('boot','/usr/src/boot','/bin/make all')
    step('install','/usr/src/boot','/bin/make install')
    stage('tmp/bootplan','0 - /bin/cc -O -i smoke.c -o smoke\n0 - /tmp/smoke\n0 - /bin/ls /unix /boot /fpe\n')
    (WORK/'steps.json').write_text(json.dumps(steps,indent=2)+'\n')
    (WORK/'results.json').write_text('[]\n')
    for name in ('artifacts.json','boot.json'):(WORK/name).unlink(missing_ok=True)
    (WORK/'selection.json').write_text(json.dumps(selection,indent=2)+'\n')
    (WORK/'seed.json').write_text(json.dumps({n:hashlib.sha256(p.read_bytes()).hexdigest() for n,p in files.items()},indent=2)+'\n')
    image(files,WORK/'hd.img',blocks=40000,inodes=4096,modes=modes)


def run(name,directory,plan,boot=False,commands=None):
    path=WORK/(name+'.log');start=time.monotonic()
    initial=f'runner {plan} {directory}\n'
    if commands is not None:
        initial="cat > /tmp/checkplan <<'END'\n"+'\n'.join(commands)+'\nEND\nrunner /tmp/checkplan '+directory+'\n'
    input_file=WORK/(name+'.input')
    input_file.write_text(initial)
    command=[str(BUILD/'test_driver'),'-c','2000000000000','-d',str(WORK/'hd.img'),'-o',str(WORK/'next.img'),
             '-j',str(input_file),'-w','NATIVE CC DONE','-I','exit\\n','-x','NATIVE CC DONE']
    if boot:command += ['-b',str(WORK/'artifacts/unix.rom')]
    with path.open('wb') as log:
        result=subprocess.run(command,cwd=BUILD,stdout=log,stderr=subprocess.STDOUT,timeout=3600)
    data=path.read_bytes()
    if result.returncode or b'NATIVE CC PASS\r\n' not in data or b'panic:' in data:
        raise SystemExit('FAILED '+name+': '+str(path))
    (WORK/'next.img').replace(WORK/'hd.img')
    return dict(name=name,seconds=round(time.monotonic()-start,2))


def verify():
    fs=Filesystem(WORK/'hd.img');artifacts=WORK/'artifacts';artifacts.mkdir(exist_ok=True)
    paths={'unix':'/unix','boot':'/boot','fpe.bin':'/fpe','unix.rom':'/usr/src/boot/unix.rom',
           'handler.sout':'/usr/src/sys/handler.sout','kernel.bin':'/usr/src/sys/kernel.bin',
           'rom.bin':'/usr/src/sys/rom.bin','block.bin':'/usr/src/boot/block.bin'}
    report={}
    for name,path in paths.items():
        data=fs.read(path);(artifacts/name).write_bytes(data)
        report[name]=dict(bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
    # Independent cross-build comparisons, not inputs to the native build.
    for name in ('handler.sout','kernel.bin','rom.bin','fpe.bin'):
        reference=BUILD/name
        equal=(artifacts/name).read_bytes()==reference.read_bytes()
        report[name]['matches_cross_build']=equal
        if not equal:raise SystemExit('Native/cross mismatch: '+name)
    selection=json.loads((WORK/'selection.json').read_text())
    _,_,objects=recipes(selection)
    compared=[]
    for obj in objects:
        native=fs.read('/usr/src/sys/'+obj)
        cross=(BUILD/Path(obj).with_suffix('.so')).read_bytes()
        if native!=cross:raise SystemExit('Native/cross object mismatch: '+obj)
        compared.append(obj)
    report['kernel_objects_compared']=compared
    # Optional independent standalone reference, generated by build_rom.py.
    reference=WORK/'cross-boot'
    for name,cross in [('boot','boot'),('unix.rom','roms/z8001unix/unix.rom'),
                       ('block.bin','block.bin'),('unix','unix')]:
        if (reference/cross).exists():
            equal=(artifacts/name).read_bytes()==(reference/cross).read_bytes()
            report[name]['matches_cross_build']=equal
            if not equal:raise SystemExit('Native/cross boot mismatch: '+name)
    block=(artifacts/'block.bin').read_bytes();disk=(WORK/'hd.img').read_bytes()
    assert disk[:256]==block[:256] and len(block)==512
    count=int.from_bytes(disk[256:258],'big')
    sectors=[int.from_bytes(disk[258+i*4:262+i*4],'big') for i in range(count)]
    boot=b''.join(disk[n*512:(n+1)*512] for n in sectors)[:report['boot']['bytes']]
    assert boot==(artifacts/'boot').read_bytes()
    (WORK/'artifacts.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS native artifacts and installed boot-sector list',flush=True)
    commands=[
        '0 - /bin/cc -O -i smoke.c -o smoke', '0 - /tmp/smoke',
        '0 - /bin/ls /unix /boot /fpe',
        '1 - /usr/src/sys/pack kernel /tmp/smoke.c /usr/src/sys/kernel.bin /tmp/rejected',
        '0 - /bin/check absent rejected',
        '1 - /usr/src/sys/pack pad /usr/src/boot/rom.bin /tmp/padbad 1 255',
        '1 - /usr/src/sys/pack install /tmp/smoke.c /boot /dev/hd0',
        '1 - /usr/src/sys/pack install /usr/src/boot/block.bin /boot /tmp/smoke.c',
        '0 - /usr/src/sys/pack install /usr/src/boot/block.bin /boot /dev/hd0',
    ]
    record=run('disk-boot','/tmp','/tmp/bootplan',boot=True,commands=commands)
    if b'NATIVE SYSTEM BOOT PASS\r\n' not in (WORK/'disk-boot.log').read_bytes():raise SystemExit('boot smoke missing')
    assert (WORK/'hd.img').read_bytes()[:512]==disk[:512], 'boot sector changed during repeat/negative tests'
    record['commands']=len(commands)
    (WORK/'boot.json').write_text(json.dumps(record,indent=2)+'\n')
    print('PASS',record,flush=True)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--setup',action='store_true');p.add_argument('--limit',type=int);p.add_argument('--verify',action='store_true')
    a=p.parse_args()
    if a.setup:setup()
    if a.verify:verify();return
    results=json.loads((WORK/'results.json').read_text());steps=[s for s in json.loads((WORK/'steps.json').read_text()) if s['name'] not in {r['name'] for r in results}]
    if a.limit is not None:steps=steps[:a.limit]
    for s in steps:
        print('START',s['name'],flush=True);record=run(s['name'],s['directory'],s['plan']);results.append(record)
        (WORK/'results.json').write_text(json.dumps(results,indent=2)+'\n');print('PASS',record,flush=True)
    if len(results)==len(json.loads((WORK/'steps.json').read_text())):verify()


if __name__=='__main__':
    WORK.mkdir(parents=True,exist_ok=True)
    with (WORK/'run.lock').open('w') as lock:
        try:fcntl.flock(lock,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError:raise SystemExit('Native system runner already active')
        main()
