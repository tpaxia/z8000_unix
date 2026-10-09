#!/usr/bin/env python3
"""Stage and monitor the native V7 makefiles; never compile on the host."""
from pathlib import Path
import argparse
import fcntl
import hashlib
import json
import struct
import subprocess
import sys
import time
sys.dont_write_bytecode=True
import test as packages
from assets import oldmembers
from object_format import sizes, terminal_sizes
ROOT=packages.ROOT
WORK=ROOT/'tests/build/userland-native-sout'
RECIPES=ROOT/'tools/userland/native'
NATIVE=ROOT/'tests/build/native-environment-sout/native'
SYS=packages.SYS


def setup(preserve=False, update_toolchain=False,sout=True):
    WORK.mkdir(parents=True,exist_ok=True)
    catalog=json.loads((RECIPES/'catalog.json').read_text())
    files,modes=packages.setup(emit_image=False,prepare_helpers=False,report_override={})
    for row in catalog:
        if row['kind']=='library':files.pop('lib/'+row['output'],None)
    # The 45 essential tools were already built inside Unix. No full-userland
    # executable or support library is copied from the cross-built image.
    essential=ROOT/'tests/build/userland-sout'
    seed=packages.Filesystem(essential/'hd.img')
    audit=json.loads((essential/'audit.json').read_text())
    seed_dir=WORK/'seed';seed_dir.mkdir(exist_ok=True)
    for name in audit['essential_commands']+['runner','normal','check']:
        p=seed_dir/name;p.write_bytes(seed.read('/bin/'+name));files['bin/'+name]=p
    for path in NATIVE.rglob('*'):
        if path.is_file():files[str(path.relative_to(NATIVE))]=path
    staged_sources={target:path for target,path in files.items()
        if target.startswith('usr/src/') or target.startswith('tmp/')}
    if preserve:
        fs=packages.Filesystem(WORK/'hd.img')
        def walk(number,path):
            offset=((number+15)//8)*512+((number+15)%8)*64
            permissions=int.from_bytes(fs.disk[offset:offset+2],'big')
            mode=permissions & 0o170000
            if mode==0o040000:
                data=fs.data(number)
                for i in range(0,len(data),16):
                    child=int.from_bytes(data[i:i+2],'big')
                    name=data[i+2:i+16].split(b'\0')[0].decode()
                    if child and name not in ('.','..'):
                        walk(child,path+'/'+name if path else name)
            elif mode==0o100000:
                dest=WORK/'saved'/path;dest.parent.mkdir(parents=True,exist_ok=True)
                dest.write_bytes(fs.data(number));files[path]=dest;modes[path]=permissions
        walk(2,'')
        # Restage current sources and harness inputs, retaining built artifacts.
        files.update(staged_sources)
    if update_toolchain:
        # Keep installed full-userland outputs paired with their native builds.
        # Refresh compiler tools and libc; changed packages rebuild separately.
        installed={row['destination'] for row in catalog if row.get('destination')}
        for path in NATIVE.rglob('*'):
            target=str(path.relative_to(NATIVE))
            if path.is_file() and target not in installed:files[target]=path
    # A refresh retains completed packages, but always replaces the harness
    # with the helpers from the verified essential-command image.
    for name in ('runner','normal','check'):
        files['bin/'+name]=seed_dir/name
    # Extract the original source-only plot archives. No target objects enter
    # the build directories; this is source staging, like unpacking a tape.
    for name in ('plot','t300','t300s','t4014','t450','vt0'):
        directory=WORK/'sources'/name;directory.mkdir(parents=True,exist_ok=True)
        seen=set()
        for member,data in oldmembers(ROOT/'v7z8000/usr/src/libplot'/(name+'.c.a')):
            p=directory/member;p.write_bytes(data)
            files['usr/src/libplot/'+name+'/'+member]=p;seen.add(member)
        if 'con.h' not in seen:files['usr/src/libplot/'+name+'/con.h']=ROOT/'v7z8000/usr/src/libplot/con.h'
    files['usr/src/build/makefile']=RECIPES/'makefile'
    files['usr/src/build/normal.c']=ROOT/'tools/native-cc/normal.c'
    for path in [ROOT/'tools/sout-utils'/n for n in ('object.c','object.h')]+[
            ROOT/'tools/asz8k/src/soutfmt.c',ROOT/'tools/asz8k/src/soutfmt.h',
            *[ROOT/'v7z8000/usr/src/cmd'/(n+'.c') for n in ('nm','size','strip')]]:
        files['usr/src/objutils/'+path.name]=path
    empty=WORK/'keep';empty.write_text('')
    for row in catalog:
        if row.get('destination'):
            files[str(Path(row['destination']).parent/'.keep')]=empty
    steps=[]
    def step(name,command):
        p=WORK/('p%03d'%len(steps));p.write_text('0 - '+command+'\n')
        files['tmp/'+p.name]=p
        steps.append({'name':name,'plan':'/tmp/'+p.name})
    files['usr/src/build/mktab.c']=ROOT/'tools/userland/mktab.c'
    step('mktab','/bin/cc -O -i -I/usr/src/objutils mktab.c /usr/src/objutils/object.c /usr/src/objutils/soutfmt.c -o /bin/mktab')
    for row in catalog:
        name=row['name'];recipe=RECIPES/(name+'.mk')
        text=recipe.read_text().replace('/bin/ldz8 -i','/bin/ldz8 -z -i')
        if name in ('file','prof','make'):
            text=text.replace('CFLAGS=','CFLAGS=-I/usr/src/objutils ')
        if name in ('prof','make'):
            text=text.replace(name+': ',name+': object.b soutfmt.b ',1)
            text=text.replace(' -o '+name,' object.b soutfmt.b -o '+name,1)
            for obj in ('object','soutfmt'):
                text+='\n'+obj+'.b: /usr/src/objutils/'+obj+'.c /usr/src/objutils/object.h /usr/src/objutils/soutfmt.h\n\t$(CC) $(CFLAGS) -c /usr/src/objutils/'+obj+'.c\n'
        if name.startswith('tab') and name!='tabs':
            text=text.replace(name+': '+name+'.b',name+': '+name+'.b /bin/mktab')
            text=text.replace('/bin/ldz8 -z -i -x '+name+'.b -o '+name,
                '/bin/ldz8 -z -r -x '+name+'.b -o table.so\n\t/bin/mktab table.so '+name+'\n\t/bin/rm -f table.so')
        if name in ('nm','size','strip'):
            objects=[name,'object','soutfmt']
            text='CC=/bin/cc\nCFLAGS=-O -Dunix=1 -Dz8000 -Dz8002 -I/usr/src/objutils\nall: '+name+'\n'
            text+=name+': '+' '.join(n+'.b' for n in objects)+'\n\t$(CC) -i -s '+' '.join(n+'.b' for n in objects)+' -o '+name+'\n'
            for obj in objects:
                text+=obj+'.b: /usr/src/objutils/'+obj+'.c /usr/src/objutils/object.h /usr/src/objutils/soutfmt.h\n\t$(CC) $(CFLAGS) -c /usr/src/objutils/'+obj+'.c\n'
            text+='install: all\n\t/bin/cp '+name+' /bin/ninstall\n\t/bin/mv /bin/ninstall /bin/'+name+' </dev/null\nclean:\n\t/bin/rm -f *.b '+name+'\n'
        recipe=WORK/(name+'.mk');recipe.write_text(text)
        files['usr/src/build/'+name+'/makefile']=recipe
        step(name,'/bin/make '+name)
    step('install','/bin/make install')
    (WORK/'steps.json').write_text(json.dumps(steps,indent=2)+'\n')
    if not preserve:(WORK/'results.json').write_text('[]\n')
    manifest={p:hashlib.sha256(s.read_bytes()).hexdigest() for p,s in files.items()}
    (WORK/'seed.json').write_text(json.dumps(manifest,indent=2)+'\n')
    packages.image(files,WORK/'hd.img',blocks=120000,inodes=16384,modes=modes,sout=sout)


def summarize(sout=True):
    fs=packages.Filesystem(WORK/'hd.img');report={}
    for row in json.loads((RECIPES/'catalog.json').read_text()):
        name=row['name'];data=fs.read('/usr/src/build/'+name+'/'+row['output'])
        rec={'kind':row['kind'],'sha256':hashlib.sha256(data).hexdigest()}
        if row['kind']!='library':
            terminal=row['name'].startswith('tab') and row['name']!='tabs'
            rec.update(terminal_sizes(data) if terminal else sizes(data))
            if terminal:rec['format']='V7 terminal resource'
        dest=row.get('destination')
        if row['kind']=='library':dest='lib/'+row['output']
        if dest:assert data==fs.read('/'+dest),name
        report[name]=rec
    (WORK/'summary.json').write_text(json.dumps(report,indent=2)+'\n')
    print('PASS native inventory:',len(report),'outputs built and installed',flush=True)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup',action='store_true')
    parser.add_argument('--limit',type=int)
    parser.add_argument('--refresh',action='store_true',help='restage recipes while retaining native outputs')
    parser.add_argument('--update-toolchain',action='store_true',
                        help='with --refresh, install the verified native environment tools')
    parser.add_argument('--summary',action='store_true')
    parser.add_argument('--sout',action='store_true',default=True,help=argparse.SUPPRESS)
    args=parser.parse_args()
    global WORK,NATIVE
    WORK=ROOT/'tests/build/userland-native-sout'
    NATIVE=ROOT/'tests/build/native-environment-sout/native'
    if args.update_toolchain and not args.refresh:
        parser.error('--update-toolchain requires --refresh')
    if args.setup:setup(sout=args.sout)
    elif args.refresh:setup(True,args.update_toolchain,args.sout)
    if args.summary:summarize(args.sout);return
    records=json.loads((WORK/'results.json').read_text())
    plan=json.loads((WORK/'steps.json').read_text())
    steps=plan[len(records):]
    if args.limit is not None:steps=steps[:args.limit]
    for step in steps:
        name=step['name'];print('START',name,flush=True);start=time.monotonic()
        logpath=WORK/(name+'.log')
        # Installing yacc makes generated-source dependencies newer. Native
        # make may rebuild those packages while installing the whole tree.
        # The complete lint front-end compile/link takes more than 200B
        # cycles with the shared assembler; do not cut off a valid link.
        cycles='2000000000000' if name=='install' else '400000000000'
        with logpath.open('wb') as log:
            result=subprocess.run(list(map(str,[SYS/'test_driver','-c',cycles,
                '-d',WORK/'hd.img','-o',WORK/'next.img',
                '-i','runner %s /usr/src/build\\n'%step['plan'],
                '-w','NATIVE CC DONE','-I','exit\\n','-x','NATIVE CC DONE'])),
                cwd=SYS,stdout=log,stderr=subprocess.STDOUT,timeout=3600)
        output=logpath.read_bytes()
        if result.returncode or b'NATIVE CC PASS\r\n' not in output:
            raise SystemExit('FAILED '+name+': '+str(logpath))
        (WORK/'next.img').replace(WORK/'hd.img')
        records.append({'name':name,'seconds':round(time.monotonic()-start,2)})
        (WORK/'results.json').write_text(json.dumps(records,indent=2)+'\n')
        print('PASS',records[-1],flush=True)
    if len(records)==len(plan):
        summarize(args.sout)
        packages.WORK=WORK
        sys.argv=[sys.argv[0]]
        packages.main()


if __name__=='__main__':
    WORK=ROOT/'tests/build/userland-native-sout'
    WORK.mkdir(parents=True,exist_ok=True)
    with (WORK/'run.lock').open('w') as lock:
        try:fcntl.flock(lock,fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:raise SystemExit('A native userland runner is already active')
        main()
