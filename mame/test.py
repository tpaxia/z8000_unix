#!/usr/bin/env python3
"""Run a console regression against a disposable MAME disk copy."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('mame',type=Path)
p.add_argument('--machine', choices=['z8001unix','z8002unix'], default='z8001unix')
p.add_argument('--chdman',type=Path,required=True)
p.add_argument('--disk',type=Path)
p.add_argument('--rompath',type=Path)
p.add_argument('--input',default='cc -i /usr/src/hello.c -o /tmp/hello\n/tmp/hello\n')
p.add_argument('--seven-bit',action='store_true',help='strip software parity in the captured terminal transcript')
p.add_argument('--login',help='name to enter at the getty prompt after initial shell input')
p.add_argument('--boot',default='\n',help='input at the standalone loader prompt')
p.add_argument('--expect',default='Hello from native C')
p.add_argument('--save-disk',type=Path,help='export the modified guest disk after success')
p.add_argument('--settle',type=int,default=0,help='guest seconds to wait after expected output')
p.add_argument('--ram')
p.add_argument('--seconds',type=int,default=1200)
p.add_argument('--output',type=Path)
a=p.parse_args()
base=ROOT/'tests/build'/a.machine
if a.disk is None: a.disk=base/('boot.img' if a.machine=='z8002unix' else 'full-userland.img')
if a.rompath is None: a.rompath=base/'roms'
if a.output is None: a.output=base/'smoke'
if a.ram is None: a.ram='1M' if a.machine=='z8002unix' else '8M'
if a.save_disk and a.save_disk.exists(): raise SystemExit('refusing to overwrite saved disk')
a.output.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(prefix='disk-',dir=a.output) as tmp:
    disk=Path(tmp)/'root.chd'
    n=a.disk.stat().st_size
    if n%512: raise SystemExit('disk image is not sector aligned')
    subprocess.run([str(a.chdman.resolve()),'createhd','-i',str(a.disk.resolve()),'-o',str(disk.resolve()),
                    '-chs',f'{n//512},1,1','-ss','512','-c','none'],check=True,stdout=subprocess.DEVNULL)
    env=dict(os.environ,Z8001UNIX_LOG=str((a.output/'console.log').resolve()),
             Z8001UNIX_INPUT=a.input,Z8001UNIX_BOOT=a.boot,
             Z8001UNIX_EXPECT=a.expect.replace('\r',''),
             Z8001UNIX_SETTLE=str(a.settle))
    if a.seven_bit: env['Z8001UNIX_SEVEN_BIT']='1'
    if a.login: env['Z8001UNIX_LOGIN']=a.login
    command=[str(a.mame.resolve()),a.machine,'-window','-rompath',str(a.rompath.resolve()),
             '-hard',str(disk.resolve()),'-ram',a.ram.lower(),'-video','none','-sound','none','-nothrottle',
             '-skip_gameinfo','-seconds_to_run',str(a.seconds),'-autoboot_delay','0',
             '-autoboot_script',str(ROOT/'mame/smoke.lua'),'-inipath',str(Path(tmp).resolve()),
             '-cfg_directory',str(Path(tmp).resolve()),'-nvram_directory',str(Path(tmp).resolve())]
    r=subprocess.run(command,env=env,capture_output=True,timeout=max(180,a.seconds*2))
    (a.output/'mame.log').write_bytes(r.stdout+r.stderr)
    console=(a.output/'console.log').read_text() if (a.output/'console.log').exists() else ''
    if r.returncode or b'Z8001UNIX TEST PASS' not in r.stdout or 'panic:' in console:
        print((r.stdout+r.stderr).decode(errors='replace'));print(console)
        raise SystemExit('MAME test failed')
    if a.save_disk:
        subprocess.run([str(a.chdman.resolve()),'extracthd','-i',str(disk.resolve()),
                        '-o',str(a.save_disk.resolve())],check=True,stdout=subprocess.DEVNULL)
    print(console);print('MAME test passed')
