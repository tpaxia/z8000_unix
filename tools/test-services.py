#!/usr/bin/env python3
"""Public ABI, accounting, profiling and memory locking."""
from pathlib import Path
import subprocess
import sys
root = Path(__file__).resolve().parents[1]
tools = root / 'tools'
pcc = root / 'PCC-z8000/z8000'
if len(sys.argv) not in (2, 3) or (len(sys.argv) == 3 and sys.argv[2] != '--policy-only'):
    raise SystemExit('usage: test-services.py build-dir [--policy-only]')
policy_only = len(sys.argv) == 3
build = Path(sys.argv[1]).resolve()
work = root / 'tests/build/services'
work.mkdir(parents=True, exist_ok=True)
(work / 'asz8k.pd').write_bytes((tools / 'asz8k/src/asz8k.pd').read_bytes())
def run(args, **kw):
    r = subprocess.run(list(map(str, args)), capture_output=True, **kw)
    if r.returncode:
        sys.stdout.buffer.write(r.stdout + r.stderr)
        r.check_returncode()
    return r.stdout
run(['make', '-C', tools, 'libv7.a', 'libc/crt0.b', 'sh', 'init', 'v7mkfs'])
pre = run(['cpp', '-nostdinc', '-undef', '-Dz8000', '-Dz8002',
           '-I' + str(root / 'v7z8000/usr/include'), tools / 'servicetest.c'])
(work / 'exec.az8').write_bytes(run([pcc / 'cz8/cz8'], input=pre))
run([tools.parent / 'tests/build/asz8k-host/asz8k', '-c', '-o', 'exec.b', 'exec.az8'], cwd=work)
kernel = root / 'v7z8000/usr/sys'
headers = ''.join('#include <sys/%s.h>\n' % h for h in ['param', 'dir', 'user', 'proc', 'text'])
headers += '#include <stdio.h>\nextern int coremap[], nswap, runin, runout;\n'
cpu = (kernel / 'machine/cpu.c').read_text().split('\naddupc(pc, p, ticks)\n',1)[1]
mmu = (kernel / 'machine/paged.c').read_text().split('/* Extent growth must keep',1)[1].split('\nswapout(p)\n',1)[0]
mmu = '/* Extent growth must keep' + mmu
victims = (kernel / 'sys/slp.c').read_text().split('struct proc *\nswapvict(skip)\n',1)[1].split('/*\n * Switch to the highest-priority',1)[0]
(work / 'policy.c').write_text(headers + 'struct proc *\nswapvict(skip)\n' + victims + 'addupc(pc, p, ticks)\n' + cpu +
    '\n#define malloc coremalloc\n#define swapout coreswap\n#define sleep coresleep\n#define wakeup corewake\n' + mmu +
    '\n#undef malloc\n#undef swapout\n#undef sleep\n#undef wakeup\n' + (tools / 'servicepolicy.c').read_text())
pre = run(['cpp', '-nostdinc', '-undef', '-Dz8000', '-Dz8002',
           '-I' + str(root / 'v7z8000/usr/include'), work / 'policy.c'])
(work / 'policy.az8').write_bytes(run([pcc / 'cz8/cz8'], input=pre))
run([tools.parent / 'tests/build/asz8k-host/asz8k', '-c', '-o', 'policy.b', 'policy.az8'], cwd=work)
run([tools.parent / 'tests/build/ldz8-host/ldz8', '-x', tools / 'libc/crt0.b', work / 'policy.b', tools / 'libv7.a', '-o', work / 'policy'])
# Compile actual shared text/fork routines with deterministic failing storage.
shared = (kernel / 'sys/text.c').read_text()
shared = shared[shared.index('/* V7 inode-backed'):]
fork = (kernel / 'sys/sys1.c').read_text().split('\nfork()\n',1)[1].split('/*\n * exec system call.',1)[0]
head = ''.join('#include <sys/%s.h>\n' % h for h in
    ['param', 'dir', 'user', 'proc', 'text', 'inode', 'file', 'map', 'acct'])
head += '#include <stdio.h>\n#define malloc tmalloc\n#define mfree tmfree\n#define sleep tsleep\n#define wakeup twake\n#define time testtime\nextern long testtime;\n#undef MAXUPRC\n#define MAXUPRC 2\n'
(work / 'proc.c').write_text(head + shared + '\nforktest()\n' + fork + (tools / 'procpolicy.c').read_text())
pre = run(['cpp', '-nostdinc', '-undef', '-Dz8000', '-Dz8002',
           '-I' + str(root / 'v7z8000/usr/include'), work / 'proc.c'])
(work / 'proc.az8').write_bytes(run([pcc / 'cz8/cz8'], input=pre))
run([tools.parent / 'tests/build/asz8k-host/asz8k', '-c', '-o', 'proc.b', 'proc.az8'], cwd=work)
run([tools.parent / 'tests/build/ldz8-host/ldz8', '-x', tools / 'libc/crt0.b', work / 'proc.b', tools / 'libv7.a', '-o', work / 'proc'])
schedule = (kernel / 'sys/slp.c').read_text().split('\nsched()\n',1)[1].split('/*\n * V7 sched() victim policy',1)[0]
head = ''.join('#include <sys/%s.h>\n' % h for h in ['param', 'dir', 'user', 'proc', 'text'])
head += '#include <stdio.h>\n#include <setjmp.h>\n#define sleep schedsleep\nint runin, runout;\n'
(work / 'sched.c').write_text(head + 'sched()\n' + schedule + (tools / 'schedpolicy.c').read_text())
pre = run(['cpp', '-nostdinc', '-undef', '-Dz8000', '-Dz8002',
           '-I' + str(root / 'v7z8000/usr/include'), work / 'sched.c'])
(work / 'sched.az8').write_bytes(run([pcc / 'cz8/cz8'], input=pre))
run([tools.parent / 'tests/build/asz8k-host/asz8k', '-c', '-o', 'sched.b', 'sched.az8'], cwd=work)
run([tools.parent / 'tests/build/ldz8-host/ldz8', '-x', tools / 'libc/crt0.b', work / 'sched.b', tools / 'libv7.a', '-o', work / 'sched'])
for layout in ['combined', 'split']:
    run([tools.parent / 'tests/build/ldz8-host/ldz8', '-x', *(['-i'] if layout == 'split' else []),
         tools / 'libc/crt0.b', work / 'exec.b', tools / 'libv7.a', '-o', work / 'exectest'])
    # mkfs represents the set-ID bits as the second and third mode characters.
    (work / 'proto').write_text(f'''boot
2400 128
d--755 0 0
bin d--755 0 0
sh ---755 0 0 {tools}/sh
services ---755 0 0 {work}/exectest
policy ---755 0 0 {work}/policy
proc ---755 0 0 {work}/proc
sched ---755 0 0 {work}/sched
$
dev d--755 0 0
console c--644 0 0 0 0
tty c--644 0 0 2 0
$
etc d--755 0 0
init ---755 0 0 {tools}/init
$
tmp d--777 0 0
$
$
''')
    run([tools / 'v7mkfs', work / 'hd.img', work / 'proto'])
    for ram in ([] if policy_only else [8192, 320]):
        r = subprocess.run(list(map(str, [build / 'test_driver', '-c', '3000000000',
            '-R', ram, '-S', 4096, '-d', work / 'hd.img', '-i', '/bin/services\\n',
            '-w', 'services: passed', '-I', 'exit\\n', '-x', 'services: passed'])),
            cwd=build, capture_output=True, timeout=90)
        output = r.stdout + r.stderr
        label = f'{layout}-{ram}'
        (work / (label + '.log')).write_bytes(output)
        if r.returncode or b'services: FAIL' in output:
            sys.stdout.buffer.write(output)
            raise SystemExit(label + ': services regression failed')
        assert b'Absent RAM accesses: 0' in output
        print(label + ': services passed', flush=True)

r = run([build / 'test_driver', '-c', '400000000', '-d', work / 'hd.img',
    '-i', '/bin/policy\\n', '-w', 'policy: passed', '-I', 'exit\\n', '-x', 'policy: passed'], cwd=build, timeout=30)
assert b'policy: FAIL' not in r
print('service policy: passed')

r = run([build / 'test_driver', '-c', '400000000', '-d', work / 'hd.img',
    '-i', '/bin/proc\\n', '-w', 'proc policy: passed', '-I', 'exit\\n', '-x', 'proc policy: passed'], cwd=build, timeout=30)
assert b'proc policy: FAIL' not in r
print('process/text policy: passed')

r = run([build / 'test_driver', '-c', '400000000', '-d', work / 'hd.img',
    '-i', '/bin/sched\\n', '-w', 'sched policy: passed', '-I', 'exit\\n', '-x', 'sched policy: passed'], cwd=build, timeout=30)
assert b'sched policy: FAIL' not in r
print('scheduler policy: passed')
