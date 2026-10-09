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
    """Build startup/libc directly as s.out; no legacy object producer is used."""
    WORK.mkdir(parents=True, exist_ok=True)
    run(['make','-C',PCC/'cz8'])
    run(['make','-C',ROOT/'tools/asz8k'])
    run(['make','-C',ROOT/'tools/ldz8'])
    shutil.copyfile(ROOT/'tools/asz8k/src/asz8k.pd',WORK/'asz8k.pd')
    query = WORK/'sources.mk'
    query.write_text('include Makefile\nprint-sout-sources:\n\t@echo $(LIBV7_NAMES)\n')
    names = run(['make','-s','-C',ROOT/'tools','-f',query,
        'print-sout-sources']).stdout.decode().split()
    runtime = ROOT/'tools/libc'
    run([sys.executable,runtime/'split-syscalls.py',WORK])
    syscalls = run([sys.executable,runtime/'split-syscalls.py','--names']).stdout.decode().split()
    run([sys.executable,ROOT/'tools/fpe/wrappers.py',WORK/'epu.az8'])
    names += ['setjmp',*syscalls,'float','softfp','epu','arith','csv','object','soutfmt']
    # The s.out nlist adapter uses stdio. Put its reader before V7's stdio
    # members so the real cleanup is selected before the fakcu fallback.
    first = ['nlist','object','soutfmt']
    names = first + [n for n in names if n not in first]
    report = []
    for name in names+['crt0']:
        assembly = WORK/(name+'.az8')
        if name == 'nlist': source=ROOT/'tools/sout-utils/nlist.c'
        elif name == 'object': source=ROOT/'tools/sout-utils/object.c'
        elif name == 'soutfmt': source=ROOT/'tools/asz8k/src/soutfmt.c'
        elif name == 'softfp': source=ROOT/'tools/fpe/glue.c'
        else:
            source=next((p for p in [ROOT/'v7z8000/usr/src/libc/stdio'/(name+'.c'),
                ROOT/'v7z8000/usr/src/libc/gen'/(name+'.c'),runtime/(name+'.c')]
                if p.exists()),None)
        obj=WORK/(name+'.b')
        if source:
            compile_c(source,obj,['-I'+str(ROOT/'tools/sout-utils'),
                '-I'+str(ROOT/'tools/asz8k/src'),
                '-I'+str(ROOT/'v7z8000/usr/src/libc/stdio')],sout=True)
        else:
            if name in ('float','arith','csv'): shutil.copyfile(PCC/'lib'/(name+'.az8'),assembly)
            elif (runtime/(name+'.az8')).exists(): shutil.copyfile(runtime/(name+'.az8'),assembly)
            if not assembly.exists(): raise SystemExit('Missing runtime source: '+name)
            run([AS,'-zc','-o',obj.name,assembly.name],cwd=WORK)
        data=obj.read_bytes()
        t,d,b=struct.unpack_from('>3H',data,28)
        assert struct.unpack_from('>H',data)[0]==0xe707,name
        report.append({'member':obj.name,'text':t,'data':d,'bss':b})
        print('PASS assemble',obj.name,flush=True)
    archive=WORK/'libc.a'; archive.unlink(missing_ok=True)
    run(['ar','rc',archive,*[n+'.b' for n in names]],cwd=WORK)
    (WORK/'library.json').write_text(json.dumps(report,indent=2)+'\n')
    return [n+'.b' for n in names+['crt0']]


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
            obj=dest/(source.stem+'.b'); compile_c(source,obj,flags,sout=True); objects.append(obj)
        run([LD,'-z','-i','-x',WORK/'crt0.b',*objects,WORK/'libc.a','-o',directory/(tool+'.out')])
        data=(directory/(tool+'.out')).read_bytes()
        h=struct.unpack_from('>3H',data,28)
        assert struct.unpack_from('>H',data)[0]==0xe711 and h[1]+h[2]<65536
        sizes[tool]=dict(zip(('text','data','bss'),h))
        print('SEED',tool,sizes[tool],flush=True)
    (WORK/'seed-sizes.json').write_text(json.dumps(sizes,indent=2)+'\n')


if __name__=='__main__': library(); seeds()
