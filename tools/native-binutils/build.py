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


def compile_c(source, destination, flags=()):
    pre = run(['cpp', '-nostdinc', '-undef', '-Dz8000', '-Dz8002', '-Dunix=1',
               '-I' + str(source.parent), '-I' + str(ROOT / 'v7z8000/usr/include'), *flags, source])
    compiled = run([PCC / 'cz8/cz8'], input=pre.stdout)
    destination.with_suffix('.az8').write_bytes(compiled.stdout)
    destination.with_suffix('.log').write_bytes(pre.stderr + compiled.stderr)
    run([PCC / 'az8/az8', '-o', destination.name,
         destination.with_suffix('.az8').name], cwd=destination.parent)


def build():
    WORK.mkdir(parents=True, exist_ok=True)
    for directory in ['cz8', 'az8']:
        run(['make', '-C', PCC / directory])
    run(['make', '-C', PCC / 'test', '../ldz8'])
    run(['make', '-C', ROOT / 'tools', 'libv7.a', 'libc/crt0.b', 'v7mkfs', 'sh', 'init'])
    report = {}
    for tool, sources in [
        ('az8', [PCC / 'az8' / (n + '.c') for n in
                 'error init ins ioz8 ps rel sdi sym scan'.split()]),
        ('ldz8', [PCC / 'ldz8.c']),
    ]:
        directory = WORK / tool
        directory.mkdir(exist_ok=True)
        objects = []
        for source in sources:
            obj = directory / (source.stem + '.b')
            compile_c(source, obj)
            objects.append(obj)
        run([PCC / 'ldz8', '-i', '-x', ROOT / 'tools/libc/crt0.b', *objects,
             ROOT / 'tools/libv7.a', '-o', directory / tool])
        h = struct.unpack('>8H', (directory / tool).read_bytes()[:16])
        assert h[0] == 0o411 and h[1] < 65536 and h[2] + h[3] < 65536
        report[tool] = dict(zip(['text', 'data', 'bss'], h[1:4]))
        print(tool, report[tool], flush=True)
    (WORK / 'sizes.json').write_text(json.dumps(report, indent=2) + '\n')


if __name__ == '__main__':
    build()
