#!/usr/bin/env python3
"""Install the driver into an isolated MAME checkout and build a focused binary."""
import argparse
from pathlib import Path
import shutil
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('source',type=Path,help='isolated MAME source tree (will be modified)')
p.add_argument('--machine', choices=['unixv7_demo','z8001unix','z8002unix'], default='unixv7_demo',
               help='build both machines (default), or a focused executable')
p.add_argument('-j',type=int,default=8)
a=p.parse_args()
root=Path(__file__).resolve().parent
target=a.source/'src/mame/homebrew'
target.mkdir(parents=True, exist_ok=True)
for source in (root/'unixv7').iterdir():
    if source.suffix in ('.cpp', '.h'):
        shutil.copyfile(source,target/source.name)
listing=a.source/'src/mame/mame.lst'
text=listing.read_text()
for machine,cpu in [('z8001unix','z8001'),('z8002unix','z8002')]:
    old='@source:zilog/'+machine+'.cpp'
    new='@source:homebrew/unixv7_'+cpu+'.cpp'
    text=text.replace(old,new)
    if '\n'+machine+'\n' not in text:
        text+='\n'+new+'\n'+machine+'\n'
listing.write_text(text)
sources=['src/mame/homebrew/unixv7.cpp']
cpus=['z8001','z8002'] if a.machine=='unixv7_demo' else [a.machine[:5]]
sources += ['src/mame/homebrew/unixv7_'+cpu+'.cpp' for cpu in cpus]
subprocess.run(['make','SUBTARGET='+a.machine,'SOURCES='+','.join(sources),
                'REGENIE=1',f'-j{a.j}'],cwd=a.source,check=True)
