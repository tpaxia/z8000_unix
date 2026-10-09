#!/usr/bin/env python3
"""Build utilities inside V7; compare host/native reads and stripping."""
from pathlib import Path
import json
import shutil
import struct
import subprocess
import sys
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/native-cc'))
from build import image, run
from selfhost import Filesystem
SOURCE = ROOT/'tools/sout-utils'
WORK = ROOT/'tests/build/sout-utils'
HOST = ROOT/'tests/build/sout-utils-host'
AS = ROOT/'tests/build/asz8k-host/asz8k'
LD = ROOT/'tests/build/ldz8-host/ldz8'
SYS = ROOT/'v7z8000/usr/sys/build'


def main():
    if sys.argv[1:] not in ([], ['--reuse-build']):
        raise SystemExit('usage: test.py [--reuse-build]')
    reuse = bool(sys.argv[1:])
    WORK.mkdir(parents=True, exist_ok=True)
    for directory in ('asz8k', 'ldz8', 'sout-utils'):
        run(['make', '-C', ROOT/'tools'/directory])
    shutil.copyfile(ROOT/'tools/asz8k/src/asz8k.pd', WORK/'asz8k.pd')
    for name in ('start.8kn', 'help.8kn', 'ext.8ks'):
        shutil.copyfile(ROOT/'tools/ldz8/tests'/name, WORK/name)
    shutil.copyfile(ROOT/'tools/asz8k/tests/soutseg.8ks', WORK/'seg.8ks')
    for name in ('start.8kn', 'help.8kn', 'seg.8ks', 'ext.8ks'):
        run([AS, '-z', *(['-s'] if name.endswith('s') else []), name], cwd=WORK)
    for name, options in [('combined', []), ('split', ['-i']), ('partial', ['-r'])]:
        run([LD, '-z', *options, '-e', 'entry', 'start.so', 'help.so', '-o', name], cwd=WORK)
    for name, options in [('segexec', []), ('segpart', ['-r']), ('segrev', ['-C','5','-D','3'])]:
        run([LD, '-z', *options, '-e', 'entry', 'seg.so', 'ext.so', '-o', name], cwd=WORK)
    # Historical reader fixture; no legacy assembler/linker participates.
    (WORK/'legacy.b').write_bytes(struct.pack('>8H',0o407,2,0,0,12,0,0,0)+
        bytes.fromhex('9e08')+struct.pack('>8sHH',b'entry',0o42,0))
    fs = Filesystem(ROOT/'tests/build/native-cc-sout/hd.img')
    shutil.copyfile(ROOT/'tests/build/sout-cc/seed/cc.out', WORK/'compiler')
    wide = bytearray((WORK/'combined').read_bytes())
    struct.pack_into('>H', wide, 12, struct.unpack_from('>H', wide, 12)[0]+14)
    wide += struct.pack('>IBB8s', 0xffffffff, 33, 0, b'wideabs')
    (WORK/'wideabs').write_bytes(wide)
    # Mixed portable archive, odd non-object member and both object formats.
    archive = bytearray(b'!<arch>\n')
    for name in ('start.so', 'seg.so', 'legacy.b'):
        data = (WORK/name).read_bytes()
        archive += (('%-16s%-12s%-6s%-6s%-8s%-10s`\n' %
            (name+'/', 0, 0, 0, '644', len(data))).encode()+data+b'\n'*(len(data)&1))
    archive += b'notes/          0           0     0     644     3         `\nabc\n'
    (WORK/'mixed.a').write_bytes(archive)
    corrupt = {}
    base = (WORK/'combined').read_bytes()
    for name, offset, value in [('badflags',18,2),('badtotals',28,254),
                                ('badreserved',20,1)]:
        data=bytearray(base); struct.pack_into('>H',data,offset,value); corrupt[name]=bytes(data)
    corrupt['short'] = base[:-1]
    data=bytearray(base); symoff=40+struct.unpack_from('>I',data,2)[0]
    # A defined symbol must refer to a segment-table entry.
    for pos in range(symoff,len(data),14):
        if data[pos+4]&31 >= 2: data[pos+5]=255; break
    corrupt['badseg']=bytes(data)
    corrupt['badarc']=bytes(archive[:-1])
    for name,data in corrupt.items(): (WORK/name).write_bytes(data)

    files = {}
    for target in ('bin/cc','bin/asz8k','bin/ldz8','bin/sh',
                   'lib/crt0.b','lib/libc.a','lib/front','lib/back','lib/oz8'):
        path=WORK/'seed'/target; path.parent.mkdir(parents=True,exist_ok=True)
        path.write_bytes(fs.read('/'+target)); files[target]=path
    files['bin/runner']=ROOT/'tests/build/native-cc-sout/runner'
    for name in ('ar','cp','cmp'):
        files['bin/'+name]=ROOT/'tests/build/native-environment-sout/native/bin'/name
    files['usr/lib/asz8k.pd']=ROOT/'tools/asz8k/src/asz8k.pd'
    sources=list(SOURCE.glob('*.c'))+[SOURCE/'object.h']+[
        ROOT/'tools/asz8k/src/soutfmt.c',ROOT/'tools/asz8k/src/soutfmt.h']
    for path in sources: files['usr/src/utils/'+path.name]=path
    fixtures=['start.so','help.so','seg.so','ext.so','combined','split','partial',
              'segexec','segpart','segrev','legacy.b','compiler','wideabs','mixed.a']
    for name in fixtures+list(corrupt): files['usr/src/utils/'+name]=WORK/name
    build=[]
    for name in ('object','soutfmt','nlist','nm','size','strip'):
        build += ['/bin/cc -O -c '+name+'.c']
    for name in ('nm','size','strip'):
        build += ['/bin/cc -i '+name+'.b object.b soutfmt.b -o '+name]
    build += ['/bin/cc -i check.c nlist.b object.b soutfmt.b -o check']
    checks=[]; expected={}
    def command(args, failure=False):
        result=subprocess.run(list(map(str,args)),cwd=WORK,capture_output=True)
        assert (result.returncode != 0)==failure, (args,result.stderr)
        return result.stdout
    for name in fixtures:
        for flags in ('','-g','-u','-n','-r','-p','-o'):
            args=[HOST/'nm',*([flags] if flags else []),name]
            output=command(args); dest='out%03d'%len(expected)
            expected[dest]=output
            checks += ['0 '+dest+' ./nm '+(flags+' ' if flags else '')+name]
        if name != 'mixed.a':
            output=command([HOST/'size',name]); dest='out%03d'%len(expected)
            expected[dest]=output; checks += ['0 '+dest+' ./size '+name]
    # Multiple operands and combined switches retain original V7 presentation.
    for tool,args in [('nm',['-gnro','combined','mixed.a']),('size',['combined','split','segexec'])]:
        dest='out%03d'%len(expected); expected[dest]=command([HOST/tool,*args])
        checks += ['0 '+dest+' ./'+tool+' '+' '.join(args)]
    strips=[]
    for i,name in enumerate(['combined','split','partial','segexec','segpart','segrev','compiler','legacy.b','wideabs']):
        target='s%d'%i; shutil.copyfile(WORK/name,WORK/target)
        command([HOST/'strip',target]); first=(WORK/target).read_bytes()
        command([HOST/'strip',target]); assert (WORK/target).read_bytes()==first
        files['usr/src/utils/'+target]=WORK/name
        strips.append(target)
        checks += ['0 - ./strip '+target,'0 - ./strip '+target,'0 - ./size '+target]
    checks += ['0 - ./s0','0 - ./s1','0 - ./s6','0 - ./check']
    for name in corrupt:
        for tool in ('nm','size','strip'):
            if name == 'badseg' and tool == 'size': continue
            before=(WORK/name).read_bytes(); command([HOST/tool,name],True)
            assert (WORK/name).read_bytes()==before, name
            checks += ['1 - ./'+tool+' '+name]
    # Replace the libc entry only in this trial and verify archive extraction.
    checks += ['0 - /bin/ar r /lib/libc.a nlist.b object.b soutfmt.b',
               '0 - /bin/cc -i check.c -o libcheck','0 - ./libcheck']
    for name,commands in [('build',['0 - '+c for c in build]),('checks',checks)]:
        plan=WORK/(name+'.plan'); plan.write_text('\n'.join(commands)+'\n')
        files['tmp/'+plan.name]=plan
    modes={'usr/src/utils/'+name:0o755 for name in strips}
    if reuse:
        saved=Filesystem(WORK/'hd.img')
        assert b'NATIVE CC PASS\r\n' in (WORK/'build.log').read_bytes()
        for source in sources:
            assert saved.read('/usr/src/utils/'+source.name)==source.read_bytes(),source
        for name in ('object.b','soutfmt.b','nlist.b','nm.b','size.b','strip.b',
                     'nm','size','strip','check'):
            path=WORK/'reuse'/name; path.parent.mkdir(exist_ok=True)
            path.write_bytes(saved.read('/usr/src/utils/'+name))
            files['usr/src/utils/'+name]=path
            if not name.endswith('.b'): modes['usr/src/utils/'+name]=0o755
    image(files,WORK/'hd.img',blocks=14000,inodes=1200,modes=modes)
    for name in (('checks',) if reuse else ('build','checks')):
        print('START',name,flush=True)
        logpath=WORK/(name+'.log')
        with logpath.open('wb') as log:
            result=subprocess.run([str(SYS/'test_driver'),'-c','200000000000',
                '-d',str(WORK/'hd.img'),'-o',str(WORK/'next.img'),
                '-i','runner /tmp/'+name+'.plan /usr/src/utils\\n','-w','NATIVE CC DONE',
                '-I','exit\\n','-x','NATIVE CC DONE'],cwd=SYS,
                stdout=log,stderr=subprocess.STDOUT,timeout=900)
        data=logpath.read_bytes()
        assert result.returncode==0 and b'NATIVE CC PASS\r\n' in data,logpath
        assert b'Absent RAM accesses: 0' in data and b'stack warnings: 0' in data,logpath
        (WORK/'next.img').replace(WORK/'hd.img'); print('PASS',name,flush=True)
    native=Filesystem(WORK/'hd.img')
    for dest,output in expected.items(): assert native.read('/usr/src/utils/'+dest)==output,dest
    for name in strips: assert native.read('/usr/src/utils/'+name)==(WORK/name).read_bytes(),name
    for name,data in corrupt.items(): assert native.read('/usr/src/utils/'+name)==data,name
    for source in sources: assert native.read('/usr/src/utils/'+source.name)==source.read_bytes(),source
    sizes={}
    for name in ('nm','size','strip'):
        data=native.read('/usr/src/utils/'+name); (WORK/('native-'+name)).write_bytes(data)
        sizes[name]=dict(zip(('text','data','bss'),struct.unpack_from('>3H',data,28)))
    report={'output_comparisons':len(expected),'strip_comparisons':len(strips),
            'native_commands':len(build)+len(checks),'native':sizes}
    (WORK/'results.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS',report)


if __name__=='__main__': main()
