#!/usr/bin/env python3
"""Check an installed bootstrap disk against its original filesystem image."""
import argparse
from pathlib import Path
import struct
from install_boot import Installer


def blocks(fs, inode):
    size=fs.u32(inode,8)
    addr=[int.from_bytes(inode[12+3*i:15+3*i],'big') for i in range(13)]
    def walk(block,depth):
        entries=struct.unpack('>128I',fs.block(block))
        for b in entries:
            if depth==1: yield b
            elif b: yield from walk(b,depth-1)
            else: yield from [0]*(128**(depth-1))
    result=addr[:10]
    for depth,threshold in [(1,10),(2,138),(3,16522)]:
        if size>threshold*512:
            result+=list(walk(addr[9+depth],depth))
    return result[:(size+511)//512]


def data(fs,inode):
    return b''.join(bytes(fs.block(b)) if b else bytes(512) for b in blocks(fs,inode))[:fs.u32(inode,8)]


def entries(fs):
    raw=data(fs,fs.inode(2))
    return {raw[i+2:i+16].split(b'\0')[0].decode():fs.u16(raw,i)
            for i in range(0,len(raw),16) if fs.u16(raw,i)}


def free(fs):
    n=fs.u16(fs.sb,6)
    cache=[fs.u32(fs.sb,8+i*4) for i in range(n)]
    result=set()
    while cache:
        b=cache.pop()
        if not b: break
        assert b not in result, 'duplicate free block'
        result.add(b)
        if not cache:
            chunk=fs.block(b)
            n=fs.u16(chunk,0)
            assert 1<=n<=50
            cache=[fs.u32(chunk,2+i*4) for i in range(n)]
    return result


def check(source,installed,build):
    old,new=Installer(source),Installer(installed)
    before,after=entries(old),entries(new)
    assert all(after.get(k)==v for k,v in before.items())
    for i in range(3,(old.isize-2)*8+1):
        ino=old.inode(i)
        if not old.u16(ino,0): continue
        assert bytes(ino)==bytes(new.inode(i)), ('changed inode',i)
        if old.u16(ino,0)&0o170000 in (0o100000,0o40000):
            original=data(old,ino)
            current=data(new,new.inode(i))
            if i==before.get('unix') and original!=current:
                patched=bytearray(original)
                assert original[14:18]==bytes(4) and not any(original[40:552])
                patched[14:18]=current[14:18]
                patched[40:552]=current[40:552]
                assert patched==current, 'changed kernel beyond entry/vectors'
            else:
                assert original==current, ('changed file',i)
    for name,file in [('boot','boot'),('unix','unix'),('fpe','fpe.image')]:
        assert data(new,new.inode(after[name]))==(build/file).read_bytes(),name
    count=new.u16(new.disk,256)
    sectors=[new.u32(new.disk,258+i*4) for i in range(count)]
    assert sectors==blocks(new,new.inode(after['boot']))
    remaining=free(new)
    original=free(old)
    assert remaining<=original
    assert old.u32(old.sb,418)-new.u32(new.sb,418)==len(original)-len(remaining), 'incorrect free block accounting'
    # Existing mkfs images count their free-list zero sentinel in s_tfree.

    used=set()
    for i in range(2,(new.isize-2)*8+1):
        ino=new.inode(i)
        if new.u16(ino,0)&0o170000 not in (0o100000,0o40000): continue
        used.update(b for b in blocks(new,ino) if b)
    assert not used&remaining, 'allocated data on free list'
    old_count=sum(old.u16(old.inode(i),0)==0 for i in range(1,(old.isize-2)*8+1))
    new_count=sum(new.u16(new.inode(i),0)==0 for i in range(1,(new.isize-2)*8+1))
    assert old_count-new_count==old.u16(old.sb,422)-new.u16(new.sb,422), 'incorrect free inode accounting'
    print('Boot files, primary sector list, original files and free lists verified')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('source',type=Path);p.add_argument('installed',type=Path)
    p.add_argument('--build',type=Path,default=Path(__file__).resolve().parents[1]/'tests/build/z8001unix')
    a=p.parse_args();check(a.source,a.installed,a.build)
