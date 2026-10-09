#!/usr/bin/env python3
"""Seed the shared s.out C pipeline and rebuild startup/libc with asz8k."""
from pathlib import Path
import json
import re
import shutil
import struct
import sys
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/native-binutils'))
from build import compile_c, run
WORK = ROOT/'tests/build/sout-cc'
PCC = ROOT/'PCC-z8000/z8000'
AS = ROOT/'tests/build/asz8k-host/asz8k'
LD = ROOT/'tests/build/ldz8-host/ldz8'


def library():
    WORK.mkdir(parents=True, exist_ok=True)
    run(['make','-C',ROOT/'tools/asz8k'])
    run(['make','-C',ROOT/'tools/ldz8'])
    run(['make','-C',ROOT/'tools','libv7.a','libc/crt0.b'])
    shutil.copyfile(ROOT/'tools/asz8k/src/asz8k.pd',WORK/'asz8k.pd')
    query = WORK/'sources.mk'
    query.write_text('include Makefile\nprint-sout-sources:\n\t@echo $(LIBV7_OBJS) libc/crt0.b\n')
    paths = run(['make','-s','-C',ROOT/'tools','-f',query,
        'print-sout-sources']).stdout.decode().split()
    report = []
    for name in paths:
        obj = ROOT/'tools'/name
        source = obj.with_suffix('.az8')
        shutil.copyfile(source,WORK/source.name)
        run([AS,'-zc','-o',obj.name,source.name],cwd=WORK)
        data = (WORK/obj.name).read_bytes()
        h = struct.unpack('>HIIHHIHHH',data[:24])
        old = obj.read_bytes(); a = struct.unpack('>8H',old[:16])
        t,d,b = struct.unpack_from('>3H',data,28)
        assert h[0]==0xe707 and t<=a[1] and tuple((n+3)&~3 for n in (d,b))==a[2:4], (name,(t,d,b),a[1:4])
        # az8 rounds sections to four bytes; s.out uses word alignment.
        # Adjust only recorded local relocations for that placement difference.
        normalized = bytearray(old[16:16+a[1]+a[2]])
        pos = 16+a[1]+a[2]
        for length, base in ((a[6],0),(a[7],a[1])):
            for r in range(pos,pos+length,8):
                info,symbol,offset = struct.unpack_from('>HHI',old,r)
                kind = (info>>14)&3
                if kind in (1,2):
                    width = (1,2,4)[(info>>12)&3]
                    start = base+offset
                    val = int.from_bytes(normalized[start:start+width],'big')
                    delta = t-a[1] + (d-a[2] if kind==2 else 0)
                    normalized[start:start+width] = ((val+delta)&((1<<(8*width))-1)).to_bytes(width,'big')
            pos += length
        equal = data[40:40+t]==normalized[:t] and data[40+t:40+t+d]==normalized[a[1]:a[1]+d]
        compact_byte = bool(re.search(r'\bldb\s+r[0-7],#',source.read_text()))
        if not compact_byte and (t+3)&~3 == a[1]: assert equal, name
        report.append({'member':obj.name,'image_equal':equal,'text':t,'data':d,'bss':b,
                       'legacy_text':a[1]})
        print('PASS assemble',obj.name,flush=True)
    archive=WORK/'libc.a'; archive.unlink(missing_ok=True)
    run(['ar','rc',archive,*[Path(p).name for p in paths if Path(p).name!='crt0.b']],cwd=WORK)
    (WORK/'library.json').write_text(json.dumps(report,indent=2)+'\n')
    return paths


def seeds():
    directory=WORK/'seed'; directory.mkdir(exist_ok=True)
    sizes={}
    for tool,sources,flags in [
        ('asz8k',sorted((ROOT/'tools/asz8k/src').glob('*.c')),[]),
        ('ldz8',[ROOT/'tools/ldz8/dispatch.c',ROOT/'tools/ldz8/ldso.c',ROOT/'tools/asz8k/src/soutfmt.c'],
            ['-I'+str(PCC),'-I'+str(ROOT/'tools/asz8k/src')]),
        ('cc',[PCC/'ccz8.c'],['-DTWOPASS','-DSOUT']),
    ]:
        dest=directory/tool; dest.mkdir(exist_ok=True)
        objects=[]
        for source in sources:
            obj=dest/(source.stem+'.b'); compile_c(source,obj,flags); objects.append(obj)
        run([PCC/'ldz8','-i','-x',ROOT/'tools/libc/crt0.b',*objects,ROOT/'tools/libv7.a','-o',directory/(tool+'.out')])
        h=struct.unpack('>8H',(directory/(tool+'.out')).read_bytes()[:16])
        assert h[0]==0o411 and h[2]+h[3]<65536
        sizes[tool]=dict(zip(('text','data','bss'),h[1:4]))
        print('SEED',tool,sizes[tool],flush=True)
    (WORK/'seed-sizes.json').write_text(json.dumps(sizes,indent=2)+'\n')


if __name__=='__main__': library(); seeds()
