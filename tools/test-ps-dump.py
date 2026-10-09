#!/usr/bin/env python3
"""Inspect a stopped-machine RAM/swap snapshot with the natively built V7 ps."""
from pathlib import Path
import argparse, struct, subprocess, sys
sys.dont_write_bytecode=True
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools/native-cc'))
from build import image, compile_c, run
from selfhost import Filesystem

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('build',type=Path,nargs='?',default=ROOT/'v7z8000/usr/sys/build')
a=p.parse_args()
BUILD=a.build.resolve()
INSPECT=ROOT/'tests/build/inspection'
WORK=ROOT/'tests/build/ps-dump';WORK.mkdir(parents=True,exist_ok=True)

def trial(name, disk, commands, expect, extra=(), cycles=15000000000, status=0):
    with (WORK/(name+'.log')).open('wb') as log:
        result=subprocess.run([str(BUILD/'test_driver'),'-d',str(disk),
            '-c',str(cycles),'-i',commands,'-x',expect,*map(str,extra)],
            cwd=BUILD,stdout=log,stderr=subprocess.STDOUT,timeout=600)
    output=(WORK/(name+'.log')).read_bytes()
    assert result.returncode==status and expect.encode() in output,output[-4000:]
    return output

# No process or device runs between the RAM and swap saves. The pressure
# fixture leaves nine children and their parent asleep instead of killing them.
trial('capture',INSPECT/'pressure.img','pressure dump\\n','inspection dump: ready',
      ['-R','512','-K',WORK/'core','-W',WORK/'swap'])
assert (WORK/'core').stat().st_size==512*1024
assert (WORK/'swap').stat().st_size==4096*1024
short=WORK/'short';short.write_bytes((WORK/'core').read_bytes()[:-1])
empty=WORK/'empty';empty.write_bytes(b'')
plan=WORK/'plan'
plan.write_text('\n'.join([
    '0 default.txt /bin/ps axlk',
    '0 explicit.txt /bin/ps axlk /unix /usr/sys/core /dev/swap',
    '0 pid.txt /bin/ps axlk4 /unix /usr/sys/core /dev/swap',
    '1 - /bin/ps axlk /unix /tmp/short /dev/swap',
    '1 - /bin/ps axlk /unix /tmp/missing /dev/swap',
    '1 - /bin/ps axlk /unix /usr/sys/core /tmp/empty',
    '1 - /bin/ps axlk /unix /usr/sys/core /tmp/missing',
    '1 - /bin/ps axlk /bin/pressure /usr/sys/core /dev/swap',
])+'\n')
# This is a different boot with its own empty swap unit. /dev/swap is a regular
# saved image here; the kernel's internal swap device remains ATA unit 1.
files={'bin/ps':INSPECT/'ps','bin/pressure':INSPECT/'pressure',
       'bin/runner':INSPECT/'runner','unix':BUILD/'handler.sout',
       'usr/sys/core':WORK/'core','dev/swap':WORK/'swap',
       'tmp/short':short,'tmp/empty':empty,'tmp/plan':plan}
image(files,WORK/'disk.img',blocks=24000,
      modes={'usr/sys/core':0o600,'dev/swap':0o600,'tmp/short':0o600})
output=trial('inspect',WORK/'disk.img','runner /tmp/plan /tmp\\n','NATIVE CC DONE',
             ['-o',WORK/'saved.img'])
assert b'NATIVE CC PASS' in output and b'FAILED command' not in output
fs=Filesystem(WORK/'saved.img')
default=fs.read('/tmp/default.txt');explicit=fs.read('/tmp/explicit.txt')
assert default==explicit,(default,explicit)
for name in ('default','explicit','pid'):
    (WORK/(name+'.txt')).write_bytes(fs.read('/tmp/'+name+'.txt'))
rows=[line.split() for line in default.splitlines()[1:]]
assert {int(row[3]) for row in rows}==set(range(13))|{14,15},default
pressure=[row for row in rows if row[-2:]==[b'pressure',b'dump']]
assert len(pressure)==11,default
assert any(int(row[0],8)&1 for row in pressure),default
assert any(not int(row[0],8)&1 for row in pressure),default
assert b'init' in default and b'-sh' in default and b'swapper' in default
assert any(row[1]==b'T' and row[3]==b'14' for row in pressure),default
assert any(row[3]==b'15' and row[-1]==b'<defunct>' for row in rows),default
pidrows=fs.read('/tmp/pid.txt').splitlines()[1:]
assert len(pidrows)==1 and pidrows[0].split()[3]==b'4',pidrows
for marker in (b'Incomplete physical memory dump',b'No mem',
               b'Cannot read dumped u-area',b'No saved swap',b'No namelist'):
    assert marker in output,marker
print('PASS native ps k: physical kernel bank, resident/swapped processes, saved arguments and PID filter')
print('PASS ps k rejects missing/truncated RAM, missing/truncated swap and a missing namelist')

# Exercise the same captures after a genuine kernel panic, not only idle HALT.
source=WORK/'panic.c'
source.write_text('#include <stdio.h>\nmain() { puts("dump-panic-ready");fflush(stdout);for(;;)pause(); }\n')
compile_c(source,WORK/'panic.b')
rt=ROOT/'tests/build/sout-cc'
run([ROOT/'tests/build/ldz8-host/ldz8','-x',rt/'crt0.b',WORK/'panic.b',rt/'libc.a','-o',WORK/'panic'])
image({'bin/panic':WORK/'panic'},WORK/'panic.img')
kernel=(BUILD/'handler.sout').read_bytes()
symbol_start=40+struct.unpack_from('>I',kernel,2)[0]
symbol_bytes=struct.unpack_from('>H',kernel,12)[0]
symbols={name.rstrip(b'\0'):value for value,kind,segment,name in
         struct.iter_unpack('>IBB8s',kernel[symbol_start:symbol_start+symbol_bytes])}
output=trial('panic-capture',WORK/'panic.img','panic\\n','panic: kernel access fault',
      ['-R','512','-w','dump-panic-ready','-I','',
       '-F','k:%x'%symbols[b'_time'],'-K',WORK/'panic-core','-W',WORK/'panic-swap'],
      cycles=200000000,status=1)
assert b'Halted: Yes' in output and b'panic flush:' in output
panicplan=WORK/'panic-plan';panicplan.write_text('0 panic.txt /bin/ps axlk\n')
files.update({'usr/sys/core':WORK/'panic-core','dev/swap':WORK/'panic-swap',
              'tmp/plan':panicplan})
image(files,WORK/'panic-inspect.img',blocks=24000,
      modes={'usr/sys/core':0o600,'dev/swap':0o600,'tmp/short':0o600})
output=trial('panic-inspect',WORK/'panic-inspect.img','runner /tmp/plan /tmp\\n',
             'NATIVE CC DONE',['-o',WORK/'panic-saved.img'])
assert b'NATIVE CC PASS' in output and b'FAILED command' not in output
report=Filesystem(WORK/'panic-saved.img').read('/tmp/panic.txt')
(WORK/'panic.txt').write_bytes(report)
assert {int(line.split()[3]) for line in report.splitlines()[1:]}==set(range(4)),report
assert b'panic' in report and b'runner' not in report,report
print('PASS native ps k inspects RAM/swap captured after a real kernel panic')
