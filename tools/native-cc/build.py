#!/usr/bin/env python3
"""Build a bootable disk containing the native two-pass C toolchain."""
from pathlib import Path
import importlib.util
import json
import shutil
import struct
import sys
sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parents[2]
PCC = ROOT / 'PCC-z8000/z8000'
WORK = ROOT / 'tests/build/native-cc'
PASSES = ROOT / 'tests/build/native-pcc'
BINUTILS = ROOT / 'tests/build/native-binutils'


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


helpers = module('binutils_build', ROOT / 'tools/native-binutils/build.py')
run, compile_c = helpers.run, helpers.compile_c


def image(extra_files=None, destination=None, blocks=6000, inodes=512, modes=None):
    """Install target tools, V7 headers, and optional test fixtures."""
    run([sys.executable, ROOT / 'tools/export-headers.py', '--check'])
    files = {
        'bin/cc': WORK / 'cc', 'bin/az8': BINUTILS / 'az8/az8',
        'bin/ldz8': BINUTILS / 'ldz8/ldz8', 'bin/sh': ROOT / 'tools/sh',
        'etc/init': ROOT / 'tools/init', 'lib/cpp': WORK / 'cpp',
        'lib/front': PASSES / 'target-front/front',
        'lib/back': PASSES / 'target-back/back',
        'lib/oz8': WORK / 'oz8',
        'lib/crt0.b': ROOT / 'tools/libc/crt0.b',
        'lib/libc.a': ROOT / 'tools/libv7.a',
        'usr/src/hello.c': ROOT / 'tools/native-cc/hello.c',
    }
    includes = ROOT / 'v7z8000/usr/include'
    for path in includes.rglob('*'):
        if path.is_file():
            files['usr/include/' + str(path.relative_to(includes))] = path
    files.update(extra_files or {})
    tree = {'tmp': {}, 'dev': {'console': 'c--644 0 0 0 0', 'tty': 'c--644 0 0 2 0'}}
    for target, source in files.items():
        parts = target.split('/')
        node = tree
        for part in parts[:-1]:
            node = node.setdefault(part, {})
        mode = '755' if parts[0] in ('bin', 'etc') or target in (
            'lib/cpp', 'lib/front', 'lib/back', 'lib/oz8') else '644'
        if modes and target in modes:
            mode = '%03o' % (modes[target] & 0o777)
        node[parts[-1]] = '---' + mode + ' 0 0 ' + str(source)

    def directory(node):
        lines = []
        for name, value in sorted(node.items()):
            assert len(name) <= 14, name
            if isinstance(value, dict):
                lines.append(name + (' d--777' if name == 'tmp' else ' d--755') + ' 0 0')
                lines.extend(directory(value))
            else:
                lines.append(name + ' ' + value)
        return lines + ['$']

    destination = Path(destination) if destination else WORK / 'hd.img'
    proto = destination.with_suffix('.proto')
    proto.write_text('boot\n%d %d\nd--755 0 0\n' % (blocks, inodes) + '\n'.join(directory(tree)) + '\n')
    run([ROOT / 'tools/v7mkfs', destination, proto])


def build(no_compact=False):
    WORK.mkdir(parents=True, exist_ok=True)
    run([sys.executable, ROOT / 'tools/pcc-native/build.py',
         *(['--no-compact'] if no_compact else [])])
    run([sys.executable, ROOT / 'tools/native-binutils/build.py'])
    regen = module('regen', PCC / 'cz8/regen_cgram.py')
    yaccdir = WORK / 'yacc'
    yaccdir.mkdir(exist_ok=True)
    yacc = regen.build_yacc(yaccdir)
    cppsrc = ROOT / 'v7z8000/usr/src/cmd/cpp'
    shutil.copyfile(cppsrc / 'cpy.y', yaccdir / 'cpy.y')
    run([yacc, 'cpy.y'], cwd=yaccdir)
    report = {}
    for name, sources, flags in [
        ('cpp', [cppsrc / 'cpp.c', yaccdir / 'y.tab.c'], ['-I' + str(cppsrc)]),
        ('cc', [PCC / 'ccz8.c'], ['-DTWOPASS']),
        ('oz8', [PCC / 'oz8.c'], []),
    ]:
        objects = []
        for source in sources:
            obj = WORK / (source.stem + '.b')
            compile_c(source, obj, flags)
            objects.append(obj)
        run([PCC / 'ldz8', '-i', '-x', ROOT / 'tools/libc/crt0.b', *objects,
             ROOT / 'tools/libv7.a', '-o', WORK / name])
        h = struct.unpack('>8H', (WORK / name).read_bytes()[:16])
        report[name] = dict(zip(['text', 'data', 'bss'], h[1:4]))
        print(name, report[name], flush=True)
    (WORK / 'sizes.json').write_text(json.dumps(report, indent=2) + '\n')
    image()
    print('Native compiler disk:', WORK / 'hd.img', flush=True)


if __name__ == '__main__':
    if any(arg != '--no-compact' for arg in sys.argv[1:]):
        raise SystemExit('usage: build.py [--no-compact]')
    build('--no-compact' in sys.argv[1:])
