#!/usr/bin/env python3
"""Resumable two-generation native PCC rebuild. Run build.py first."""
from pathlib import Path
import argparse
import csv
import hashlib
import json
import struct
import subprocess
import sys
import time
sys.dont_write_bytecode = True
from build import ROOT, PCC, PASSES, WORK, compile_c, image, run

HERE = ROOT / 'tests/build/selfhost'
SYS = ROOT / 'v7z8000/usr/sys/build'
SOURCES = {
    'front': 'cgram xdefs scan pftn trees optim code local comm1 frontglue'.split(),
    'back': 'reader local2 order match allo comm2 table backglue'.split(),
    'oz8': ['oz8'],
}
CASES = {'hello': ROOT / 'tools/native-cc/hello.c',
         'structret': PCC / 'test/pcc_structret.c',
         'math': PCC / 'test/pcc_math.c',
         'floating': PCC / 'test/regress/float_general.c'}


class Filesystem:
    def __init__(self, path):
        self.disk = path.read_bytes()

    def data(self, number):
        offset = ((number + 15) // 8) * 512 + ((number + 15) % 8) * 64
        inode = self.disk[offset:offset + 64]
        size = struct.unpack_from('>I', inode, 8)[0]
        addresses = [int.from_bytes(inode[12+3*i:15+3*i], 'big') for i in range(13)]
        def indirect(block, depth):
            for entry in struct.unpack('>128I', self.disk[block*512:(block+1)*512]):
                if not entry:
                    break
                if depth == 1:
                    yield entry
                else:
                    yield from indirect(entry, depth - 1)
        blocks = addresses[:10]
        for depth, threshold in [(1, 10), (2, 138), (3, 16522)]:
            if size > threshold * 512:
                blocks += list(indirect(addresses[9 + depth], depth))
        return b''.join(self.disk[b*512:(b+1)*512] for b in blocks)[:size]

    def read(self, path):
        number = 2
        for name in path.strip('/').split('/'):
            directory = self.data(number)
            entries = {directory[i+2:i+16].split(b'\0')[0].decode():
                       int.from_bytes(directory[i:i+2], 'big')
                       for i in range(0, len(directory), 16)}
            number = entries[name]
        return self.data(number)


def plans():
    result = []
    for stage in (1, 2):
        directory = '/tmp/s%d' % stage
        flags = '-B/tmp/s1/ -t012 ' if stage == 2 else ''
        for tool, sources in SOURCES.items():
            for source in sources:
                command = ('0 - /bin/cc %s-O -c -DBUG4 '
                           '-I/usr/src/pcc /usr/src/pcc/%s.c' % (flags, source))
                result.append((f's{stage}-{source}', directory, [command]))
            command = '0 - /bin/cc -i ' + ' '.join(s + '.b' for s in sources) + ' -o ' + tool
            commands = [command]
            if stage == 2:
                commands.append('0 - /bin/check same %s /tmp/s1/%s' % (tool, tool))
            result.append((f's{stage}-link-{tool}', directory, commands))
        for case in CASES:
            command = (f'0 - /bin/cc -B{directory}/ -t012 -O -i /usr/src/{case}.c -o probe')
            # hello returns 42 in the standalone suite, use our Unix hello.
            result.append((f's{stage}-test-{case}', directory,
                           [command, '0 - ' + directory + '/probe']))
    return result


def setup():
    HERE.mkdir(parents=True, exist_ok=True)
    extra = {'lib/libc.a': PASSES / 'libv7.a'}
    for name in ['runner', 'check']:
        compile_c(ROOT / 'tools/native-cc' / (name + '.c'), HERE / (name + '.b'))
        run([PCC / 'ldz8', '-x', ROOT / 'tools/libc/crt0.b', HERE / (name + '.b'),
             ROOT / 'tools/libv7.a', '-o', HERE / name])
        extra['bin/' + name] = HERE / name
    headers = ['manifest', 'macdefs', 'mac2defs', 'mfile1', 'mfile2', 'common']
    for name in headers + [s + '.c' for values in SOURCES.values() for s in values if s != 'oz8']:
        extra['usr/src/pcc/' + name] = PASSES / name
    extra['usr/src/pcc/oz8.c'] = PCC / 'oz8.c'
    for case, path in CASES.items():
        extra['usr/src/' + case + '.c'] = path
    (HERE / 'keep').write_text('')
    for stage in (1, 2):
        extra['tmp/s%d/keep' % stage] = HERE / 'keep'
    for index, (_, _, commands) in enumerate(plans()):
        path = HERE / ('p%03d' % index)
        path.write_text('\n'.join(commands) + '\n')
        extra['tmp/' + path.name] = path
    image(extra, HERE / 'hd.img', blocks=24000)
    (HERE / 'results.json').write_text('[]\n')
    for report in ['memory.json', 'convergence.json']:
        (HERE / report).unlink(missing_ok=True)


def summarize():
    binaries = {'/lib/front': PASSES / 'target-front/front',
                '/lib/back': PASSES / 'target-back/back', '/lib/oz8': WORK / 'oz8',
                '/lib/cpp': WORK / 'cpp', '/bin/cc': WORK / 'cc',
                '/bin/az8': ROOT / 'tests/build/native-binutils/az8/az8',
                '/bin/ldz8': ROOT / 'tests/build/native-binutils/ldz8/ldz8'}
    for stage in (1, 2):
        for tool in SOURCES:
            binaries[f'/tmp/s{stage}/{tool}'] = HERE / f's{stage}-link-{tool}.out'
    summaries = {}
    records = json.loads((HERE / 'results.json').read_text())
    for record in records:
        with (HERE / (record['step'] + '.tsv')).open() as file:
            for row in csv.DictReader(file, delimiter='\t'):
                path = row['path']
                if path not in binaries or not binaries[path].exists():
                    continue
                header = struct.unpack('>8H', binaries[path].read_bytes()[:16])
                assert header[0] == 0o411
                static_end = (header[2] + header[3] + 63) & ~63
                peak = max(static_end, int(row['maximum_break']))
                low = int(row['minimum_sp'])
                total = summaries.setdefault(path, {'text': header[1], 'data': header[2],
                    'bss': header[3], 'maximum_break': static_end, 'minimum_sp': 65535,
                    'minimum_gap': 65535, 'max_stack_below_initial_sp': 0, 'executions': 0})
                total['maximum_break'] = max(total['maximum_break'], peak)
                total['minimum_sp'] = min(total['minimum_sp'], low)
                total['minimum_gap'] = min(total['minimum_gap'], low - peak)
                total['max_stack_below_initial_sp'] = max(total['max_stack_below_initial_sp'],
                                                         int(row['initial_sp']) - low)
                total['executions'] += 1
    (HERE / 'memory.json').write_text(json.dumps(summaries, indent=2) + '\n')
    if len(records) == len(plans()):
        convergence = {}
        for tool, sources in SOURCES.items():
            for source in sources:
                assert (HERE / ('s1-' + source + '.b')).read_bytes() == (HERE / ('s2-' + source + '.b')).read_bytes()
            first = (HERE / ('s1-link-' + tool + '.out')).read_bytes()
            second = (HERE / ('s2-link-' + tool + '.out')).read_bytes()
            assert first == second, tool
            convergence[tool] = {'identical': True, 'objects_compared': len(sources),
                                 'sha256': hashlib.sha256(second).hexdigest()}
        (HERE / 'convergence.json').write_text(json.dumps(convergence, indent=2) + '\n')
    print(json.dumps(summaries, indent=2))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup', action='store_true', help='create a fresh trial disk and reset results')
    parser.add_argument('--limit', type=int, help='maximum additional build steps')
    parser.add_argument('--summary', action='store_true', help='summarize completed steps without running more')
    args = parser.parse_args()
    if args.summary:
        summarize()
        return
    if args.setup:
        setup()
    records = json.loads((HERE / 'results.json').read_text())
    driver = HERE / 'host/test_driver'
    if not driver.exists():
        driver = SYS / 'test_driver'
    pending = list(enumerate(plans()))[len(records):]
    if args.limit is not None:
        pending = pending[:args.limit]
    for index, (name, directory, _) in pending:
        print('START', name, flush=True)
        started = time.monotonic()
        log = HERE / (name + '.log')
        with log.open('wb') as output:
            result = subprocess.run(list(map(str, [driver, '-c', '60000000000',
                '-d', HERE / 'hd.img', '-o', HERE / 'next.img', '-P', HERE / (name + '.tsv'),
                '-i', 'runner /tmp/p%03d %s\\n' % (index, directory),
                '-w', 'NATIVE CC DONE', '-I', 'exit\\n', '-x', 'NATIVE CC PASS'])),
                cwd=SYS, stdout=output, stderr=subprocess.STDOUT, timeout=1800)
        if result.returncode or b'FAILED command' in log.read_bytes():
            raise SystemExit('FAILED ' + name + ': see ' + str(log))
        (HERE / 'next.img').replace(HERE / 'hd.img')
        record = {'step': name, 'seconds': round(time.monotonic() - started, 2)}
        if '-link-' not in name and '-test-' not in name:
            source = name.split('-', 1)[1]
            data = Filesystem(HERE / 'hd.img').read(directory + '/' + source + '.b')
            (HERE / (name + '.b')).write_bytes(data)
            record['object_bytes'] = len(data)
            if name.startswith('s2-'):
                assert data == (HERE / ('s1-' + source + '.b')).read_bytes(), name
        if '-link-' in name:
            tool = name.split('-')[-1]
            data = Filesystem(HERE / 'hd.img').read(directory + '/' + tool)
            (HERE / (name + '.out')).write_bytes(data)
            header = struct.unpack('>8H', data[:16])
            assert header[0] == 0o411
            record.update(zip(['text', 'data', 'bss'], header[1:4]))
        records.append(record)
        (HERE / 'results.json').write_text(json.dumps(records, indent=2) + '\n')
        print('PASS', record, flush=True)
    summarize()


if __name__ == '__main__':
    main()
