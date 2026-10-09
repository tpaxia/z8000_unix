#!/usr/bin/env python3
"""Reserve/recover the emulated board's raw crash area after a V7 filesystem."""
import argparse, struct
from pathlib import Path

def boundary(data):
    if len(data)<1024 or len(data)%512:raise ValueError('invalid disk size')
    isize,blocks=struct.unpack_from('>HI',data,512)
    if not 2<isize<blocks<=65535:
        raise ValueError('invalid or truncated V7 filesystem')
    return blocks*512

p=argparse.ArgumentParser(description=__doc__)
sub=p.add_subparsers(dest='command',required=True)
r=sub.add_parser('reserve');r.add_argument('source',type=Path);r.add_argument('output',type=Path)
r.add_argument('--ram-kib',type=int,required=True);r.add_argument('--swap-kib',type=int,default=4096)
e=sub.add_parser('extract');e.add_argument('disk',type=Path);e.add_argument('directory',type=Path)
a=p.parse_args()
try:
    if a.command=='reserve':
        if a.output.exists():raise ValueError('output already exists')
        data=a.source.read_bytes();base=boundary(data)
        if len(data)>base:raise ValueError('source already has a disk tail')
        if not 192<=a.ram_kib<=8192 or a.ram_kib%2 or not 0<=a.swap_kib<=16000:
            raise ValueError('invalid RAM/swap size')
        size=base+512+(a.ram_kib+a.swap_kib)*1024
        if size>65535*512:raise ValueError('dump exceeds current ATA sector range')
        with a.output.open('xb') as f:f.write(data);f.truncate(size)
        print('Reserved',size-base,'bytes after filesystem block',base//512)
    else:
        data=a.disk.read_bytes();base=boundary(data)
        magic,version,ram,swap,time=struct.unpack_from('>5I',data,base)
        if magic!=0x5a384b44 or version!=1:raise ValueError('no complete crash dump')
        if not 196608<=ram<=8388608 or ram%512 or swap>16384000 or swap%512 or base+512+ram+swap>len(data):
            raise ValueError('invalid dump lengths')
        a.directory.mkdir(parents=True,exist_ok=True)
        for name,start,size in [('core',base+512,ram),('swap',base+512+ram,swap)]:
            with (a.directory/name).open('xb') as f:f.write(data[start:start+size])
            (a.directory/name).chmod(0o600)
        print('Recovered',ram,'RAM bytes and',swap,'swap bytes; clock',time)
except (ValueError,OSError,struct.error) as ex:
    p.exit(1,str(ex)+'\n')
