#!/usr/bin/env python3
"""Build the complete command inventory, generate its data, and test the image."""
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]

def run(script, *args):
    subprocess.run([sys.executable, str(ROOT / script), *args], cwd=ROOT, check=True)

run('tools/pcc-native/build.py')
run('tools/native-binutils/build.py')
run('tools/native-cc/build.py')
run('tools/userland/build.py', 'lex', 'cp', 'make', 'yacc')
run('tools/userland/generate.py')
run('tools/userland/build.py')
run('tools/userland/test.py', '--setup')
run('tools/userland/inventory.py')
