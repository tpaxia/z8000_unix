#!/usr/bin/env python3
"""Exercise original V7 startup, authentication, credentials and logout."""
from pathlib import Path
import subprocess
import sys
sys.dont_write_bytecode = True
import multiuser as m
w = m.WORK
b = m.ROOT / 'v7z8000/usr/sys/build'
# Reuse a completed native rebuild; --rebuild-startup is the documented producer.
exports = {str(p.relative_to(w/'native')): p for p in (w/'native').rglob('*') if p.is_file()}
if not exports: exports = m.rebuild_startup()
for name in ('libc.a', 'crt0.b'):
    exports['lib/' + name] = m.ROOT / 'tests/build/sout-cc' / name
passwd = w/'test-passwd'
passwd.write_text((m.ROOT/'v7z8000/etc/passwd').read_text()+'test:abJnggxhB/yWI:100:10:Test:/usr/test:\n')
root_profile = w/'root-profile'
root_profile.write_text((m.ROOT/'v7z8000/etc/root.profile').read_text()+"PS1='ROOT> '\nexport PS1\n")
user_profile = w/'user-profile'
user_profile.write_text("stty erase '^H'\nPS1='USER> '\nexport PS1\n")
crontab = w/'crontab';crontab.write_text('* * * * * echo CRON-PASS > /tmp/cron-proof\n')
exports.update({'etc/passwd':passwd, '.profile':root_profile, 'usr/test/.profile':user_profile,
    'usr/lib/crontab':crontab, 'usr/src/muprobe.c':m.ROOT/'tools/multiuser-probe.c'})
disk=m.build(destination=w/'trial.img',extra_files=exports,
    extra_owners={'usr/test':(100,10),'usr/test/.profile':(100,10)})
actions = [
 ('login: ', 'root\n'),
 ('ROOT> ', 'cc -O -i /usr/src/muprobe.c -o /bin/muprobe\n/bin/muprobe\nps axl > /usr/adm/ps-start\nwho > /usr/adm/who-root\necho ROOT-DONE\n'),
 ('ROOT-DONE\r\n', '\x04'),
 ('login: ', 'test\n'),
 ('Password:', 'bad\n'),
 ('login: ', 'test\n'),
 ('Password:', 'password\n'),
 ('USER> ', '/bin/muprobe user\nwho > /usr/test/who-user\necho USER-DONE\n'),
 ('USER-DONE\r\n', 'su\n'),
 ('# ', '/bin/muprobe su\necho SU-DONE\n'),
 ('SU-DONE\r\n', '\x04'),
 ('USER> ', 'passwd\n'),
 ('Old password:', 'password\n'),
 ('New password:', 'newpass1\n'),
 ('Retype new password:', 'newpass1\n'),
 ('USER> ', '\x04'),
 ('login: ', 'test\n'),
 ('Password:', 'newpass1\n'),
 ('USER> ', '/bin/muprobe user\necho NEW-PASSWORD-DONE\n'),
 ('NEW-PASSWORD-DONE\r\n', '\x04'),
 ('login: ', 'root\n'),
 ('ROOT> ', '/bin/muprobe\ncat /tmp/cron-proof\nps axl > /usr/adm/ps-end\nsync\necho MULTIUSER-DONE\n'),
]
p=w/'actions';p.write_text(''.join(marker.replace('\n','\\n')+'\t'+text.replace('\n','\\n')+'\n' for marker,text in actions))
with (w/'sessions.log').open('wb') as log:
 r=subprocess.run([str(b/'test_driver'),'-7','-T','66667','-d',str(disk),'-i','\x04','-q','ROOT> ','-A',str(p),
   '-c','20000000000','-x','MULTIUSER-DONE','-o',str(w/'saved.img')],
   cwd=b,stdout=log,stderr=subprocess.STDOUT,timeout=600)
out=(w/'sessions.log').read_bytes()
if r.returncode:raise SystemExit('Session test failed: '+str(w/'sessions.log'))
for marker in (b'Login incorrect',b'MULTIUSER ROOT PASS',b'MULTIUSER USER PASS',b'MULTIUSER SETUID PASS',b'CRON-PASS\r\n'):
 assert marker in out,marker
assert b'password\r\n' not in out and b'bad\r\n' not in out,'password echoed'
fs=m.Filesystem(w/'saved.img')
assert fs.read('/usr/test/proof')==b'USER-WRITE\n'
assert b'root     console' in fs.read('/usr/adm/who-root')
assert b'test     console' in fs.read('/usr/test/who-user')
records=[fs.read('/usr/adm/wtmp')[i:i+20] for i in range(0,len(fs.read('/usr/adm/wtmp')),20)]
assert [rec[8:16].rstrip(b'\0') for rec in records]==[b'root',b'',b'test',b'',b'test',b'',b'root'],records
assert all(rec[:8].rstrip(b'\0')==b'console' for rec in records)
for path in ('/usr/adm/ps-start','/usr/adm/ps-end'):
 data=fs.read(path)
 assert data.count(b'/etc/update')==1 and data.count(b'/etc/cron')==1,data
 (w/Path(path).name).write_bytes(data)
print('PASS original init/getty/login: password rejection/change, setuid su, non-root credentials, tty ownership, logout/respawn, utmp/wtmp, update and cron')

# HUP to PID 1 must kill the sessions/daemons, enter single-user mode, then
# start one fresh getty/update/cron set after the administrator types EOF.
reset_actions = [('login: ', 'root\n'), ('ROOT> ', 'kill -1 1\n'),
                 ('ROOT> ', '\x04'), ('login: ', 'root\n'),
                 ('ROOT> ', '/bin/muprobe\nsync\necho INIT-RESET-DONE\n')]
p=w/'reset-actions';p.write_text(''.join(marker+'\t'+text.replace('\n','\\n')+'\n' for marker,text in reset_actions))
with (w/'reset.log').open('wb') as log:
 r=subprocess.run([str(b/'test_driver'),'-7','-T','66667','-d',str(w/'saved.img'),
   '-i','\x04','-q','ROOT> ','-A',str(p),'-c','5000000000','-x','INIT-RESET-DONE'],
   cwd=b,stdout=log,stderr=subprocess.STDOUT,timeout=180)
out=(w/'reset.log').read_bytes()
assert r.returncode==0 and b'MULTIUSER ROOT PASS' in out,out[-2000:]
print('PASS SIGHUP init shutdown, single-user transition and multiuser restart')
