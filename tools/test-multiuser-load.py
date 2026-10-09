#!/usr/bin/env python3
"""Native compilation, swapping, daemons, logout and reboot on a runtime disk."""
from pathlib import Path
import argparse
import re
import subprocess
import sys
sys.dont_write_bytecode = True
import multiuser as m

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--ram', type=int, default=512)
parser.add_argument('--prepared', type=Path, help='reuse an already prepared fixture disk')
args = parser.parse_args()
WORK = m.ROOT / 'tests/build/multiuser-load' / ('ram-'+str(args.ram))
WORK.mkdir(parents=True, exist_ok=True)
BUILD = m.ROOT / 'v7z8000/usr/sys/build'
profile = WORK / 'profile'
profile.write_text((m.ROOT / 'v7z8000/etc/root.profile').read_text()+"PS1='ROOT> '\nexport PS1\n")
cron = WORK / 'crontab'
cron.write_text('* * * * * echo CRON-LOAD-PASS >> /usr/adm/cron-load\n')
payload = WORK / 'payload.c'
payload.write_text('''#include <stdio.h>
main() { int i; long sum; sum=0; for(i=0;i<1000;i++)sum+=i;
if(sum!=499500L)return 1; puts("NATIVE LOAD PASS");return 0; }
''')
files = {str(p.relative_to(m.WORK/'native')): p
         for p in (m.WORK/'native').rglob('*') if p.is_file()}
assert files, 'Run tools/multiuser.py --rebuild-startup first'
for name in ('libc.a', 'crt0.b'):
    files['lib/'+name] = m.ROOT/'tests/build/sout-cc'/name
files.update({'.profile': profile, 'usr/lib/crontab': cron,
              'usr/src/load.c': m.ROOT/'tools/multiuser-load.c',
              'usr/src/payload.c': payload,
              'usr/src/muprobe.c': m.ROOT/'tools/multiuser-probe.c'})
disk = m.build(destination=WORK/'seed.img', extra_files=files)

def trial(name, disk, actions, verdict, ram=8192):
    plan = WORK/(name+'-actions')
    plan.write_text(''.join(marker.replace('\n','\\n')+'\t'+text.replace('\n','\\n')+'\n'
                            for marker, text in actions))
    log = WORK/(name+'.log')
    saved = WORK/(name+'.img')
    print('START', name, 'RAM', ram, flush=True)
    with log.open('wb') as out:
        result = subprocess.run([str(BUILD/'test_driver'), '-7', '-T', '66667',
            '-R', str(ram), '-d', str(disk), '-i', '\x04', '-q', 'ROOT> ',
            '-A', str(plan), '-c', '100000000000', '-x', verdict, '-o', str(saved)],
            cwd=BUILD, stdout=out, stderr=subprocess.STDOUT, timeout=900)
    output = log.read_bytes()
    assert result.returncode == 0, str(log)
    assert b'Absent RAM accesses: 0' in output and b'Unmapped accesses: 0' in output, str(log)
    assert b'panic:' not in output and b'out of space' not in output, str(log)
    print('Completed', name, flush=True)
    return saved, output

if args.prepared:
    disk = args.prepared.resolve()
else:
    disk, _ = trial('prepare', disk, [
        ('login: ', 'root\n'),
        ('ROOT> ', 'cc -O -i /usr/src/load.c -o /bin/load\n'),
        ('ROOT> ', 'cc -O -i /usr/src/muprobe.c -o /bin/muprobe\n'),
        ('ROOT> ', "test -s /bin/load && test -s /bin/muprobe && sync && echo LOAD-''PREPARED\n"),
    ], 'LOAD-PREPARED')

disk, output = trial('pressure', disk, [
    ('login: ', 'root\n'),
    ('ROOT> ', 'rm -f /usr/adm/release\n'),
    ('ROOT> ', 'cat /dev/null > /usr/adm/cron-load\n'),
    ('ROOT> ', 'chmod 666 /usr/adm/cron-load\n'),
    ('ROOT> ', '/bin/load > /usr/adm/memory-load &\n'),
    ('LOAD READY\r\n', 'ps axl > /usr/adm/ps-pressure\n'),
    ('ROOT> ', '/bin/load check > /usr/adm/swapped\n'),
    ('ROOT> ', 'cc -O -i /usr/src/payload.c -o /usr/adm/native-i; echo compile-i $?: done\n'),
    ('ROOT> ', '/usr/adm/native-i; echo execute-i $?: done\n'),
    ('ROOT> ', 'cc -O /usr/src/payload.c -o /usr/adm/native-n; echo compile-n $?: done\n'),
    ('ROOT> ', '/usr/adm/native-n; echo execute-n $?: done\n'),
    ('ROOT> ', 'echo pipeline-proof | cat | cat > /usr/adm/pipeline\n'),
    ('ROOT> ', 'while test ! -s /usr/adm/cron-load; do sleep 2; done\n'),
    ('ROOT> ', 'echo release > /usr/adm/release\nwait\n'),
    ('ROOT> ', 'cat /usr/adm/memory-load\n'),
    ('ROOT> ', "/bin/muprobe\nsync\necho LOAD-SESSION-''DONE\n"),
    ('LOAD-SESSION-DONE\r\n', '\x04'),
    ('login: ', 'root\n'),
    ('ROOT> ', "/bin/muprobe\nps axl > /usr/adm/ps-after\nsync\necho LOAD-''PERSISTED\n"),
], 'LOAD-PERSISTED', ram=args.ram)
fs = m.Filesystem(disk)
memory = fs.read('/usr/adm/memory-load')
assert b'LOAD MEMORY PASS\n' in memory, ('memory holders failed', memory, WORK/'pressure.log')
swapped = fs.read('/usr/adm/swapped')
assert int(re.search(rb'LOAD SWAPPED (\d+)', swapped)[1]) > 0 or args.ram > 512, swapped
assert output.count(b'NATIVE LOAD PASS\r\n') == 2
for stage in ('compile-i', 'execute-i', 'compile-n', 'execute-n'):
    assert (stage+' 0: done\r\n').encode() in output, stage
assert b'MULTIUSER ROOT PASS' in output
assert fs.read('/usr/adm/pipeline') == b'pipeline-proof\n'
assert b'CRON-LOAD-PASS\n' in fs.read('/usr/adm/cron-load')
listings = {name: fs.read('/usr/adm/'+name)
            for name in ('ps-pressure', 'ps-after')}
after = [line.split() for line in listings['ps-after'].splitlines()[1:]]
pressure = [line.split() for line in listings['ps-pressure'].splitlines()[1:]]
for command, uid in ((b'/etc/update', b'0'), (b'/etc/cron', b'1')):
    daemons = [row for row in after if row[4] == b'1' and row[-1] == command]
    assert len(daemons) == 1 and daemons[0][2] == uid, listings['ps-after']
    # Live V7 ps samples process metadata and arguments separately. Swapping
    # can stale its command text; cron also forks temporary job children.
    # Compare the daemon's PID, UID and init parent during pressure instead.
    same = [row for row in pressure if row[3] == daemons[0][3]]
    assert len(same) == 1 and same[0][2] == uid and same[0][4] == b'1', listings['ps-pressure']
for name, data in listings.items():
    (WORK/name).write_bytes(data)
assert fs.read('/usr/adm/native-i')[:2] == b'\xe7\x11'
assert fs.read('/usr/adm/native-n')[:2] == b'\xe7\x07'
records = fs.read('/usr/adm/wtmp')
assert [records[i+8:i+16].rstrip(b'\0') for i in range(0,len(records),20)] == [b'root',b'root',b'',b'root']
counts = re.search(rb'Swap sectors: (\d+) read, (\d+) written', output)
assert counts and min(map(int, counts.groups())) > 0
print('PASS private memory, native split/combined builds, pipes, cron/update and relogin; swap sectors', counts.groups(), flush=True)

before = {name: fs.read('/usr/adm/'+name) for name in
          ('native-i', 'native-n', 'pipeline', 'memory-load', 'swapped')}
disk, output = trial('reboot', disk, [
    ('login: ', 'root\n'),
    ('ROOT> ', "/usr/adm/native-i\n/usr/adm/native-n\ncat /usr/adm/pipeline /usr/adm/memory-load\n/bin/muprobe\nps axl > /usr/adm/ps-reboot\nsync\necho LOAD-REBOOT-''PASS\n"),
], 'LOAD-REBOOT-PASS', ram=args.ram)
fs = m.Filesystem(disk)
for name, data in before.items():
    assert fs.read('/usr/adm/'+name) == data, name
assert output.count(b'NATIVE LOAD PASS\r\n') == 2 and b'MULTIUSER ROOT PASS' in output
data = fs.read('/usr/adm/ps-reboot')
assert data.count(b'/etc/update') == 1 and data.count(b'/etc/cron') == 1, data
(WORK/'ps-reboot').write_bytes(data)
print('PASS synced filesystem persistence and executable reuse after fresh kernel boot', flush=True)
