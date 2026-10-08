#!/usr/bin/env python3
"""Install the driver into an isolated MAME checkout and build a focused binary."""
import argparse
from pathlib import Path
import shutil
import subprocess
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('source',type=Path,help='isolated MAME source tree (will be modified)')
p.add_argument('-j',type=int,default=8)
a=p.parse_args()
root=Path(__file__).resolve().parent
shutil.copyfile(root/'z8001unix.cpp',a.source/'src/mame/zilog/z8001unix.cpp')
listing=a.source/'src/mame/mame.lst'
text=listing.read_text()
if '\nz8001unix\n' not in text:
    listing.write_text(text+'\n@source:zilog/z8001unix.cpp\nz8001unix\n')
subprocess.run(['make','SUBTARGET=z8001unix','SOURCES=src/mame/zilog/z8001unix.cpp',
                'REGENIE=1',f'-j{a.j}'],cwd=a.source,check=True)
