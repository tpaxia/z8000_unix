#!/usr/bin/env python3
"""Independently assemble native trial inputs with the full host asz8k."""
from pathlib import Path
import json
import shlex
import shutil
import struct
import subprocess
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / 'tools/asz8k'
WORK = ROOT / 'tests/build/asz8k-host'
NATIVE = ROOT / 'tests/build/asz8k'
sys.path.insert(0, str(ROOT / 'tools/native-cc'))
from selfhost import Filesystem


def check(fs=None):
    subprocess.run(['make', '-C', str(SOURCE)], check=True)
    if fs is None:
        fs = Filesystem(NATIVE / 'hd.img')
    # Refuse to compare against native binaries tested with stale sources.
    for p in (SOURCE / 'src').iterdir():
        assert fs.read('/usr/src/asz8k/' + p.name) == p.read_bytes(), ('refresh native sources', p)
    for directory in (SOURCE / 'tests', SOURCE / 'tests/aout'):
        for p in directory.iterdir():
            if p.is_file():
                expected=p.read_bytes()
                if p.name in ('probe.8kn','probe.az8'):expected=expected.replace(b'.byte _abs',b'.byte 9').replace(b'.long _abs',b'.word 0,_abs')
                assert fs.read('/usr/src/asz8k/' + p.name)==expected,('refresh native tests',p)
                (WORK/p.name).write_bytes(expected)
    for p in (SOURCE / 'src').glob('*.pd'):
        shutil.copy2(p, WORK / p.name)
    for name in ('fpe.8kn','overflow.8kn','badbyte.8kn'):
        (WORK / name).write_bytes(fs.read('/usr/src/asz8k/' + name))
    commands = json.loads((NATIVE / 'steps.json').read_text())
    complete = json.loads((NATIVE / 'results.json').read_text())
    assert complete == [name for name, _ in commands], 'native trial must finish first'
    records, outputs, listings = [], set(), set()
    for name, command in commands:
        if not command.startswith('./asz8k '):
            continue
        argv = shlex.split(command)[1:]
        result = subprocess.run([str(WORK / 'asz8k')] + argv,
                                cwd=WORK, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, timeout=30)
        failed = name.startswith(('bad', 'sout-bad')) or name in ('bad-mode', 'bad-option', 'overflow')
        assert result.returncode == int(failed), (name, result.returncode, result.stdout.decode(errors='replace'))
        (WORK / (name + '.log')).write_bytes(result.stdout)
        if not failed:
            source = Path(argv[-1])
            suffix = '.so'
            output = argv[argv.index('-o')+1] if '-o' in argv else source.stem + suffix
            data = (WORK / output).read_bytes()
            assert data == fs.read('/usr/src/asz8k/' + output), ('object differs', name, output)
            outputs.add(output)
            if '-l' in argv or '-x' in argv:
                listings.add(source.stem + '.lst')
        records.append(name)
    # A later -x invocation can replace an earlier -l listing for that source.
    for listing in listings:
        assert (WORK / listing).read_bytes() == fs.read('/usr/src/asz8k/' + listing), ('listing differs', listing)
        outputs.add(listing)
    expected = struct.pack('>8I', 0x80000000, 0xffffffff, 0x7fffffff,
                           0x80000000, 0, 0xffffffff, 0x80000000, 0xc0000000)
    expected += bytes.fromhex('ffffffffff000000')
    data=(WORK/'bounds.b').read_bytes()
    assert struct.unpack_from('>H',data)[0]==0xe707
    assert data[40:40+len(expected)]==expected
    summary = {'commands': len(records), 'identical_files': sorted(outputs)}
    (WORK / 'results.json').write_text(json.dumps(summary, indent=2) + '\n')
    print('PASS: full host/native assemblers; %d commands, %d byte-identical objects/listings' % (len(records), len(outputs)))
    return summary


if __name__ == '__main__':
    check()
