#!/usr/bin/env python3
"""Build the V7 development environment under Unix, with portable archives."""
import argparse
import json
import re
import hashlib
from pathlib import Path
import struct
import subprocess
import sys
import time
sys.dont_write_bytecode = True
from build import ROOT, PCC, PASSES, compile_c, image, run
from selfhost import Filesystem
from object_format import sizes

WORK = ROOT / 'tests/build/native-environment-sout'
SYS = ROOT / 'v7z8000/usr/sys/build'
CMD = ROOT / 'v7z8000/usr/src/cmd'


def setup(preserve=False, reset_compiler=False, sout=True):
    WORK.mkdir(parents=True, exist_ok=True)
    extra = {'usr/lib/yaccpar': PCC / 'yacc/yaccpar'}
    if not sout: extra['lib/libc.a']=PASSES/'libv7.a'
    modes={}
    for tool in ['front', 'back', 'oz8']:
        extra['lib/' + tool] = (PASSES/('target-'+tool)/tool if tool in ('front','back')
                              else ROOT/'tests/build/native-cc-sout'/tool)
    if preserve:
        fs = Filesystem(WORK / 'hd.img')
        def walk(number, path):
            offset = ((number+15)//8)*512 + ((number+15)%8)*64
            permissions = int.from_bytes(fs.disk[offset:offset+2], 'big')
            mode = permissions & 0o170000
            if mode == 0o040000:
                data = fs.data(number)
                for i in range(0,len(data),16):
                    child = int.from_bytes(data[i:i+2],'big')
                    name = data[i+2:i+16].split(b'\0')[0].decode()
                    if child and name not in ('.','..'):
                        walk(child, path + '/' + name if path else name)
            elif mode == 0o100000:
                if reset_compiler and path.startswith(('usr/src/front/','usr/src/back/')) and path.endswith('.b'):
                    return
                dest = WORK / 'saved-tree' / path
                dest.parent.mkdir(parents=True,exist_ok=True)
                dest.write_bytes(fs.data(number)); extra[path] = dest
                modes[path] = permissions
        walk(2,'')
    if reset_compiler:
        for tool in ['front', 'back', 'oz8']:
            extra['lib/' + tool] = (PASSES/('target-'+tool)/tool if tool in ('front','back')
                                  else ROOT/'tests/build/native-cc-sout'/tool)
    for name in ['runner', 'check']:
        compile_c(ROOT / 'tools/native-cc' / (name + '.c'), WORK / (name + '.b'),sout=sout)
        run([ROOT/'tests/build/ldz8-host/ldz8', '-z', '-x', ROOT/'tests/build/sout-cc/crt0.b', WORK / (name + '.b'),
             ROOT/'tests/build/sout-cc/libc.a', '-o', WORK / name])
        extra['bin/' + name] = WORK / name
    steps = []
    def step(name, directory, commands):
        path = WORK / ('p%03d' % len(steps))
        path.write_text('\n'.join('0 - ' + c for c in commands) + '\n')
        extra['tmp/' + path.name] = path
        steps.append({'name': name, 'directory': directory, 'plan': '/tmp/' + path.name})
    for tool in ['ar', 'cp', 'rm', 'mv', 'cmp']:
        extra['usr/src/utils/' + tool + '.c'] = CMD / (tool + '.c')
        step(tool, '/usr/src/utils', ['/bin/cc -O -i -Dunix=1 ' + tool + '.c -o /bin/' + tool])
    for name in ['y1.c', 'y2.c', 'y3.c', 'y4.c', 'dextern', 'files']:
        extra['usr/src/yacc/' + name] = CMD / 'yacc' / name
    for name in ['y1', 'y2', 'y3', 'y4']:
        step(name, '/usr/src/yacc', ['/bin/cc -O -c -Dunix=1 ' + name + '.c'])
    step('yacc', '/usr/src/yacc', ['/bin/cc -i y1.b y2.b y3.b y4.b -o /bin/yacc'])
    for name in ['ident.c', 'main.c', 'doname.c', 'misc.c', 'files.c', 'dosys.c', 'defs', 'gram.y']:
        extra['usr/src/make/' + name] = CMD / 'make' / name
    if sout:
        for path in [ROOT/'tools/sout-utils'/n for n in ('object.c','object.h')]+[
                ROOT/'tools/asz8k/src'/n for n in ('soutfmt.c','soutfmt.h')]:
            extra['usr/src/make/'+path.name]=path
    step('make-parser', '/usr/src/make', ['/bin/yacc gram.y'])
    for name in ['ident', 'main', 'doname', 'misc', 'files', 'dosys', 'y.tab']:
        step('make-' + name, '/usr/src/make', ['/bin/cc -O -c -Dunix=1 '+('-DSOUT ' if sout else '') + name + '.c'])
    if sout:
        for name in ('object','soutfmt'):
            step('make-'+name,'/usr/src/make',['/bin/cc -O -c '+name+'.c'])
    makeobjects='ident.b main.b doname.b misc.b files.b dosys.b y.tab.b'+(' object.b soutfmt.b' if sout else '')
    step('make', '/usr/src/make', ['/bin/cc -i '+makeobjects+' -o /bin/make'])
    extra['usr/src/pcc/cgram.y'] = PCC / 'cz8/cgram.y'
    step('pcc-parser', '/usr/src/pcc', ['/bin/yacc -v cgram.y'])

    def makegroup(group, sources, output, flags=''):
        directory = '/usr/src/' + group
        install = output
        output = output.rsplit('/',1)[-1]
        objects = [name.rsplit('.',1)[0]+'.b' for name in sources]
        rules = ['CC=/bin/cc', 'CFLAGS=-O -Dunix=1 ' + flags, 'all: ' + output]
        headers = {'az8':'mical.h inst.h ../b.out.h', 'ldz8':'soutfmt.h',
                   'asz8k':'acom.h asz8k.h obj.h soutfmt.h',
                   'nm':'object.h soutfmt.h','size':'object.h soutfmt.h','strip':'object.h soutfmt.h',
                   'front':'manifest macdefs mac2defs mfile1 mfile2 common',
                   'back':'manifest macdefs mac2defs mfile1 mfile2 common'}.get(group,'')
        for name,path in sources.items():
            if path is not None: extra['usr/src/' + group + '/' + name] = path
            obj = name.rsplit('.',1)[0]+'.b'
            rules += [obj + ': ' + name + ' ' + headers, '\t$(CC) $(CFLAGS) -c ' + name]
            step(group+'-'+name, directory,
                 ['/bin/rm -f '+obj, '/bin/make -f makefile ' + obj])
        rules += [output + ': ' + ' '.join(objects), '\t$(CC) -i ' + ' '.join(objects) + ' -o ' + output]
        if group=='front':
            rules += ['cgram.c: ../pcc/cgram.y /bin/yacc /usr/lib/yaccpar',
                      '\tcd ../pcc && /bin/yacc cgram.y', '\t/bin/cp ../pcc/y.tab.c cgram.c']
        path = WORK / (group + '.mk'); path.write_text('\n'.join(rules)+'\n')
        extra['usr/src/' + group + '/makefile'] = path
        commands=['/bin/make -f makefile ' + output]
        if group not in ('front','back'):commands.append('/bin/cp ' + output + ' ' + install)
        step(group+'-link', directory, commands)

    makegroup('cc', {'ccz8.c': PCC/'ccz8.c'}, '/bin/cc', '-DTWOPASS'+(' -DSOUT' if sout else ''))
    if sout:
        assembler=ROOT/'tools/asz8k/src'
        for path in assembler.iterdir():
            if path.suffix in ('.h','.pd'): extra['usr/src/asz8k/'+path.name]=path
        makegroup('asz8k',{p.name:p for p in sorted(assembler.glob('*.c'))},'/bin/asz8k')
        for tool in ('nm','size','strip'):
            for path in (ROOT/'tools/sout-utils/object.h',assembler/'soutfmt.h'):
                extra['usr/src/'+tool+'/'+path.name]=path
            makegroup(tool,{tool+'.c':ROOT/'tools/sout-utils'/(tool+'.c'),
                            'object.c':ROOT/'tools/sout-utils/object.c',
                            'soutfmt.c':assembler/'soutfmt.c'},'/bin/'+tool)
    else:
        for name in ['mical.h','inst.h']:
            extra['usr/src/az8/' + name] = PCC/'az8'/name
        extra['usr/src/b.out.h'] = PCC/'b.out.h'
        makegroup('az8', {n+'.c':PCC/'az8'/(n+'.c') for n in 'error init ins ioz8 ps rel sdi sym scan'.split()}, '/bin/az8')
    extra['usr/src/ldz8/soutfmt.h'] = ROOT/'tools/asz8k/src/soutfmt.h'
    makegroup('ldz8', {n+'.c': ROOT/'tools/ldz8'/(n+'.c') for n in ['dispatch','ldso']}
              | {'soutfmt.c': ROOT/'tools/asz8k/src/soutfmt.c'}, '/bin/ldz8')
    for path in (CMD/'cpp').iterdir():
        if path.is_file(): extra['usr/src/cpp/'+path.name] = path
    step('cpp-parser', '/usr/src/cpp', ['/bin/yacc cpy.y'])
    # y.tab.c is generated inside Unix; avoid staging a host-generated parser.
    path = WORK/'cpp.mk'
    path.write_text('all: cpp\ncpp.b: cpp.c\n\t/bin/cc -O -Dunix=1 -c cpp.c\ny.tab.c: cpy.y yylex.c /bin/yacc /usr/lib/yaccpar\n\t/bin/yacc cpy.y\ny.tab.b: y.tab.c yylex.c\n\t/bin/cc -O -Dunix=1 -c y.tab.c\ncpp: cpp.b y.tab.b\n\t/bin/cc -i cpp.b y.tab.b -o cpp\n')
    extra['usr/src/cpp/makefile'] = path
    extra['usr/src/cppprobe.c']=ROOT/'tools/native-cc/cppprobe.c'
    step('cpp', '/usr/src/cpp', ['/bin/rm -f cpp.b y.tab.b', '/bin/make', '/bin/cp cpp /lib/cpp',
        '/bin/cc -O -i /usr/src/cppprobe.c -o /tmp/cppprobe','/tmp/cppprobe'])

    text = (ROOT/'tools/Makefile').read_text()
    names = re.search(r'LIBV7_NAMES = (.*?)\nLIBV7_OBJS',text,re.S)[1].replace('\\\n',' ').split()
    syscalls = run([sys.executable, ROOT/'tools/libc/split-syscalls.py', '--names']).stdout.decode().split()
    objects = [n+'.b' for n in names] + ['setjmp.b'] + [n+'.b' for n in syscalls] + ['float.b','softfp.b','epu.b','arith.b','csv.b']
    libobjects = objects[:]
    if sout:
        objects += ['object.b','soutfmt.b']; libobjects=objects[:]
        first = ['nlist.b','object.b','soutfmt.b']
        libobjects = first + [n for n in objects if n not in first]
        for path in [ROOT/'tools/sout-utils'/n for n in ('object.h','object.c')]+[
                ROOT/'tools/asz8k/src'/n for n in ('soutfmt.h','soutfmt.c')]:
            extra['usr/src/libc/'+path.name]=path
    rules = ['all: libc.a']
    for name in names:
        options = [ROOT/'v7z8000/usr/src/libc'/part/(name+'.c') for part in ['stdio','gen']]
        options.append(ROOT/'tools/libc'/(name+'.c'))
        source = next(p for p in options if p.exists())
        if sout and name=='nlist': source=ROOT/'tools/sout-utils/nlist.c'
        extra['usr/src/libc/'+name+'.c'] = source
        rules += [name+'.b: '+name+'.c'+(' object.h soutfmt.h' if sout and name=='nlist' else ''), '\t/bin/cc -O -Dunix=1 -c '+name+'.c']
    if sout:
        for name in ('object','soutfmt'):
            rules += [name+'.b: '+name+'.c object.h soutfmt.h','\t/bin/cc -O -Dunix=1 -c '+name+'.c']
    for name in ['setjmp'] + syscalls + ['float','softfp','epu','arith','csv']:
        if name == 'setjmp': source=ROOT/'tools/libc'/(name+'.az8')
        elif name=='arith': source=ROOT/'tools/arith.az8'
        elif name=='softfp':
            extra['usr/src/libc/softfp.c']=ROOT/'tools/fpe/glue.c'
            rules += ['softfp.b: softfp.c','\t/bin/cc -O -Dunix=1 -c softfp.c']
            continue
        else: source=ROOT/'tools/libv7'/(name+'.az8')
        extra['usr/src/libc/'+name+'.az8']=source
        rules += [name+'.b: '+name+'.az8','\t/bin/'+('asz8k -zc' if sout else 'az8')+' -o '+name+'.b '+name+'.az8']
    rules += ['libc.a: '+' '.join(objects),'\t/bin/rm -f libc.a']
    for i in range(0,len(objects),20):
        rules.append('\t/bin/ar qc libc.a '+' '.join(libobjects[i:i+20]))
        step('libc-'+str(i),'/usr/src/libc',['/bin/make '+' '.join(objects[i:i+20])])
    path=WORK/'libc.mk'; path.write_text('\n'.join(rules)+'\n'); extra['usr/src/libc/makefile']=path
    step('libc-archive','/usr/src/libc',['/bin/rm -f libc.a','/bin/make libc.a'])
    extra['usr/src/libc/libctest.c']=ROOT/'tools/libctest.c'
    step('libc-test','/usr/src/libc',['/bin/cc -O -c libctest.c',
        '/bin/ldz8 '+('-z ' if sout else '')+'-i -x /lib/crt0.b libctest.b libc.a -o libctest',
        '/bin/cp libctest /bin/libctest','/bin/libctest'])
    extra['usr/src/libc/crt0.az8']=ROOT/'tools/libc/crt0.az8'
    step('install-libc','/usr/src/libc',['/bin/'+('asz8k -zc' if sout else 'az8')+' -o crt0.b crt0.az8',
        '/bin/cp crt0.b /lib/crt0.b','/bin/cp libc.a /lib/libc.a'])
    # Close the bootstrap loop using native make, cpp, assembler, linker,
    # archive and parser output. Two-pass glue is staged as C source.
    for group,names in [('front','cgram xdefs scan pftn trees optim code local comm1 frontglue'),
                        ('back','reader local2 order match allo comm2 table backglue')]:
        for header in ['manifest','macdefs','mac2defs','mfile1','mfile2','common']:
            extra['usr/src/'+group+'/'+header]=PASSES/header
        if group=='front':
            step('front-parser','/usr/src/pcc',
                 ['/bin/yacc cgram.y','/bin/cp y.tab.c /usr/src/front/cgram.c'])
        makegroup(group,{n+'.c':None if n=='cgram' else PASSES/(n+'.c') for n in names.split()},
                  '/lib/'+group,'-DBUG4')
    step('install-compiler','/usr/src',
         ['/bin/cp front/front /lib/front','/bin/cp back/back /lib/back'])
    makegroup('oz8',{'oz8.c':PCC/'oz8.c'},'/lib/oz8')
    step('native-smoke','/tmp',['/bin/cc -O -i /usr/src/hello.c -o hello','/tmp/hello'])
    extra['usr/src/largeoff.c']=PCC/'test/regress/large_offsets.c'
    extra['usr/src/addrbyte.c']=PCC/'test/regress/address_bytes.c'
    extra['usr/src/common.c']=ROOT/'tools/ldz8/tests/common.c'
    extra['usr/src/nlprobe.c']=ROOT/'tools/ldz8/tests/nlist-only.c'
    extra['usr/src/pstat.c']=CMD/'pstat.c'
    step('native-offsets','/tmp',
         ['/bin/cc -O -i /usr/src/largeoff.c -o largeoff','/tmp/largeoff',
          '/bin/cc -O /usr/src/addrbyte.c -o addrbyte','/tmp/addrbyte',
          '/bin/cc -O -i /usr/src/addrbyte.c -o addrbyte','/tmp/addrbyte',
          '/bin/cc -O /usr/src/common.c -o common','/tmp/common',
          '/bin/cc -O -i /usr/src/common.c -o common','/tmp/common',
          '/bin/cc -O /usr/src/nlprobe.c -o nlprobe','/tmp/nlprobe',
          '/bin/cc -O -i /usr/src/nlprobe.c -o nlprobe','/tmp/nlprobe',
          '/bin/cc -O -Dunix=1 -Dz8000 -Dz8002 -c /usr/src/pstat.c'])
    step('native-libctest','/usr/src/libc',['/bin/rm -f libctest.b',
        '/bin/cc -O -i libctest.c -o /bin/libctest','/bin/libctest'])
    for tool in ['ar','cp','rm','mv','cmp']:
        install=['/bin/cp '+tool+' /bin/'+tool]
        if tool=='cp':
            install=['/bin/cp cp /bin/cp.new','/bin/mv /bin/cp.new /bin/cp']
        step('final-'+tool,'/usr/src/utils',
             ['/bin/cc -O -i -Dunix=1 '+tool+'.c -o '+tool]+install)
    for group, objects, tool in [
        ('yacc','y1.b y2.b y3.b y4.b','yacc'),
        ('make',makeobjects,'make')]:
        step('final-'+tool,'/usr/src/'+group,['/bin/cc -i '+objects+' -o '+tool,
                                             '/bin/cp '+tool+' /bin/'+tool])
    asname='asz8k' if sout else 'az8'
    finaltools=[('cc','/bin/cc'),(asname,'/bin/'+asname),('ldz8','/bin/ldz8'),('cpp','/lib/cpp')]
    if sout: finaltools += [(n,'/bin/'+n) for n in ('nm','size','strip')]
    for group, destination in finaltools:
        remove=group
        if group=='az8':remove+=' error.b init.b ins.b ioz8.b ps.b rel.b sdi.b sym.b scan.b'
        if group=='asz8k':remove+=' '+' '.join(p.stem+'.b' for p in sorted((ROOT/'tools/asz8k/src').glob('*.c')))
        step('final-'+group,'/usr/src/'+group,['/bin/rm -f '+remove,'/bin/make',
                                              '/bin/cp '+group+' '+destination])
        if group==asname:
            extra['usr/src/dc.c']=CMD/'dc/dc.c'
            extra['usr/src/dc.h']=CMD/'dc/dc.h'
            step('native-assembler','/usr/src',
                 ['/bin/cc -O -Dunix=1 -Dz8000 -Dz8002 -S dc.c',
                  '/bin/'+('asz8k -zc' if sout else 'az8')+' -o dc.b dc.az8'])
    step('final-smoke','/tmp',['/bin/cc -O -i /usr/src/hello.c -o hello','/tmp/hello','/bin/libctest'])
    for group,names in [('yacc','y1 y2 y3 y4'),('make','ident main doname misc files dosys y.tab')]:
        objects=[n+'.b' for n in names.split()]+(['object.b','soutfmt.b'] if sout and group=='make' else [])
        deps='dextern files' if group=='yacc' else 'defs'
        rules=['CFLAGS=-O -Dunix=1'+(' -DSOUT' if sout and group=='make' else ''),'all: '+group,group+': '+' '.join(objects),
               '\t/bin/cc -i '+' '.join(objects)+' -o '+group]
        for name in names.split():
            rules += [name+'.b: '+name+'.c '+deps,'\t/bin/cc $(CFLAGS) -c '+name+'.c']
        if group=='make':
            rules += ['files.b: /usr/include/arport.h','y.tab.c: gram.y /bin/yacc /usr/lib/yaccpar','\t/bin/yacc gram.y']
            if sout:
                rules += ['files.b: object.h soutfmt.h']
                for name in ('object','soutfmt'):
                    rules += [name+'.b: '+name+'.c object.h soutfmt.h','\t/bin/cc $(CFLAGS) -c '+name+'.c']
        path=WORK/(group+'.mk'); path.write_text('\n'.join(rules)+'\n')
        extra['usr/src/'+group+'/makefile']=path
    rules=['all: ar cp rm mv cmp']
    for name in ['ar','cp','rm','mv','cmp']:
        rules += [name+': '+name+'.c'+(' /usr/include/arport.h' if name=='ar' else ''),
                  '\t/bin/cc -O -Dunix=1 -i '+name+'.c -o '+name]
    path=WORK/'utils.mk';path.write_text('\n'.join(rules)+'\n');extra['usr/src/utils/makefile']=path
    rules=['all:']
    for group in ['utils','yacc','make','cc','cpp',asname,'ldz8','libc','front','back','oz8']+(['nm','size','strip'] if sout else []):
        rules.append('\tcd '+group+' && /bin/make all')
    path=WORK/'all.mk';path.write_text('\n'.join(rules)+'\n');extra['usr/src/makefile']=path
    step('make-all','/usr/src',['/bin/make all'])
    # The bootstrap compiler may predate code-generation changes. Rebuild
    # libc with the newly built compiler before validating its objects.
    # The legacy profile also compares the current cross-built reference.
    for i in range(0,len(libobjects),20):
        batch=' '.join(libobjects[i:i+20])
        step('final-libc-'+str(i),'/usr/src/libc',
             ['/bin/rm -f '+batch,'/bin/make '+batch])
    step('final-libc-archive','/usr/src/libc',
         ['/bin/rm -f libc.a','/bin/make libc.a','/bin/cp libc.a /lib/libc.a'])
    step('final-libc-test','/usr/src/libc',
         ['/bin/cc -O -i libctest.c -o /bin/libctest','/bin/libctest'])
    (WORK / 'steps.json').write_text(json.dumps(steps, indent=2) + '\n')
    staged={target:hashlib.sha256(path.read_bytes()).hexdigest()
            for target,path in extra.items() if target.startswith('usr/src/') and
            (Path(target).suffix in ('.c','.h','.y','.pd') or
             Path(target).name in ('makefile','defs','dextern','files','manifest',
                                   'macdefs','mac2defs','mfile1','mfile2','common')) and
            Path(target).name not in ('y.tab.c','cgram.c')}
    (WORK/'staged.json').write_text(json.dumps(staged,indent=2)+'\n')
    image(extra, WORK / 'hd.img', blocks=30000, inodes=2048, modes=modes,sout=sout)
    if not preserve: (WORK / 'results.json').write_text('[]\n')


def summarize(sout=True):
    fs=Filesystem(WORK/'hd.img')
    manifest=WORK/'staged.json'
    if sout: assert manifest.is_file(),'missing staged-source manifest'
    if manifest.is_file():
        for target,digest in json.loads(manifest.read_text()).items():
            assert hashlib.sha256(fs.read('/'+target)).hexdigest()==digest,target
    report={}
    tools=['cc','asz8k' if sout else 'az8','ldz8','make','ar','yacc','cp','rm','mv','cmp']
    if sout: tools += ['nm','size','strip']
    for path in ['/bin/'+n for n in tools] + [
            '/lib/front','/lib/back','/lib/oz8','/lib/cpp']:
        data=fs.read(path)
        dest=WORK/'native'/path.lstrip('/'); dest.parent.mkdir(parents=True,exist_ok=True)
        dest.write_bytes(data)
        report[path]=sizes(data,sout)
        report[path]['sha256']=hashlib.sha256(data).hexdigest()
    data=fs.read('/lib/libc.a'); (WORK/'native/lib/libc.a').write_bytes(data)
    (WORK/'native/lib/crt0.b').write_bytes(fs.read('/lib/crt0.b'))
    def members(data):
        assert data[:8]==b'!<arch>\n'
        out={}; offset=8
        while offset<len(data):
            header=data[offset:offset+60]; assert header[58:]==b'`\n'
            name=header[:16].decode().strip().rstrip('/'); size=int(header[48:58])
            out[name]=data[offset+60:offset+60+size]; offset+=60+size+(size&1)
        assert offset==len(data)
        return out
    actual=members(data)
    if sout:
        expected=members((ROOT/'tests/build/sout-cc/libc.a').read_bytes())
        assert set(actual)==set(expected)|{'object.b','soutfmt.b'}
        assert list(actual)[:3]==['nlist.b','object.b','soutfmt.b']
        for name,body in actual.items():
            assert struct.unpack_from('>H',body)[0]==0xe707,name
        report['format']='s.out'
        report['libc_members_validated']=len(actual)
    else:
        expected=members((PASSES/'libv7.a').read_bytes())
        assert list(actual)==list(expected) and actual==expected
        report['libc_members_identical']=len(actual)
    assert fs.read('/usr/src/pcc/y.tab.c')==(PCC/'cz8/cgram.c').read_bytes()
    report['native_parser_identical']=True
    (WORK/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS native environment summary:',WORK/'summary.json',flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup', action='store_true')
    parser.add_argument('--limit', type=int)
    parser.add_argument('--refresh', action='store_true', help='refresh staged sources while retaining native outputs')
    parser.add_argument('--reset-compiler', action='store_true',
                        help='with --refresh, restore bootstrap passes and discard their native objects')
    parser.add_argument('--from-step', type=int, help='resume at a zero-based step index')
    parser.add_argument('--summary', action='store_true')
    parser.add_argument('--sout',action='store_true',default=True,help=argparse.SUPPRESS)
    args = parser.parse_args()
    global WORK
    if args.sout: WORK=ROOT/'tests/build/native-environment-sout'
    if args.reset_compiler and not args.refresh:parser.error('--reset-compiler requires --refresh')
    if args.summary:
        summarize(args.sout); return
    if args.setup:
        setup(sout=args.sout)
    elif args.refresh:
        setup(True, args.reset_compiler,args.sout)
        if args.reset_compiler:
            steps=json.loads((WORK/'steps.json').read_text())
            args.from_step=next(i for i,s in enumerate(steps) if s['name']=='front-parser')
    records = json.loads((WORK / 'results.json').read_text())
    if args.from_step is not None:
        records = records[:args.from_step]
        (WORK / 'results.json').write_text(json.dumps(records, indent=2) + '\n')
    steps = json.loads((WORK / 'steps.json').read_text())[len(records):]
    if args.limit is not None:
        steps = steps[:args.limit]
    driver = SYS / 'test_driver'
    for step in steps:
        name = step['name']
        print('START', name, flush=True)
        start = time.monotonic()
        cycles = '300000000000' if name == 'final-asz8k' else '100000000000'
        with (WORK / (name + '.log')).open('wb') as log:
            result = subprocess.run(list(map(str, [driver, '-c', cycles,
                '-d', WORK / 'hd.img', '-o', WORK / 'next.img', '-P', WORK / (name + '.tsv'),
                '-i', 'runner %s %s\\n' % (step['plan'], step['directory']),
                '-w', 'NATIVE CC DONE', '-I', 'exit\\n', '-x', 'NATIVE CC DONE'])),
                cwd=SYS, stdout=log, stderr=subprocess.STDOUT, timeout=1800)
        output=(WORK / (name + '.log')).read_bytes()
        if result.returncode or b'NATIVE CC PASS\r\n' not in output:
            raise SystemExit('FAILED ' + name + ': see ' + str(WORK / (name + '.log')))
        if b'Absent RAM accesses: 0' not in output:
            raise SystemExit('Memory diagnostic in '+name+': '+str(WORK/(name+'.log')))
        (WORK / 'next.img').replace(WORK / 'hd.img')
        record = {'name': name, 'seconds': round(time.monotonic() - start, 2)}
        if name in ['ar', 'cp', 'rm', 'mv', 'cmp', 'yacc', 'make']:
            data = Filesystem(WORK / 'hd.img').read('/bin/' + name)
            (WORK / (name + '.out')).write_bytes(data)
            record.update(sizes(data,args.sout))
        records.append(record)
        (WORK / 'results.json').write_text(json.dumps(records, indent=2) + '\n')
        print('PASS', record, flush=True)
    if len(records)==len(json.loads((WORK/'steps.json').read_text())):
        summarize(args.sout)


if __name__ == '__main__':
    main()
