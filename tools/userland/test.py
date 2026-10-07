#!/usr/bin/env python3
"""Create the full userland development image and exercise runnable packages."""
from pathlib import Path
import argparse
import json
import subprocess
import sys
sys.dont_write_bytecode=True
from assets import oldmembers
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/native-cc'))
from build import image, compile_c, PCC, run
from selfhost import Filesystem
WORK=ROOT/'tests/build/userland-all'
SYS=ROOT/'v7z8000/usr/sys/build'


def setup(extra=None, plan=None, emit_image=True, prepare_helpers=True, report_override=None):
    WORK.mkdir(parents=True,exist_ok=True)
    report=(json.loads((WORK/'report.json').read_text())
            if report_override is None else report_override)
    files={};modes={}
    # Native replacements are already supplied by build.image().
    replaced={'cc','ld','init','arcv','ranlib','adb','ps','pstat','dmesg','iostat'}
    for name,rec in report.items():
        if rec['status']!='built' or name.startswith('lib'):continue
        destination='bin/'+name
        if rec.get('kind')=='game':destination='usr/games/'+name
        if name in replaced:
            if name=='init':destination='etc/init.v7'
            else:continue
        if name in ('diffh','diff3','calendar','makekey'):destination='usr/lib/'+name
        if name.startswith('tab') and name!='tabs':destination='usr/lib/term/'+name
        if name in ('structure','beautify'):destination='usr/lib/struct/'+name
        if name in ('hunt','inv','mkey','deliv'):destination='usr/lib/refer/'+name
        if name in ('spell','spellin','spellout'):destination='usr/lib/'+name
        if name in ('lint1','lint2'):destination='usr/lib/'+name
        files[destination]=WORK/name/name
        modes[destination]=0o755
    native=ROOT/'tests/build/native-environment/native/bin'
    for name in ('make','yacc'):
        if 'bin/'+name not in files:files['bin/'+name]=native/name
    for name in ('m','mp','ln','plot','t300','t300s','t4014','t450','vt0','dbm','F77','I77'):
        p=WORK/('lib'+name)/('lib'+name+'.a')
        if p.exists():files['lib/lib'+name+'.a']=p
    for name in ('runner','normal','check'):
        if prepare_helpers:
            compile_c(ROOT/'tools/native-cc'/(name+'.c'),WORK/(name+'.b'))
            run([PCC/'ldz8','-x',ROOT/'tools/libc/crt0.b',WORK/(name+'.b'),ROOT/'tools/libv7.a','-o',WORK/name])
        files['bin/'+name]=WORK/name
    files['usr/lib/yaccpar']=PCC/'yacc/yaccpar'
    files['usr/lib/lex/ncform']=ROOT/'v7z8000/usr/lib/lex/ncform'
    for p in (ROOT/'v7z8000/usr/lib/tmac').rglob('*'):
        if p.is_file():files['usr/lib/tmac/'+str(p.relative_to(ROOT/'v7z8000/usr/lib/tmac'))]=p
    files['usr/lib/units']=ROOT/'v7z8000/usr/lib/units'
    files['usr/games/lib/fortunes']=ROOT/'v7unix/usr/games/lib/fortunes'
    for p in (ROOT/'v7unix/usr/games/quiz.k').rglob('*'):
        if p.is_file():files['usr/games/quiz.k/'+str(p.relative_to(ROOT/'v7unix/usr/games/quiz.k'))]=p
    files['usr/include/dbm.h']=ROOT/'v7z8000/usr/src/libdbm/dbm.h'
    for p in (ROOT/'v7z8000/usr/lib/font').iterdir():
        if p.is_file():files['usr/lib/font/'+p.name]=p
    for p in (ROOT/'v7z8000/usr/lib/learn').iterdir():
        if p.suffix=='.a':
            directory=WORK/'lessons'/p.stem;directory.mkdir(parents=True,exist_ok=True)
            for name,data in oldmembers(p):
                out=directory/name;out.write_bytes(data)
                files['usr/lib/learn/'+p.stem+'/'+name]=out
        elif p.is_file():files['usr/lib/learn/'+p.name]=p
    if report.get('lcount',{}).get('status')=='built':files['usr/lib/learn/lcount']=WORK/'lcount/lcount'
    if report.get('learntee',{}).get('status')=='built':files['usr/lib/learn/tee']=WORK/'learntee/learntee'
    modes.update({'usr/lib/learn/lcount':0o755,'usr/lib/learn/tee':0o755})
    for name in ('calendar','diff3','false','lookbib','lorder','man','nohup','plot','spell','struct'):
        files['bin/'+name]=ROOT/'v7unix/bin'/name
    files['bin/lint']=ROOT/'v7z8000/usr/src/cmd/lint/SHELL'
    for name in ('llib-lc','llib-lm','llib-port'):
        files['usr/lib/'+name]=ROOT/'v7z8000/usr/lib'/name
    files['usr/dict/words']=ROOT/'v7unix/usr/dict/words'
    for p in (ROOT/'v7unix/usr/man').rglob('*'):
        if p.is_file():files['usr/man/'+str(p.relative_to(ROOT/'v7unix/usr/man'))]=p
    for p in (ROOT/'v7z8000/usr/src').rglob('*'):
        if p.is_file():files['usr/src/'+str(p.relative_to(ROOT/'v7z8000/usr/src'))]=p
    def stage(name,text,destination):
        p=WORK/('fixture-'+name);p.write_text(text);files[destination]=p
    stage('passwd','root::0:0:Superuser:/:\n','etc/passwd')
    stage('group','root::0:\n','etc/group')
    stage('empty','','usr/tmp/.keep')
    stage('true','','bin/true')
    stage('spellhist','','usr/dict/spellhist')
    stage('learnplay','','usr/lib/learn/play/.keep')
    stage('learnlog','','usr/lib/learn/log/.keep')
    stage('input','alpha 1\nbeta 2\ngamma 3\n','tmp/input')
    files['tmp/runtime.c']=ROOT/'tools/userland/runtime.c'
    files['tmp/permissions.c']=ROOT/'tools/native-cc/userland-syscalls.c'
    for name in ('dbm','fortran'):files['tmp/'+name+'.c']=ROOT/'tools/userland'/(name+'.c')
    stage('changed','alpha 1\nbeta 4\ngamma 3\n','tmp/changed')
    stage('smoke.sh',(ROOT/'tools/userland/smoke.sh').read_text(),'tmp/smoke')
    stage('plan',plan or '0 - /bin/sh /tmp/smoke\n','tmp/plan')
    files.update(extra or {})
    if not emit_image:
        return files, modes
    image(files,WORK/'hd.img',blocks=60000,inodes=8192,modes=modes)


def main():
    parser=argparse.ArgumentParser();parser.add_argument('--setup',action='store_true');args=parser.parse_args()
    if args.setup:setup()
    with (WORK/'smoke.log').open('wb') as log:
        r=subprocess.run(list(map(str,[SYS/'test_driver','-c','60000000000','-d',WORK/'hd.img','-o',WORK/'next.img',
            '-i','runner /tmp/plan /tmp\\n','-w','NATIVE CC DONE','-I','exit\\n','-x','NATIVE CC DONE'])),
            cwd=SYS,stdout=log,stderr=subprocess.STDOUT,timeout=1800)
    text=(WORK/'smoke.log').read_bytes()
    if r.returncode or b'NATIVE CC PASS\r\n' not in text:raise SystemExit('FAIL: '+str(WORK/'smoke.log'))
    for diagnostic in (b'Inode table overflow', b'File table overflow', b'cannot execute',
                       b'cannot initialize hash table', b'panic:', b'Segmentation violation'):
        if diagnostic in text:raise SystemExit('Guest error: '+diagnostic.decode())
    fs=Filesystem(WORK/'next.img')
    expected={'awk.out':b'6\n','big.out':b'121932631112635269\n',
              'sqrt.out':b'9\n','misspell.out':b'zxqvnonword\n','spell.out':b''}
    for name,data in expected.items():
        if fs.read('/tmp/'+name)!=data:raise SystemExit('Incorrect output: '+name)
    fortune=fs.read('/tmp/fortune.out')
    if fortune not in (ROOT/'v7unix/usr/games/lib/fortunes').read_bytes().splitlines(keepends=True):
        raise SystemExit('Incorrect fortune output')
    (WORK/'next.img').replace(WORK/'hd.img')
    print('PASS full-userland package smoke tests')


if __name__=='__main__':main()
