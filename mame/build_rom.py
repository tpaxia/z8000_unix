#!/usr/bin/env python3
"""Build board firmware, a disk boot block, and the V7 standalone /boot loader."""
import argparse
import importlib.util
import struct
import subprocess
import sys
from pathlib import Path
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
PCC = ROOT/'PCC-z8000/z8000'
spec = importlib.util.spec_from_file_location('build_helpers', ROOT/'tools/native-binutils/build.py')
helpers = importlib.util.module_from_spec(spec)
spec.loader.exec_module(helpers)
p = argparse.ArgumentParser(description=__doc__)
p.add_argument('--kernel-build', type=Path, default=ROOT/'v7z8000/usr/sys/build')
p.add_argument('--output', type=Path, default=ROOT/'tests/build/z8001unix')
a = p.parse_args()
a.output = a.output.resolve()
a.output.mkdir(parents=True, exist_ok=True)
AS = ROOT/'tests/build/asz8k-host/asz8k'
LD = ROOT/'tests/build/ldz8-host/ldz8'
subprocess.run(['make','-C',str(ROOT/'tools/asz8k')],check=True)
subprocess.run(['make','-C',str(ROOT/'tools/ldz8')],check=True)
(a.output/'asz8k.pd').write_bytes((ROOT/'tools/asz8k/src/asz8k.pd').read_bytes())

def assemble(name, source, address=0):
    (a.output/(name+'.s')).write_text(source)
    for cmd in [[str(AS),'-zgs','-o',name+'.so',name+'.s'],
                [str(LD),'-z','-b','-C',str(address>>16),'-T',str(address&0xffff),
                 '-o',name+'.bin',name+'.so']]:
        subprocess.run(cmd,cwd=a.output,check=True)
    return (a.output/(name+'.bin')).read_bytes()

handoff = (ROOT/'v7z8000/usr/sys/machine/emurom.s').read_text().split('initboot:',1)[1]
handoff = handoff.replace('ROM data', 'RAM').replace('ROM offset', 'RAM offset')
rom = assemble('rom',(ROOT/'mame/boot/rom.s').read_text()+'\n.org 0x200\n'+handoff)
if len(rom)>2048: raise SystemExit('ROM exceeds 2 KiB')
romdir=a.output/'roms/z8001unix'
romdir.mkdir(parents=True,exist_ok=True)
(romdir/'unix.rom').write_bytes(rom.ljust(2048,b'\xff'))
assemble('block',(ROOT/'mame/boot/block.s').read_text(),0x3fe00)
standalone=ROOT/'v7unix/usr/src/cmd/standalone'
# Device-independent standalone printf, unchanged; console tail is PDP-11 specific.
(a.output/'prf.c').write_text((standalone/'prf.c').read_text().split('struct\tdevice')[0])
(a.output/'SYS.c').write_text('#include <sys/param.h>\nstatic ino_t dlook();\n#include "'+str(standalone/'SYS.c')+'"\n')
objects=[]
for name,source,flags in [
    ('boot', ROOT/'mame/boot/boot.c', []), ('SYS',a.output/'SYS.c',[]),
    ('prf',a.output/'prf.c',[]),
    ('l3',ROOT/'v7z8000/usr/src/libc/gen/l3.c',['-Dinterdata'])]:
    obj=a.output/(name+'.b')
    helpers.compile_c(source,obj,['-I'+str(standalone),*flags])
    objects.append(obj)
(a.output/'start.az8').write_bytes((ROOT/'mame/boot/start.az8').read_bytes())
subprocess.run([str(PCC/'az8/az8'),'-o','start.b','start.az8'],cwd=a.output,check=True)
subprocess.run([str(PCC/'ldz8'),'-x',str(a.output/'start.b'),*map(str,objects),str(ROOT/'tools/libv7.a'),'-o',str(a.output/'boot')],check=True)
h=struct.unpack('>8H',(a.output/'boot').read_bytes()[:16])
if h[0]!=0o407 or sum(h[1:4])>0xe000: raise SystemExit('standalone loader too large')
kernel=bytearray((a.kernel_build/'handler.bout').read_bytes())
hk=list(struct.unpack('>8H',kernel[:16]))
vectors=(a.kernel_build/'kernel.bin').read_bytes()
if hk[0]!=0o411 or len(vectors)>512: raise SystemExit('invalid kernel layout')
hk[4]=0;hk[5]=0x1f0;hk[7]=1
kernel[:16]=struct.pack('>8H',*hk)
kernel[16:528]=vectors.ljust(512,b'\0')
(a.output/'unix').write_bytes(kernel[:16+hk[1]+hk[2]])
(a.output/'fpe.image').write_bytes((a.kernel_build/'fpe.bin').read_bytes())
print(f'ROM {len(rom)}/2048 bytes (no kernel payload); /boot text/data/bss {h[1:4]}')
