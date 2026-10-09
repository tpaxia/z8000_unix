#!/usr/bin/env python3
"""Exercise the separate guest EPU service, in combined and split I/D."""
from pathlib import Path
import subprocess
import sys

root=Path(__file__).resolve().parents[1]
tools=root/'tools';pcc=root/'PCC-z8000/z8000'
build=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else root/'v7z8000/usr/sys/build'
work=root/'tests/build/fpe';work.mkdir(parents=True,exist_ok=True)
(work / 'asz8k.pd').write_bytes((tools / 'asz8k/src/asz8k.pd').read_bytes())
def run(args,**kw):
    p=subprocess.run(list(map(str,args)),capture_output=True,**kw)
    if p.returncode: print((p.stdout+p.stderr).decode(errors='replace'))
    p.check_returncode();return p.stdout
run(['make','-C',tools,'libv7.a','libc/crt0.b','sh','init','v7mkfs'])
cases=['float_add','float_general','float_storage','float_vectors',
       'float_ops_vectors','float_convert_vectors']
sources=[pcc/'test/regress'/(n+'.c') for n in cases]+[tools/'fpetest.c']
(work/'probe.az8').write_bytes((tools/'fpe/probe.az8').read_bytes())
run([tools.parent / 'tests/build/asz8k-host/asz8k', '-c','-o','probe.b','probe.az8'],cwd=work)
names=[]
for i,src in enumerate(sources):
    name='t'+str(i)
    pre=run(['cpp','-nostdinc','-undef','-I'+str(root/'v7z8000/usr/include'),src])
    (work/(name+'.az8')).write_bytes(run([pcc/'cz8/cz8'],input=pre))
    run([tools.parent / 'tests/build/asz8k-host/asz8k', '-c','-o',name+'.b',name+'.az8'],cwd=work)
    for split in [False,True]:
        exe=name+('i' if split else 'n');names.append(exe)
        run([tools.parent / 'tests/build/ldz8-host/ldz8',*(['-i'] if split else []),'-x',tools/'libc/crt0.b',
             work/(name+'.b'),work/'probe.b',tools/'libv7.a','-o',work/exe])
runner='''#include <stdio.h>
char *names[]={NAMES,0};
main(){int i,p,s,bad;char *av[2];bad=0;
for(i=0;names[i];i++){p=fork();if(p==0){av[0]=names[i];av[1]=0;execve(names[i],av,0);exit(127);}
if(p<0 || wait(&s)!=p || s){printf("EPU FAIL %s %d\\n",names[i],s);bad++;}}
printf("EPU RESULT %d\\n",bad);return bad;}
'''.replace('NAMES',','.join('"/bin/'+n+'"' for n in names))
(work/'runner.c').write_text(runner)
pre=run(['cpp','-nostdinc','-I'+str(root/'v7z8000/usr/include'),work/'runner.c'])
(work/'runner.az8').write_bytes(run([pcc/'cz8/cz8'],input=pre))
run([tools.parent / 'tests/build/asz8k-host/asz8k', '-c','-o','runner.b','runner.az8'],cwd=work)
run([tools.parent / 'tests/build/ldz8-host/ldz8','-x',tools/'libc/crt0.b',work/'runner.b',tools/'libv7.a','-o',work/'runner'])
files='\n'.join(n+' ---755 0 0 '+str(work/n) for n in names+['runner'])
(work/'proto').write_text(f'''boot
1600 96
d--755 0 0
bin d--755 0 0
sh ---755 0 0 {tools}/sh
{files}
$
dev d--755 0 0
console c--644 0 0 0 0
tty c--644 0 0 2 0
$
etc d--755 0 0
init ---755 0 0 {tools}/init
$
tmp d--777 0 0
$
$
''')
run([tools/'v7mkfs',work/'hd.img',work/'proto'])
p=subprocess.run([str(build/'test_driver'),'-c','1800000000','-d',str(work/'hd.img'),
    '-i','runner\\n','-w','EPU RESULT','-I','exit\\n','-x','EPU RESULT 0'],
    cwd=build,capture_output=True,timeout=120)
(work/'run.log').write_bytes(p.stdout+p.stderr)
print(p.stdout.decode(errors='replace'))
p.check_returncode()
if b'EPU FAIL' in p.stdout or b'fpe: FAIL' in p.stdout:raise SystemExit(1)
print('EPU: arithmetic vectors and process/signal tests passed in both I/D layouts')
