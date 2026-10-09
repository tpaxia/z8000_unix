#!/usr/bin/env python3
"""Build the V7 command inventory for Z8000 and record every result."""
from pathlib import Path
import argparse
import json
import re
import shutil
import struct
import subprocess
import sys
sys.dont_write_bytecode = True
from assets import oldmembers
ROOT = Path(__file__).resolve().parents[2]
CMD = ROOT/'v7z8000/usr/src/cmd'
PCC = ROOT/'PCC-z8000/z8000'
WORK = ROOT/'tests/build/userland-all'
REPLACED = {
    'cc':'PCC Z8000 cc/front/back replace the PDP-11 compiler driver.',
    'ld':'ldz8 implements the Z8000 object format.',
    'arcv':'Native ar already uses portable ASCII archives.',
    'ranlib':'ldz8 uses unindexed portable archives; the PDP-11 index is not used.',
}
UNPORTED = {
    'bas':'PDP-11 assembly interpreter; needs a Z8000 implementation.',
    'roff':'PDP-11 assembly formatter; nroff is available.',
    'factor':'PDP-11 assembly; needs a Z8000 implementation.',
    'primes':'PDP-11 assembly; needs a Z8000 implementation.',
    'f77':'The original backend emits PDP-11 code and requires the Ritchie second pass.',
    'chess':'The move generators and game control include PDP-11 assembly.',
}
LIBRARIES = ['m','mp','ln','plot','t300','t300s','t4014','t450','vt0','dbm','F77','I77']


def plot_sources(name, directory):
    """Extract the original PDP-11 source archive, preserving its C files."""
    sources=[]
    for member,data in oldmembers(ROOT/'v7z8000/usr/src/libplot'/(name+'.c.a')):
        path=directory/member;path.write_bytes(data)
        if path.suffix=='.c':sources.append(path)
    if not (directory/'con.h').exists():
        shutil.copyfile(ROOT/'v7z8000/usr/src/libplot/con.h',directory/'con.h')
    return sources


def invoke(args, log, cwd, data=None):
    r = subprocess.run(list(map(str,args)), input=data, capture_output=True, cwd=cwd)
    with log.open('ab') as f: f.write(r.stderr)
    return r


def compile_source(source, directory, flags=()):
    name = source.stem
    (directory/'asz8k.pd').write_bytes((ROOT/'tools/asz8k/src/asz8k.pd').read_bytes())
    log = directory/(name+'.log')
    log.write_bytes(b'')
    r = invoke(['cc','-E','-x','c','-nostdinc','-undef','-Dz8000','-Dz8002','-Dunix=1',
        '-I'+str(directory),'-I'+str(source.parent),'-I'+str(ROOT/'v7z8000/usr/include'),*flags,source],log,directory)
    if r.returncode: return None,'preprocess'
    preprocessed=r.stdout
    if source==CMD/'struct/3.loop.c' and (directory/'3.loop.i').exists():
        preprocessed=(directory/'3.loop.i').read_bytes()
    r = invoke([PCC/'cz8/cz8'],log,directory,preprocessed)
    (directory/(name+'.az8')).write_bytes(r.stdout)
    if r.returncode: return None,'compile'
    obj=directory/(name+'.b')
    r=invoke([ROOT/'tests/build/asz8k-host/asz8k','-c','-o',obj.name,name+'.az8'],log,directory)
    if r.returncode: return None,'assemble'
    return obj,None


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('names',nargs='*')
    args=parser.parse_args()
    WORK.mkdir(parents=True,exist_ok=True)
    subprocess.run(['make','-C',str(ROOT/'tools'),'libv7.a','libc/crt0.b'],check=True)
    subprocess.run(['make','-C',str(ROOT/'tools/asz8k')],check=True)
    subprocess.run(['make','-C',str(ROOT/'tools/ldz8')],check=True)
    hosttab=WORK/'mktab'
    subprocess.run(list(map(str,['cc','-std=gnu89','-O2','-w',
        '-I'+str(ROOT/'tools/sout-utils'),'-I'+str(ROOT/'tools/asz8k/src'),
        ROOT/'tools/userland/mktab.c',ROOT/'tools/sout-utils/object.c',
        ROOT/'tools/asz8k/src/soutfmt.c','-o',hosttab])),check=True)
    report={}
    if (WORK/'report.json').exists(): report=json.loads((WORK/'report.json').read_text())
    specs = {p.stem: {'sources':[p]} for p in sorted(CMD.glob('*.c'))}
    for source in sorted((ROOT/'v7z8000/usr/src/games').glob('*.c')):
        specs[source.stem]={'sources':[source],'game':True}
    specs['make']={'sources':[CMD/'make'/(n+'.c') for n in ['ident','main','doname','misc','files','dosys']],
                   'grammar':CMD/'make/gram.y'}
    specs['yacc']={'sources':[CMD/'yacc'/('y'+str(n)+'.c') for n in range(1,5)]}
    for name in ['bc','egrep','expr']:
        specs[name]={'sources':[], 'grammar':CMD/(name+'.y')}
    simple = {'sed':['sed0','sed1'], 'tar':['tar'], 'dc':['dc'],
              'prep':['prep0','prep1','prep2'], 'm4':['m4'],
              'lex':['lmain','sub1','sub2','header'],
              'ratfor':['r0','r1','r2','rio','rlook','rlex'],
              'learn':['copy','dounit','learn','list','mem','makpipe','maktee','mysys',
                       'selsub','selunit','start','whatnow','wrapup']}
    for name, names in simple.items():
        specs[name]={'sources':[CMD/name/(n+'.c') for n in names]}
    for name, grammar in [('m4','m4y.y'),('lex','parser.y'),('ratfor','r.g')]:
        specs[name]['grammar']=CMD/name/grammar
    for name in ['eqn','neqn']:
        specs[name]={'sources':sorted((CMD/name).glob('*.c')), 'grammar':CMD/name/'e.y',
                     'header':'e.def','flags':['-DNEQN'] if name=='neqn' else []}
    specs['tbl']={'sources':sorted(p for p in (CMD/'tbl').glob('*.c') if p.name!='t..c')}
    specs['structure']={'sources':sorted(p for p in (CMD/'struct').glob('[0-4].*.c') if not p.name.endswith('.test.c'))+[CMD/'struct/main.c']}
    specs['beautify']={'sources':[CMD/'struct/tree.c',WORK/'beautify/lextab.c',CMD/'struct/bdef.c'],
                       'grammar':CMD/'struct/beauty.y','flags':['-I'+str(CMD/'struct')], 'libs':['ln']}
    for name in ['spell','spellin','spellout']:
        specs[name]={'sources':[CMD/'spell'/(name+'.c')]}
    for name in ['enroll','xget','xsend']:
        specs[name]={'sources':[CMD/'xsend'/(name+'.c'),CMD/'xsend/lib.c'], 'libs':['mp']}
    for name in ['lcount','learntee']:
        specs[name]={'sources':[CMD/'learn'/('tee.c' if name=='learntee' else 'lcount.c')]}
    specs['vplot']={'sources':[CMD/'plot/vplot.c',CMD/'plot/chrtab.c']}
    groups={
      'mkey':'mkey1 mkey2 mkey3 deliv2', 'inv':'inv1 inv2 inv3 inv5 inv6 deliv2',
      'hunt':'hunt1 hunt2 hunt3 hunt5 hunt6 hunt7 hunt8 hunt9 refer3 glue5 glue4 shell deliv2',
      'refer':'glue1 glue2 glue3 glue4 glue5 refer0 refer1 refer2 refer4 refer5 refer6 refer7 refer8 hunt2 hunt3 hunt5 hunt6 hunt7 hunt8 hunt9 mkey3 shell deliv2',
      'deliv':'deliv1 deliv2'}
    for name,names in groups.items(): specs[name]={'sources':[CMD/'refer'/(n+'.c') for n in names.split()]}
    specs['awk']={'sources':[CMD/'awk'/(n+'.c') for n in ['b','main','token','tran','lib','run','parse']]
                  +[WORK/'awk/y.tab.c',WORK/'awk/lex.yy.c',WORK/'awk/proctab.c'],
                  'flags':['-I'+str(CMD/'awk')], 'libs':['m']}
    specs['tp']={'sources':[CMD/'tp'/('tp'+str(n)+'.c') for n in range(4)]}
    specs['adb']={'sources':[CMD/'adb'/(n+'.c') for n in
        'access command expr findfn format input opset main message output pcs print runpcs setup sym'.split()]}
    specs['lint2']={'sources':[CMD/'lint/lpass2.c'],'flags':['-I'+str(CMD/'mip')]}
    specs['lint1']={'sources':[CMD/'mip'/(n+'.c') for n in ['xdefs','scan','comm1','pftn','trees','optim']]+[CMD/'lint/lint.c'],
                    'grammar':CMD/'mip/cgram.y','flags':['-I'+str(CMD/'lint'),'-I'+str(CMD/'mip')]}
    for name in ['nroff','troff']:
        names='n1 n2 n3 n4 n5 n6 n7 n8 n9 n10 ni nii ntab hytab suftab'.split()
        if name=='troff':names=[{'n6':'t6','n10':'t10','ntab':'tab3'}.get(n,n) for n in names]
        specs[name]={'sources':[CMD/'troff'/(n+'.c') for n in names],
                     'flags':['-DNROFF'] if name=='nroff' else []}
    for source in sorted((CMD/'troff/term').glob('tab*.c')):
        specs[source.stem]={'sources':[source],'data_only':True}
    common='cpmv expfile gename getpwinfo index lastpart prefix shio ulockf xqt'
    uu={
      'uucp':'uucp gwd '+common+' chkpth getargs logent versys',
      'uux':'uux gwd '+common+' chkpth getargs getprm versys',
      'uuxqt':'uuxqt '+common+' getprm gnamef logent',
      'uucico':'cico cntrl conn pk0 pk1 gio sdmail pkon '+common+' anlwrk chkpth getargs gnamef gnsys gnxseq imsg logent sysacct systat',
      'uulog':'uulog prefix xqt ulockf gnamef',
      'uuclean':'uuclean gnamef prefix sdmail getpwinfo'}
    for name,names in uu.items():
        specs[name]={'sources':[CMD/'uucp'/(n+'.c') for n in names.split()]}
    specs['graph']['libs']=['plot','m']
    specs['spline']['libs']=['m']
    for name in ['t300','t300s','t4014','t450']:
        specs['tek' if name=='t4014' else name]={'sources':[CMD/'plot/driver.c'],'libs':[name,'m']}
    unknown=set(args.names)-set(specs)-set(UNPORTED)
    if unknown:parser.error('unknown command(s): '+', '.join(sorted(unknown)))
    for name in LIBRARIES:
        directory=WORK/('lib'+name);directory.mkdir(exist_ok=True)
        objects=[]
        archive=directory/('lib'+name+'.a');archive.unlink(missing_ok=True)
        sources=ROOT/('v7z8000/usr/src/lib'+name)
        if name=='m':
            names='asin atan hypot jn j0 j1 pow fabs log sin sqrt tan tanh sinh exp floor'.split()
        elif name=='mp': names='pow gcd msqrt mult mdiv mout madd util'.split()
        elif name=='ln':
            sources=CMD/'lex/lib';names='main allprint reject yyless yywrap'.split()
        elif name=='dbm':names=['dbm']
        elif name=='F77':
            makefile=(sources/'Makefile').read_text()
            assignments=makefile[makefile.index('MISC ='):makefile.index('libF77.a :')]
            names=re.findall(r'([A-Za-z0-9_]+)\.o',assignments)+['cabs']
        elif name=='I77':names=re.findall(r'([A-Za-z0-9_]+)\.o',(sources/'mklib').read_text())
        else:names=[]
        sourcefiles=([sources/(n+'.c') for n in names] if names else plot_sources(name,directory))
        errors=[]
        for source in sourcefiles:
            obj,error=compile_source(source,directory,['-DSYLMX=300'] if name=='I77' else [])
            if error:errors.append(source.name+': '+error)
            else:objects.append(obj)
        if not errors:
            subprocess.run(['ar','cr',str(archive),*map(str,objects)],check=True)
        report['lib'+name]={'status':'built' if not errors else 'compile','errors':errors}
    for name,spec in specs.items():
        if args.names and name not in args.names: continue
        if name in REPLACED:
            report[name]={'status':'replaced','reason':REPLACED[name]}
            continue
        directory=WORK/name;directory.mkdir(exist_ok=True)
        output=directory/name;output.unlink(missing_ok=True)
        sources=list(spec['sources']);error=None
        if 'grammar' in spec:
            log=directory/'yacc.log';log.write_bytes(b'')
            r=invoke([ROOT/'tests/build/native-cc-sout/yacc/yacc','-d',spec['grammar']],log,directory)
            if r.returncode:error='yacc'
            else:
                sources.append(directory/'y.tab.c')
                if 'header' in spec:shutil.copyfile(directory/'y.tab.h',directory/spec['header'])
        if name in ('make','prof','nm','size','strip'):
            spec['flags']=spec.get('flags',[])+['-I'+str(ROOT/'tools/sout-utils'),'-I'+str(ROOT/'tools/asz8k/src')]
            sources += [ROOT/'tools/sout-utils/object.c',ROOT/'tools/asz8k/src/soutfmt.c']
        record={'sources':[str(p.relative_to(ROOT)) for p in spec['sources']], 'status':error or 'compiled'}
        record['kind']='terminal-table' if spec.get('data_only') else ('game' if spec.get('game') else 'command')
        objects=[]
        if not error:
            for source in sources:
                obj,error=compile_source(source,directory,spec.get('flags',[])+(['-I'+str(spec['grammar'].parent)] if 'grammar' in spec else []))
                if error:record['failed_source']=str(source.relative_to(ROOT));break
                objects.append(obj)
        if not error:
            log=directory/'link.log';log.write_bytes(b'')
            libs=[WORK/('lib'+n)/('lib'+n+'.a') for n in spec.get('libs',[])]
            startup=[] if spec.get('data_only') else [ROOT/'tools/libc/crt0.b']
            runtime=[] if spec.get('data_only') else [ROOT/'tools/libv7.a']
            r=invoke([ROOT/'tests/build/ldz8-host/ldz8',*(['-r'] if spec.get('data_only') else ['-i']),'-x',*startup,*objects,*libs,
                      *runtime,'-o',output],log,directory)
            with log.open('ab') as f:f.write(r.stdout)
            error='link' if r.returncode else None
            if not error and spec.get('data_only'):
                intermediate=directory/'table.so';output.replace(intermediate)
                r=invoke([hosttab,intermediate,output],log,directory)
                error='resource' if r.returncode else None
        record['status']=error or 'built'
        if not error:
            offset=2 if spec.get('data_only') else 28
            record['sizes']=dict(zip(['text','data','bss'],struct.unpack_from('>3H',output.read_bytes(),offset)))
        report[name]=record
        print(name,record['status'],flush=True)
        (WORK/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    for name,reason in UNPORTED.items():report[name]={'status':'unported','reason':reason}
    valid=set(specs)|set(UNPORTED)|{'lib'+n for n in LIBRARIES}
    report={n:r for n,r in report.items() if n in valid}
    (WORK/'report.json').write_text(json.dumps(report,indent=2)+'\n')
    failed=[n for n,r in report.items() if r['status'] not in ('built','replaced','unported') and (not args.names or n in args.names)]
    if failed:raise SystemExit('Build failures: '+', '.join(failed))



if __name__=='__main__':main()
