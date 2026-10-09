#!/usr/bin/env python3
"""Compile, assemble, archive, link and execute the s.out C pipeline in V7."""
from pathlib import Path
import json
import re
import struct
import subprocess
import sys
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT/'tests/build/sout-cc'
SYS = ROOT/'v7z8000/usr/sys/build'
sys.path.insert(0,str(ROOT/'tools/native-cc'))
from build import image
from selfhost import Filesystem


def main():
    if sys.argv[1:] not in ([],['--resume']):
        raise SystemExit('usage: test.py [--resume]')
    resume = bool(sys.argv[1:])
    files = {}
    native = ROOT/'tests/build/native-environment/native'
    for name in ('ar','make','cp','rm','cmp'):
        files['bin/'+name] = native/'bin'/name
    for name in ('front','back','oz8'):
        files['lib/'+name] = native/'lib'/name
    for name in ('cc','asz8k','ldz8'):
        files['bin/'+name] = WORK/'seed'/(name+'.out')
    files['lib/crt0.b'] = WORK/'crt0.b'
    files['lib/libc.a'] = WORK/'libc.a'
    files['usr/lib/asz8k.pd'] = ROOT/'tools/asz8k/src/asz8k.pd'
    old = Filesystem(ROOT/'tests/build/userland-native/hd.img')
    for name in ('runner','sh'):
        path=WORK/name; path.write_bytes(old.read('/bin/'+name)); files['bin/'+name]=path
    steps=[]
    def step(name,directory,commands):
        path=WORK/('p%03d'%len(steps))
        path.write_text('\n'.join(c if c.startswith('1 ') else '0 - '+c for c in commands)+'\n')
        files['tmp/'+path.name]=path
        steps.append((name,directory,'/tmp/'+path.name))

    files['usr/src/test/hello.c']=ROOT/'tools/native-cc/hello.c'
    files['usr/src/test/libctest.c']=ROOT/'tools/libctest.c'
    for name in ('common1.c','common2.c','strong.c'):
        files['usr/src/test/'+name]=ROOT/'tools/sout-cc'/name
    step('hello','/usr/src/test',[
        '/bin/cc -i hello.c -o hello','./hello',
        '/bin/cc -O -i hello.c -o helloopt','./helloopt',
        '/bin/cc hello.c -o hellocomb','./hellocomb',
        '/bin/cc -S hello.c','/bin/cc -i hello.az8 -o helloasm','./helloasm'])
    step('libc-seed','/usr/src/test',[
        '/bin/cc -O -i libctest.c -o /bin/libctest','/bin/libctest'])
    step('commons','/usr/src/test',[
        '/bin/cc -c common1.c common2.c strong.c',
        '/bin/cc -i common1.b common2.b -o commons','./commons',
        '/bin/ldz8 -z -r common1.b common2.b -o partial.b',
        '/bin/cc -i partial.b -o frompart','./frompart',
        '/bin/cc -i common1.b common2.b strong.b -o strong','./strong strong',
        '/bin/ar rc libstrong.a strong.b',
        '/bin/cc -i common1.b common2.b libstrong.a -o fromarc','./fromarc strong'])
    # Run every unchanged assembly input with the native assembler and compare
    # its complete object (including symbols/relocations) against the host.
    members=json.loads((WORK/'library.json').read_text())
    for member in members:
        name=member['member']
        files['usr/src/oracle/'+name.replace('.b','.az8')]=WORK/name.replace('.b','.az8')
        files['usr/src/oracle/'+name]=WORK/name
    for i in range(0,len(members),15):
        commands=[]
        for m in members[i:i+15]:
            name=m['member']
            commands += ['/bin/asz8k -zc -o native.b '+name.replace('.b','.az8'),
                         '/bin/cmp native.b '+name]
        step('oracle-%03d'%i,'/usr/src/oracle',commands)
    # Rebuild libc from the original C sources inside V7, with native make/ar.
    names=re.search(r'LIBV7_NAMES = (.*?)\nLIBV7_OBJS',(ROOT/'tools/Makefile').read_text(),re.S)[1].replace('\\\n',' ').split()
    objects=[m['member'] for m in members if m['member']!='crt0.b']
    rules=['all: libc.a']
    for obj in objects+['crt0.b']:
        name=Path(obj).stem
        if name in names or name=='softfp':
            sources=[ROOT/'v7z8000/usr/src/libc'/p/(name+'.c') for p in ('gen','stdio')]
            sources += [ROOT/'tools/libc'/(name+'.c')]
            if name=='softfp': sources=[ROOT/'tools/fpe/glue.c']
            source=next(p for p in sources if p.exists())
            files['usr/src/libc/'+name+'.c']=source
            rules += [obj+': '+name+'.c','\t/bin/cc -O -Dunix=1 -c '+name+'.c']
        else:
            files['usr/src/libc/'+name+'.az8']=WORK/(name+'.az8')
            rules += [obj+': '+name+'.az8','\t/bin/asz8k -zc -o '+obj+' '+name+'.az8']
    rules += ['libc.a: '+' '.join(objects),'\t/bin/rm -f libc.a']
    for i in range(0,len(objects),20): rules += ['\t/bin/ar qc libc.a '+' '.join(objects[i:i+20])]
    path=WORK/'libc.mk'; path.write_text('\n'.join(rules)+'\n'); files['usr/src/libc/makefile']=path
    for i in range(0,len(objects),10):
        step('libc-%03d'%i,'/usr/src/libc',['/bin/make '+' '.join(objects[i:i+10])])
    step('install-libc','/usr/src/libc',[
        '/bin/make libc.a crt0.b','/bin/cp libc.a /lib/libc.a','/bin/cp crt0.b /lib/crt0.b'])
    step('libc-native','/usr/src/test',[
        '/bin/cc -O -i libctest.c -o /bin/libctest','/bin/libctest'])
    # The assembler and linker then rebuild themselves using the s.out tools
    # and the libc just compiled natively. Keep the installed seed until each
    # complete replacement executable has linked successfully.
    for p in (ROOT/'tools/asz8k/src').iterdir():
        if p.suffix in ('.c','.h','.pd'): files['usr/src/asz8k/'+p.name]=p
    asobjects=[]
    for p in sorted((ROOT/'tools/asz8k/src').glob('*.c')):
        asobjects.append(p.stem+'.b')
        step('as-'+p.stem,'/usr/src/asz8k',['/bin/cc -O -c '+p.name])
    step('assembler-link','/usr/src/asz8k',[
        '/bin/cc -i -s '+' '.join(asobjects)+' -o asz8k','/bin/cp asz8k /bin/asz8k'])
    for p in [ROOT/'tools/ldz8'/n for n in ('dispatch.c','ldso.c')]+[ROOT/'PCC-z8000/z8000'/n for n in ('ldz8.c','b.out.h')]+[ROOT/'tools/asz8k/src'/n for n in ('soutfmt.c','soutfmt.h')]:
        files['usr/src/ldz8/'+p.name]=p
    for name in ('dispatch','ldso','soutfmt'):
        step('ld-'+name,'/usr/src/ldz8',['/bin/cc -O -c '+name+'.c'])
    step('linker-link','/usr/src/ldz8',[
        '/bin/cc -i -s dispatch.b ldso.b soutfmt.b -o ldz8','/bin/cp ldz8 /bin/ldz8'])
    files['usr/src/test/ccz8.c']=ROOT/'PCC-z8000/z8000/ccz8.c'
    step('driver','/usr/src/test',[
        '/bin/cc -O -i -DTWOPASS -DSOUT ccz8.c -o cc','/bin/cp cc /bin/cc'])
    step('selfhost-smoke','/usr/src/test',[
        '/bin/cc -O -i hello.c -o finalhello','./finalhello',
        '/bin/cc -O -i libctest.c -o /bin/libctest','/bin/libctest'])
    completed=[]
    if resume:
        completed=json.loads((WORK/'completed.json').read_text())
        assert completed == [s[0] for s in steps[:len(completed)]], 'trial steps changed; start a fresh trial'
        saved=Filesystem(WORK/'hd.img')
        for target,path in files.items():
            if target.startswith(('usr/src/','tmp/','usr/lib/')):
                assert saved.read('/'+target) == path.read_bytes(), ('trial inputs changed; start fresh',target)
    else:
        image(files,WORK/'hd.img',blocks=18000,inodes=1800)
        (WORK/'completed.json').write_text('[]\n')
    for name,directory,plan in steps[len(completed):]:
        print('START',name,flush=True)
        logpath=WORK/(name+'.log')
        with logpath.open('wb') as log:
            result=subprocess.run([str(SYS/'test_driver'),'-c','200000000000',
                '-d',str(WORK/'hd.img'),'-o',str(WORK/'next.img'),
                '-i','runner '+plan+' '+directory+'\\n','-w','NATIVE CC DONE',
                '-I','exit\\n','-x','NATIVE CC DONE'],cwd=SYS,
                stdout=log,stderr=subprocess.STDOUT,timeout=900)
        data=logpath.read_bytes()
        assert result.returncode==0 and b'NATIVE CC PASS\r\n' in data, (name,logpath)
        if name in ('libc-seed','libc-native','selfhost-smoke'):
            assert b'libc: 39 passed, 0 failed' in data, name
        (WORK/'next.img').replace(WORK/'hd.img')
        completed.append(name)
        (WORK/'completed.json').write_text(json.dumps(completed,indent=2)+'\n')
        print('PASS',name,flush=True)
    fs=Filesystem(WORK/'hd.img')
    for target,path in files.items():
        if target.startswith('usr/src/') and path.suffix in ('.c','.h','.pd'):
            assert fs.read('/'+target)==path.read_bytes(), target
    sizes={}
    for name in ('asz8k','ldz8','cc'):
        data=fs.read('/bin/'+name); path=WORK/('native-'+name); path.write_bytes(data)
        assert struct.unpack_from('>H',data)[0]==0xe711, name
        sizes[name]=dict(zip(('text','data','bss'),struct.unpack_from('>3H',data,28)))
    (WORK/'results.json').write_text(json.dumps({'steps':completed,'native':sizes,'members':len(members)},indent=2)+'\n')
    print('PASS native s.out pipeline, libc and tool self-rebuild:',sizes)


if __name__=='__main__': main()
