#!/usr/bin/env python3
"""Cross-build the Unix-native assembler and linker with separate I/D."""
from pathlib import Path
import json
import struct
import subprocess

ROOT = Path(__file__).resolve().parents[2]
PCC = ROOT / 'PCC-z8000/z8000'
WORK = ROOT / 'tests/build/native-binutils'


def run(args, **kwargs):
    result = subprocess.run(list(map(str, args)), capture_output=True, **kwargs)
    if result.returncode:
        print((result.stdout + result.stderr).decode(errors='replace'))
    result.check_returncode()
    return result


def compile_c(source, destination, flags=(), sout=True, compact=False):
    pre = run(['cpp', '-nostdinc', '-undef', '-Dz8000', '-Dz8002', '-Dunix=1',
               '-I' + str(source.parent), '-I' + str(ROOT / 'v7z8000/usr/include'), *flags, source])
    compiled = run([PCC / 'cz8/cz8'], input=pre.stdout)
    if compact:
        compiled = run([PCC/'oz8'], input=compiled.stdout)
    destination.with_suffix('.az8').write_bytes(compiled.stdout)
    destination.with_suffix('.log').write_bytes(pre.stderr + compiled.stderr)
    if not sout:
        raise ValueError('obsolete object format: use s.out')
    assembler = ROOT / 'tests/build/asz8k-host/asz8k'
    (destination.parent/'asz8k.pd').write_bytes((ROOT/'tools/asz8k/src/asz8k.pd').read_bytes())
    assembly=destination.with_suffix('.az8')
    if len(assembly.name)>14:
        assembly=destination.parent/'input.az8';assembly.write_bytes(compiled.stdout)
    run([assembler, '-c', '-o', destination.name,
         assembly.name], cwd=destination.parent)


def build():
    """Publish target copies built by the common s.out bootstrap."""
    import importlib.util
    import shutil
    path = ROOT/'tools/sout-cc/build.py'
    spec = importlib.util.spec_from_file_location('sout_seed', path)
    seed = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(seed)
    seed.library(); seed.seeds()
    report = {}
    for name in ('asz8k','ldz8'):
        directory = WORK/name; directory.mkdir(parents=True,exist_ok=True)
        target = directory/name
        shutil.copyfile(seed.WORK/'seed'/(name+'.out'),target)
        report[name] = dict(zip(('text','data','bss'),struct.unpack_from('>3H',target.read_bytes(),28)))
    (WORK/'sizes.json').write_text(json.dumps(report,indent=2)+'\n')


if __name__ == '__main__':
    build()
