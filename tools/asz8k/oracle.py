#!/usr/bin/env python3
"""Check recorded Unidot objects against the original CP/M assembler."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[2]
HERE = Path(__file__).resolve().parent


def unpadded(data):
    """CP/M rounds files to 128 bytes; the object end block terminates data."""
    offset = 0
    while offset + 2 <= len(data):
        kind, size = data[offset:offset + 2]
        offset += 2 + size
        if offset > len(data): raise ValueError('Truncated Unidot block')
        if kind == 11:
            if size: raise ValueError('Malformed Unidot end block')
            return data[:offset]
    raise ValueError('Missing Unidot end block')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('cpm', type=Path, help='CPM8000 checkout with built Z8001 emulator')
    args = parser.parse_args()
    cpm = args.cpm.resolve()
    expected = json.loads((HERE / 'tests/expected.json').read_text())
    with tempfile.TemporaryDirectory(prefix='asz8k-oracle-') as tmp:
        drive = Path(tmp)
        for name in ('asz8k.z8k', 'asz8k.pd', 'xcon.z8k'):
            shutil.copyfile(cpm / 'src/cpm8k' / name, drive / name)
        for p in (HERE / 'tests').iterdir():
            if p.is_file() and p.suffix != '.json': shutil.copyfile(p, drive / p.name)
        shutil.copyfile(ROOT / 'v7z8000/usr/sys/fpe/fpe.z8k', drive / 'fpe.8kn')
        commands = ['asz8k ' + ('-s ' if name == 'seg' else '') + name +
                    ('.8ks' if name == 'seg' else '.8kn') for name in expected]
        (drive / 'TEST.SUB').write_bytes(('\r\n'.join(commands) + '\r\n').encode())
        result = subprocess.run([str(cpm / 'build/emu/cpm8k-z8001'), '-d', 'C=dir:' + tmp],
                                input=b'SUBMIT TEST\n', cwd=cpm, capture_output=True, timeout=120)
        log = ROOT / 'tests/build/asz8k/oracle.log'
        log.parent.mkdir(parents=True, exist_ok=True)
        log.write_bytes(result.stdout + result.stderr)
        result.check_returncode()
        for name, record in expected.items():
            data = unpadded((drive / (name.upper() + '.OBJ')).read_bytes())
            assert len(data) == record['bytes'], name
            assert hashlib.sha256(data).hexdigest() == record['sha256'], name
            print('PASS', name, len(data), 'bytes')


if __name__ == '__main__':
    main()
