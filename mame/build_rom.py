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
p.add_argument('--machine', choices=['z8001unix','z8002unix'], default='z8001unix')
p.add_argument('--kernel-build', type=Path)
p.add_argument('--output', type=Path)
p.add_argument('--libc', type=Path, default=ROOT/'tests/build/sout-cc/libc.a',
               help='standalone runtime archive (use the native archive for native/cross comparisons)')
a = p.parse_args()
if a.kernel_build is None:
    a.kernel_build = ROOT/('tests/build/z8002-mmu' if a.machine=='z8002unix' else 'v7z8000/usr/sys/build')
selection=(a.kernel_build/'kernel-selection.txt').read_text().splitlines()[0]
if (selection=='z8002-mmu') != (a.machine=='z8002unix'):
    raise SystemExit('kernel build does not match the selected CPU')
if a.output is None: a.output=ROOT/'tests/build'/a.machine
a.output = a.output.resolve()
a.output.mkdir(parents=True, exist_ok=True)
AS = ROOT/'tests/build/asz8k-host/asz8k'
LD = ROOT/'tests/build/ldz8-host/ldz8'
subprocess.run(['make','-C',str(ROOT/'tools/asz8k')],check=True)
subprocess.run(['make','-C',str(ROOT/'tools/ldz8')],check=True)
(a.output/'asz8k.pd').write_bytes((ROOT/'tools/asz8k/src/asz8k.pd').read_bytes())

def assemble(name, source, address=0):
    (a.output/(name+'.s')).write_text(source)
    for cmd in [[str(AS),'-zg' if a.machine=='z8002unix' else '-zgs','-o',name+'.so',name+'.s'],
                [str(LD),'-z','-b','-C',str(address>>16),'-T',str(address&0xffff),
                 '-o',name+'.bin',name+'.so']]:
        subprocess.run(cmd,cwd=a.output,check=True)
    return (a.output/(name+'.bin')).read_bytes()

if a.machine=='z8002unix':
    rom=assemble('rom',(ROOT/'mame/boot/rom2.s').read_text())
else:
    handoff = (ROOT/'v7z8000/usr/sys/machine/emurom.s').read_text().split('initboot:',1)[1]
    handoff = handoff.replace('ROM data', 'RAM').replace('ROM offset', 'RAM offset')
    rom = assemble('rom',(ROOT/'mame/boot/rom.s').read_text()+'\n.org 0x200\n'+handoff)
if len(rom)>2048: raise SystemExit('ROM exceeds 2 KiB')
romdir=a.output/'roms'/a.machine
romdir.mkdir(parents=True,exist_ok=True)
(romdir/'unix.rom').write_bytes(rom.ljust(2048,b'\xff'))
assemble('block',(ROOT/('mame/boot/block2.s' if a.machine=='z8002unix' else 'mame/boot/block.s')).read_text(),0xfe00 if a.machine=='z8002unix' else 0x3fe00)
standalone=ROOT/'v7unix/usr/src/cmd/standalone'
# Device-independent standalone printf, unchanged; console tail is PDP-11 specific.
(a.output/'prf.c').write_text((standalone/'prf.c').read_text().split('struct\tdevice')[0])
(a.output/'SYS.c').write_text('#include <sys/param.h>\nstatic ino_t dlook();\n#include "'+str(standalone/'SYS.c')+'"\n')
objects=[]
for name,source,flags in [
    ('boot', ROOT/'mame/boot/boot.c', ['-DZ8002_MMU'] if a.machine=='z8002unix' else []), ('SYS',a.output/'SYS.c',[]),
    ('prf',a.output/'prf.c',[]),
    ('l3',ROOT/'v7z8000/usr/src/libc/gen/l3.c',['-Dinterdata'])]:
    obj=a.output/(name+'.so')
    helpers.compile_c(source,obj,['-I'+str(standalone),*flags],sout=True,compact=True)
    objects.append(obj)
(a.output/'start.az8').write_bytes((ROOT/('mame/boot/start2.az8' if a.machine=='z8002unix' else 'mame/boot/start.az8')).read_bytes())
subprocess.run([str(AS),'-zc','-o','start.so','start.az8'],cwd=a.output,check=True)
subprocess.run([str(LD),'-z','-s',str(a.output/'start.so'),*map(str,objects),
                str(a.libc.resolve()),'-o',str(a.output/'boot')],check=True)
boot=(a.output/'boot').read_bytes()
h=struct.unpack_from('>3H',boot,28)
if struct.unpack_from('>H',boot)[0]!=0xe707 or sum(h)>0xe000: raise SystemExit('standalone loader too large')
kernel=bytearray((a.kernel_build/'handler.sout').read_bytes())
hk=struct.unpack_from('>3H',kernel,28)
vectors=(a.kernel_build/'kernel.bin').read_bytes()
if struct.unpack_from('>H',kernel)[0]!=0xe711 or len(vectors)>512: raise SystemExit('invalid kernel layout')
struct.pack_into('>I',kernel,14,0x1f0) # reset handoff entry convention
kernel[40:552]=vectors.ljust(512,b'\0')
(a.output/'unix').write_bytes(kernel) # retain global symbols for V7 inspection tools
(a.output/'fpe.image').write_bytes((a.kernel_build/'fpe.bin').read_bytes())
print(f'ROM {len(rom)}/2048 bytes (no kernel payload); s.out /boot text/data/bss {h}')
