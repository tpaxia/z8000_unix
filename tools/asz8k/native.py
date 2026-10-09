#!/usr/bin/env python3
"""Build and exercise the s.out assembler inside V7 Unix."""
from pathlib import Path
import argparse
import fcntl
import csv
import hashlib
import json
import struct
import subprocess
import sys
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/native-cc'))
from build import image
from selfhost import Filesystem
from soutcheck import check as check_sout
from host import check as check_host
WORK = ROOT / 'tests/build/asz8k'
SOURCE = ROOT / 'tools/asz8k'
SYS = ROOT / 'v7z8000/usr/sys/build'


def setup(preserve=False, reuse=False):
    WORK.mkdir(parents=True, exist_ok=True)
    files = {}
    native = ROOT / 'tests/build/native-environment-sout/native'
    for p in native.rglob('*'):
        if p.is_file(): files[str(p.relative_to(native))] = p
    fs = Filesystem(ROOT / 'tests/build/userland-native-sout/hd.img')
    for name in ('sh', 'make', 'rm', 'runner'):
        p = WORK / name
        p.write_bytes(fs.read('/bin/' + name))
        files['bin/' + name] = p
    for p in (SOURCE / 'src').iterdir():
        files['usr/src/asz8k/' + p.name] = p
    for p in (SOURCE / 'tests').iterdir():
        if not p.is_file(): continue
        files['usr/src/asz8k/' + p.name] = p
    for p in (SOURCE / 'tests/objects').iterdir():
        files['usr/src/asz8k/' + p.name] = p
    badbyte=WORK/'badbyte.8kn';badbyte.write_text('__data .sect\n .global _abs\n .byte _abs\n .end\n')
    files['usr/src/asz8k/badbyte.8kn']=badbyte
    for name in ('probe.8kn','probe.az8'):
        path=WORK/name
        path.write_text((SOURCE/'tests/objects'/name).read_text().replace('.byte _abs','.byte 9').replace('.long _abs','.word 0,_abs'))
        files['usr/src/asz8k/'+name]=path
    files['usr/src/asz8k/fpe.8kn'] = ROOT / 'v7z8000/usr/sys/fpe/fpe.z8k'
    if preserve:
        saved = Filesystem(WORK / 'hd.img')
        def unchanged(p):
            try: return saved.read('/usr/src/asz8k/' + p.name) == p.read_bytes()
            except KeyError: return False
        headers_same = all(unchanged(p) for p in (SOURCE / 'src').glob('*.h'))
        headers_same = headers_same and unchanged(SOURCE / 'src/makefile')
        for tool in native.rglob('*'):
            if tool.is_file():
                name = str(tool.relative_to(native))
                # Some seed commands come from the completed userland image.
                try: same = saved.read('/' + name) == files[name].read_bytes()
                except KeyError: same = False
                headers_same = headers_same and same
        sources = list((SOURCE / 'src').glob('*.c'))
        names = [p.stem + '.b' for p in sources if headers_same and unchanged(p)]
        if headers_same and all(unchanged(p) for p in sources): names.append('asz8k')
        for name in names:
            try: data = saved.read('/usr/src/asz8k/' + name)
            except KeyError: continue
            p = WORK / ('saved-' + name)
            p.write_bytes(data)
            files['usr/src/asz8k/' + name] = p
    stress = WORK / 'overflow.8kn'
    stress.write_text('__text .sect\n' + ''.join('s%04d .equ %d\n' % (i, i) for i in range(4000)) + ' .end\n')
    files['usr/src/asz8k/overflow.8kn'] = stress
    steps = []
    for p in sorted((SOURCE / 'src').glob('*.c')):
        steps.append((p.stem, '/bin/make ' + p.stem + '.b'))
    steps += [('link', '/bin/make')]
    if reuse:
        verified=Filesystem(ROOT/'tests/build/native-environment-sout/hd.img')
        for source in (SOURCE/'src').glob('*'):
            if source.suffix in ('.c','.h','.pd'):
                assert verified.read('/usr/src/asz8k/'+source.name)==source.read_bytes(),source
        target=WORK/'verified-asz8k';target.write_bytes(verified.read('/bin/asz8k'))
        files['usr/src/asz8k/asz8k']=target
        steps=[]
    steps += [(p.stem, './asz8k ' + ('-s ' if p.suffix == '.8ks' else '') + '-l ' + p.name) for p in sorted((SOURCE / 'tests').glob('*.8k*'))]
    steps.append(('fpe', './asz8k -l fpe.8kn'))
    steps += [('xref-fpe', './asz8k -x -o fpex.so fpe.8kn'),
              ('xref-macro', './asz8k -x -o macrox.so macro.8kn')]
    steps += [('format-build', '/bin/cc -I. -i fmtcheck.c soutfmt.c -o fmtcheck'),
              ('format-run', './fmtcheck'),
              ('sout-seg', './asz8k -z -s soutseg.8ks'),
              ('sout-non', './asz8k -z soutnon.8kn'),
              ('sout-oracle', './asz8k -z -s seg.8ks'),
              ('sout-bad-short', './asz8k -z -s badshort.8ks'),
              ('sout-bad-entry', './asz8k -z badentry.8kn'),
              ('sout-bad-formats', './asz8k -az soutnon.8kn'),
              ('sout-bad-byte', './asz8k badbyte.8kn')]
    steps += [('output', './asz8k -s -o custom.so seg.8ks'),
              ('bad-mode', './asz8k seg.8ks'),
              ('bad-option', './asz8k -o'),
              ('overflow', './asz8k overflow.8kn')]
    steps += [
        ('probe', './asz8k -o probe.b probe.8kn'),
        ('abs', './asz8k -o abs.b abs.8kn'),
        ('bounds', './asz8k -o bounds.b bounds.8kn'),
        ('reference-probe', './asz8k -c -o refprobe.b probe.az8'),
        ('reference-abs', './asz8k -c -o refabs.b abs.az8'),
        ('check', '/bin/cc -O -c check.c'),
        ('bad-legacy-as', './asz8k -a seg.8ks'),
    ]
    for mode, flags in [('split', '-i -s'), ('combined', '-s')]:
        steps += [
            (mode+'-link', '/bin/cc '+flags+' check.b probe.b abs.b -o '+mode),
            (mode+'-run', './'+mode),
            (mode+'-reference', '/bin/cc '+flags+' check.b refprobe.b refabs.b -o ref'+mode),
            (mode+'-reference-run', './ref'+mode),
        ]
    steps += [('partial', '/bin/ldz8 -r probe.b abs.b -o partial.b'),
              ('partial-link', '/bin/cc -i -s check.b partial.b -o partial'),
              ('partial-run', './partial')]
    steps += [('sout-'+p.stem, './asz8k '+p.name) for p in sorted((SOURCE / 'tests/objects').glob('bad*.8kn'))]
    for i, (name, command) in enumerate(steps):
        p = WORK / ('p%03d' % i)
        p.write_text(('1' if name.startswith(('bad', 'sout-bad')) or name in ('bad-mode', 'bad-option', 'overflow') else '0') + ' - ' + command + '\n')
        files['tmp/' + p.name] = p
    (WORK / 'steps.json').write_text(json.dumps(steps))
    (WORK / 'results.json').write_text('[]')
    image(files, WORK / 'hd.img', blocks=12000, inodes=1024, modes={'usr/src/asz8k/asz8k': 0o755})


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup', action='store_true')
    parser.add_argument('--refresh', action='store_true')
    parser.add_argument('--reuse-tool',action='store_true',help='test the source-verified native environment assembler')
    args = parser.parse_args()
    if args.setup or args.reuse_tool: setup(reuse=args.reuse_tool)
    elif args.refresh: setup(True)
    steps = json.loads((WORK / 'steps.json').read_text())
    records = json.loads((WORK / 'results.json').read_text())
    for i in range(len(records), len(steps)):
        name, command = steps[i]
        print('START', name, flush=True)
        logpath = WORK / (name + '.log')
        with logpath.open('wb') as log:
            result = subprocess.run([str(SYS / 'test_driver'), '-c', '200000000000',
                '-P', str(WORK / (name + '.tsv')),
                '-d', str(WORK / 'hd.img'), '-o', str(WORK / 'next.img'),
                '-i', 'runner /tmp/p%03d /usr/src/asz8k\\n' % i,
                '-w', 'NATIVE CC DONE', '-I', 'exit\\n', '-x', 'NATIVE CC DONE'],
                cwd=SYS, stdout=log, stderr=subprocess.STDOUT, timeout=1800)
        if result.returncode or b'NATIVE CC PASS\r\n' not in logpath.read_bytes():
            raise SystemExit('FAILED: ' + str(logpath))
        (WORK / 'next.img').replace(WORK / 'hd.img')
        records.append(name)
        (WORK / 'results.json').write_text(json.dumps(records))
        print('PASS', name, flush=True)
    fs = Filesystem(WORK / 'hd.img')
    binary = fs.read('/usr/src/asz8k/asz8k')
    (WORK / 'asz8k').write_bytes(binary)
    sizes = dict(zip(('text', 'data', 'bss'), struct.unpack_from('>3H',binary,28)))
    (WORK / 'sizes.json').write_text(json.dumps(sizes, indent=2) + '\n')
    for name in ['fpe.so', 'fpe.lst', 'custom.so'] + [p.stem + ext for p in (SOURCE / 'tests').glob('*.8k*') if not p.name.startswith('bad') for ext in ('.so', '.lst')]:
        (WORK / name).write_bytes(fs.read('/usr/src/asz8k/' + name))
    assert (WORK/'custom.so').read_bytes() == (WORK/'seg.so').read_bytes()
    assert b'Assembler virtual storage exhausted' in (WORK/'overflow.log').read_bytes()
    for mode in ('split', 'combined'):
        data = fs.read('/usr/src/asz8k/' + mode)
        assert data == fs.read('/usr/src/asz8k/ref' + mode), mode
        (WORK / mode).write_bytes(data)
    for name in ('probe.b', 'abs.b', 'partial.b'):
        (WORK / name).write_bytes(fs.read('/usr/src/asz8k/' + name))
    assert fs.read('/usr/src/asz8k/partial') == fs.read('/usr/src/asz8k/split')
    print('PASS: native s.out linking/execution and assembly dialect comparison, combined/split and ld -r')
    for name in ('seg.so', 'soutseg.so', 'soutnon.so'):
        (WORK / name).write_bytes(fs.read('/usr/src/asz8k/' + name))
    (WORK / 'nativefmt.bin').write_bytes(fs.read('/usr/src/asz8k/format.bin'))
    check_sout(WORK)
    check_host(fs)
    memory = {}
    for profile in WORK.glob('*.tsv'):
        with profile.open() as stream:
            for row in csv.DictReader(stream, delimiter='\t'):
                if row['path'] == './asz8k' and int(row['maximum_break']):
                    memory[profile.stem] = {key: int(row[key]) for key in
                        ('initial_sp', 'minimum_sp', 'maximum_break')}
                    memory[profile.stem]['gap'] = int(row['minimum_sp']) - int(row['maximum_break'])
    (WORK / 'memory.json').write_text(json.dumps(memory, indent=2) + '\n')
    print('Native assembler:', sizes)


if __name__ == '__main__':
    WORK.mkdir(parents=True, exist_ok=True)
    with (WORK / 'run.lock').open('w') as lock:
        try: fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError: raise SystemExit('An assembler trial is already running')
        main()
