#!/usr/bin/env python3
"""Install /boot, /unix, /fpe and a boot block into a NEW copy of a V7 disk.

The input must be an unmounted, big-endian Z8000 V7 filesystem. Existing boot files are refused; a matching development /unix is reused
in place in the new copy. Other kernels are refused: replace /unix normally from Unix; reinstall the primary
block after moving/replacing /boot. No existing inode or file is relocated.
"""
import argparse
import struct
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

class Installer:
    def __init__(self,path):
        self.disk=bytearray(path.read_bytes())
        fsize=self.u32(self.disk,514)
        if not 2<fsize<=0x10000000 or len(self.disk)%512:
            raise ValueError('invalid V7 filesystem size')
        # Sparse mkfs images are padded; a reserved panic-dump tail is retained.
        self.disk.extend(bytes(max(0,fsize*512-len(self.disk))))
        self.sb=memoryview(self.disk)[512:1024]
        self.isize=self.u16(self.sb,0)
        self.fsize=self.u32(self.sb,2)
        if not 2<self.isize<self.fsize<=0x10000000 or len(self.disk)<self.fsize*512:
            raise ValueError('expected V7 filesystem within the 28-bit LBA range')
        self.allocated=set()
    @staticmethod
    def u16(b,o): return struct.unpack_from('>H',b,o)[0]
    @staticmethod
    def u32(b,o): return struct.unpack_from('>I',b,o)[0]
    @staticmethod
    def p16(b,o,v): struct.pack_into('>H',b,o,v)
    @staticmethod
    def p32(b,o,v): struct.pack_into('>I',b,o,v)
    def block(self,n):
        if not self.isize<=n<self.fsize: raise ValueError('invalid data block')
        return memoryview(self.disk)[n*512:(n+1)*512]
    def inode(self,n):
        off=((n+15)//8)*512+((n-1)%8)*64
        return memoryview(self.disk)[off:off+64]
    def alloc(self):
        n=self.u16(self.sb,6)
        if not 1<=n<=50: raise ValueError('invalid/exhausted free list')
        b=self.u32(self.sb,8+4*(n-1))
        if b in self.allocated: raise ValueError('duplicate free block')
        data=self.block(b)
        self.allocated.add(b)
        if n==1:
            n=self.u16(data,0)
            if not 1<=n<=50: raise ValueError('invalid free-list chain')
            self.sb[8:208]=data[2:202]
        else: n-=1
        self.p16(self.sb,6,n)
        self.p32(self.sb,418,self.u32(self.sb,418)-1)
        data[:]=bytes(512)
        return b
    def root_blocks(self):
        root=self.inode(2)
        size=self.u32(root,8)
        if size>10*512 or size%16: raise ValueError('unsupported root directory size')
        return root,size,[int.from_bytes(root[12+3*i:15+3*i],'big') for i in range((size+511)//512)]
    def kernel(self,content):
        """Reuse a matching development /unix without relocating its inode."""
        root,size,blocks=self.root_blocks()
        for pos in range(0,size,16):
            ent=self.block(blocks[pos//512])[pos%512:pos%512+16]
            number=self.u16(ent,0)
            if not number or bytes(ent[2:16]).split(b'\0')[0]!=b'unix': continue
            ino=self.inode(number);length=self.u32(ino,8)
            if length!=len(content) or length>138*512:
                raise ValueError('existing /unix does not match this kernel')
            count=(length+511)//512
            sectors=[int.from_bytes(ino[12+3*i:15+3*i],'big') for i in range(min(count,10))]
            if count>10:
                indirect=self.block(int.from_bytes(ino[42:45],'big'))
                sectors += [self.u32(indirect,4*i) for i in range(count-10)]
            old=b''.join(bytes(self.block(b)) for b in sectors)[:length]
            patched=bytearray(old)
            if old!=content:
                if old[14:18]!=bytes(4) or any(old[40:552]):
                    raise ValueError('existing /unix is not an unbooted matching kernel')
                patched[14:18]=content[14:18];patched[40:552]=content[40:552]
                if patched!=content:
                    raise ValueError('existing /unix does not match this kernel')
            for i,b in enumerate(sectors):
                self.block(b)[:]=content[i*512:(i+1)*512].ljust(512,b'\0')
            return
        self.add('unix',content)

    def add(self,name,content):
        root,size,blocks=self.root_blocks()
        for pos in range(0,size,16):
            ent=self.block(blocks[pos//512])[pos%512:pos%512+16]
            if self.u16(ent,0) and bytes(ent[2:16]).split(b'\0')[0]==name.encode():
                raise ValueError('boot file already exists: '+name)
        inode_number=next((i for i in range(3,(self.isize-2)*8+1) if not self.u16(self.inode(i),0)),None)
        if inode_number is None: raise ValueError('no free inode')
        ino=self.inode(inode_number);ino[:]=bytes(64)
        self.p16(ino,0,0o100755);self.p16(ino,2,1);self.p32(ino,8,len(content))
        sectors=[]
        for offset in range(0,len(content),512):
            b=self.alloc();self.block(b)[:]=content[offset:offset+512].ljust(512,b'\0');sectors.append(b)
        if len(sectors)>138: raise ValueError('boot file exceeds single indirection')
        addresses=sectors[:10]
        if len(sectors)>10:
            indirect=self.alloc()
            for i,b in enumerate(sectors[10:]): self.p32(self.block(indirect),i*4,b)
            addresses.append(indirect)
        for i,b in enumerate(addresses): ino[12+i*3:15+i*3]=b.to_bytes(3,'big')
        # Invalidate the inode cache; the kernel will refill it from the inode table.
        self.p16(self.sb,208,0)
        self.p16(self.sb,422,self.u16(self.sb,422)-1)
        slot=next((pos for pos in range(0,size,16) if not self.u16(self.block(blocks[pos//512]),pos%512)),size)
        if slot==size:
            if size%512==0:
                if len(blocks)==10: raise ValueError('root directory full')
                b=self.alloc();root[12+len(blocks)*3:15+len(blocks)*3]=b.to_bytes(3,'big');blocks.append(b)
            self.p32(root,8,size+16)
        entry=self.block(blocks[slot//512])[slot%512:slot%512+16]
        self.p16(entry,0,inode_number);entry[2:]=name.encode().ljust(14,b'\0')
        return sectors

def install(source,output,build,console_profile=False):
    if output.exists(): raise ValueError('refusing to overwrite output disk')
    fs=Installer(source)
    sectors=fs.add('boot',(build/'boot').read_bytes())
    fs.kernel((build/'unix').read_bytes())
    fs.add('fpe',(build/'fpe.image').read_bytes())
    if console_profile:
        fs.add('.profile',b"/bin/stty erase '^H'\n")
    if not 1<=len(sectors)<=63: raise ValueError('/boot exceeds primary-loader capacity')
    boot=bytearray((build/'block.bin').read_bytes())
    if len(boot)!=512: raise ValueError('boot block must be 512 bytes')
    struct.pack_into('>H',boot,256,len(sectors))
    for i,b in enumerate(sectors): struct.pack_into('>I',boot,258+4*i,b)
    fs.disk[:512]=boot
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_bytes(fs.disk)

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('source',type=Path);p.add_argument('output',type=Path)
    p.add_argument('--build',type=Path,default=ROOT/'tests/build/z8001unix')
    p.add_argument('--console-profile',action='store_true',help='install /.profile configuring Backspace as the shell erase key (requires /bin/stty)')
    a=p.parse_args()
    install(a.source,a.output,a.build,a.console_profile)
    print('Installed disk boot:',a.output)
