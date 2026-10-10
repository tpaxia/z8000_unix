#!/usr/bin/env python3
"""Install the driver into an isolated MAME checkout and build a focused binary."""
import argparse
from pathlib import Path
import shutil
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('source',type=Path,help='isolated MAME source tree (will be modified)')
p.add_argument('--machine', choices=['z8001unix','z8002unix'], default='z8001unix')
p.add_argument('-j',type=int,default=8)
a=p.parse_args()
root=Path(__file__).resolve().parent
shutil.copyfile(root/(a.machine+'.cpp'),a.source/('src/mame/zilog/'+a.machine+'.cpp'))
listing=a.source/'src/mame/mame.lst'
text=listing.read_text()
if '\n'+a.machine+'\n' not in text:
    listing.write_text(text+'\n@source:zilog/'+a.machine+'.cpp\n'+a.machine+'\n')
subprocess.run(['make','SUBTARGET='+a.machine,'SOURCES=src/mame/zilog/'+a.machine+'.cpp',
                'REGENIE=1',f'-j{a.j}'],cwd=a.source,check=True)
