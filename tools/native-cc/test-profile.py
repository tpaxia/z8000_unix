#!/usr/bin/env python3
"""Check guest disk persistence and user-memory observations independently."""
import csv
import re
import subprocess
from build import ROOT, PCC, compile_c, image, run
from selfhost import Filesystem

work = ROOT / 'tests/build/profile-check'
work.mkdir(parents=True, exist_ok=True)
source = work / 'probe.c'
source.write_text('''#include <stdio.h>
unsigned low;
descend(n) int n; {
    char space[512];
    space[0] = n;
    if ((unsigned)space < low) low = (unsigned)space;
    if (n) descend(n-1);
    return space[0];
}
main() {
    char *sbrk();
    int fd;
    low = 65535;
    if (sbrk(4096) == (char *)-1) return 1;
    if (brk((char *)65520) != -1) return 3;
    descend(3);
    printf("PROFILE %u %u\\n", brk(0), low);
    fd = creat("/tmp/saved", 0600);
    if (fd < 0 || write(fd, "persisted", 9) != 9) return 2;
    close(fd); sync();
    return 0;
}
''')
compile_c(source, work / 'probe.b')
run([PCC / 'ldz8', '-i', '-x', ROOT / 'tools/libc/crt0.b', work / 'probe.b',
     ROOT / 'tools/libv7.a', '-o', work / 'probe'])
image({'bin/probe': work / 'probe'}, work / 'hd.img')
sys = ROOT / 'v7z8000/usr/sys/build'
result = subprocess.run(list(map(str, [sys / 'test_driver', '-c', '300000000',
    '-d', work / 'hd.img', '-o', work / 'saved.img', '-P', work / 'memory.tsv',
    '-i', 'probe\\n', '-w', 'PROFILE ', '-I', 'exit\\n', '-x', 'PROFILE '])),
    cwd=sys, capture_output=True, timeout=60)
(work / 'run.log').write_bytes(result.stdout + result.stderr)
result.check_returncode()
heap, low = map(int, re.search(rb'PROFILE (\d+) (\d+)', result.stdout).groups())
with (work / 'memory.tsv').open() as file:
    record = next(r for r in csv.DictReader(file, delimiter='\t') if r['path'] == '/bin/probe')
assert int(record['maximum_break']) == heap, (record, heap)
assert low - 128 <= int(record['minimum_sp']) <= low, (record, low)
assert int(record['initial_sp']) - int(record['minimum_sp']) >= 4 * 512
assert Filesystem(work / 'saved.img').read('/tmp/saved') == b'persisted'
print('PASS: heap limit, recursive stack observation, and persisted guest file', record)
