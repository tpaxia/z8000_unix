#!/usr/bin/env python3
"""Run V7 lex and awk's table generator in the guest."""
from pathlib import Path
import subprocess
import shutil
import sys
sys.dont_write_bytecode=True
from test import setup, SYS, Filesystem, ROOT, WORK, PCC
import importlib.util
spec=importlib.util.spec_from_file_location('allbuild',Path(__file__).with_name('build.py'))
allbuild=importlib.util.module_from_spec(spec);spec.loader.exec_module(allbuild)
CMD,compile_source=allbuild.CMD,allbuild.compile_source

work=WORK/'awk';work.mkdir(exist_ok=True)
yacc=ROOT/'tests/build/native-cc/yacc/yacc'
subprocess.run([str(yacc),'-d',str(CMD/'awk/awk.g.y')],cwd=work,check=True)
shutil.copyfile(work/'y.tab.h',work/'awk.h')
objects=[]
for name in ['proc','token']:
    obj,error=compile_source(CMD/'awk'/(name+'.c'),work)
    if error:raise SystemExit(name+': '+error)
    objects.append(obj)
subprocess.run(list(map(str,[PCC/'ldz8','-i','-x',ROOT/'tools/libc/crt0.b',*objects,ROOT/'tools/libv7.a','-o',work/'awkproc'])),check=True)
setup({'bin/awkproc':work/'awkproc'},
      '0 - /bin/lex /usr/src/cmd/awk/awk.lx.l\n'
      '0 - /bin/cp /tmp/lex.yy.c /tmp/awklex.c\n'
      '0 /tmp/proctab.c /bin/normal /bin/awkproc\n'
      '0 - /bin/lex /usr/src/cmd/struct/lextab.l\n'
      '0 /tmp/loop.i /lib/cpp -I/usr/include /usr/src/cmd/struct/3.loop.c\n')
with (WORK/'generate.log').open('wb') as log:
    r=subprocess.run(list(map(str,[SYS/'test_driver','-c','60000000000','-d',WORK/'hd.img','-o',WORK/'generated.img',
        '-i','runner /tmp/plan /tmp\\n','-w','NATIVE CC DONE','-I','exit\\n','-x','NATIVE CC DONE'])),cwd=SYS,stdout=log,stderr=subprocess.STDOUT,timeout=1800)
if r.returncode or b'NATIVE CC PASS\r\n' not in (WORK/'generate.log').read_bytes():raise SystemExit('guest generation failed')
fs=Filesystem(WORK/'generated.img')
(work/'lex.yy.c').write_bytes(fs.read('/tmp/awklex.c'))
(work/'proctab.c').write_bytes(fs.read('/tmp/proctab.c'))
(WORK/'beautify').mkdir(exist_ok=True)
(WORK/'beautify/lextab.c').write_bytes(fs.read('/tmp/lex.yy.c'))
(WORK/'structure/3.loop.i').write_bytes(fs.read('/tmp/loop.i'))
print('PASS guest awk/beautify scanners, procedure table and structure preprocessing')
