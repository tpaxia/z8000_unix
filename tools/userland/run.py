#!/usr/bin/env python3
"""Bootstrap s.out tools, then rebuild and test all userland natively."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]

def run(script, *args):
    subprocess.run([sys.executable, str(ROOT / script), *args], cwd=ROOT, check=True)

system=ROOT/'v7z8000/usr/sys'
build=system/'build'
if not (build/'CMakeCache.txt').exists():
    subprocess.run(['cmake','-S',str(system),'-B',str(build),
                    '-DCMAKE_BUILD_TYPE=Release'],check=True)
subprocess.run(['cmake','--build',str(build),'--target','kernel','test_driver'],check=True)
run('tools/native-cc/build.py')
run('tools/native-cc/test.py')
run('tools/native-cc/environment.py','--setup')
run('tools/native-cc/userland.py','--setup')
run('tools/userland/native.py','--setup')
