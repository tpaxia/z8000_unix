#!/usr/bin/env python3
"""Native portable-ar interoperability, mutation, linking and make lookups."""
import subprocess
import sys
sys.dont_write_bytecode = True
from build import ROOT, PCC, image, run
from selfhost import Filesystem

work = ROOT/'tests/build/portable-ar'
env = ROOT/'tests/build/native-environment'
work.mkdir(parents=True,exist_ok=True)
extra = {'lib/libc.a':ROOT/'tools/libv7.a', 'bin/runner':env/'runner',
         'bin/check':env/'check', 'bin/make':env/'make.out',
         'bin/cp':env/'cp.out', 'bin/rm':env/'rm.out',
         'bin/mv':env/'mv.out', 'bin/yacc':env/'yacc.out',
         'usr/lib/yaccpar':PCC/'yacc/yaccpar',
         'tmp/ar.c':ROOT/'v7z8000/usr/src/cmd/ar.c'}
fixtures = {'odd':b'abcde', 'new':b'updated!', 'helper.c':b'helper() { return 42; }\n',
            'main.c':b'main() { return helper()!=42; }\n',
            'big':bytes(i%251 for i in range(90001)),
            'archive.mk':b'all: lib.a(helper.b) lib.a((_helper))\n\t/bin/check\nlib.a(helper.b): helper.b\n\t/bin/ar r lib.a helper.b\nlib.a((_helper)):\n',
            'empty.mk':b'# use the native built-in rules\n',
            'tiny.y':b'%%\ns: ;\n%%\nyylex() { return 0; }\nyyerror(s) char *s; { return 0; }\n',
            'plain.az8':b'\t.text\n\t.globl _plain\n_plain:\n\tld r0,#42\n\tret\n'}
for name,data in fixtures.items():
    path=work/name; path.write_bytes(data); extra['tmp/'+name]=path
host=work/'host.a'; host.unlink(missing_ok=True)
run(['ar','qc',host,'odd','big'],cwd=work)
extra['tmp/host.a']=host
(work/'bad.a').write_bytes(host.read_bytes()[:12]);extra['tmp/bad.a']=work/'bad.a'
plan = [
    '0 - /bin/cc -O -i ar.c -o /bin/ar',
    '0 - /bin/ar rc fresh.a odd',
    '0 fresh.out /bin/ar p fresh.a odd', '0 - /bin/check same odd fresh.out',
    '0 - /bin/ar qc data.a odd big',
    '0 big.out /bin/ar p data.a big', '0 - /bin/check same big big.out',
    '0 host.out /bin/ar p host.a odd', '0 - /bin/check same odd host.out',
    '0 - /bin/cp new odd', '0 - /bin/ar r data.a odd',
    '0 - /bin/rm odd', '0 - /bin/ar x data.a odd', '0 - /bin/check same odd new',
    '0 - /bin/ar d data.a big', '0 listing /bin/ar t data.a',
    '0 verbose /bin/ar tv data.a',
    '1 - /bin/ar t bad.a',
    '0 - /bin/cc -O -c helper.c', '0 - /bin/ar qc lib.a helper.b',
    '0 - /bin/cc -i main.c lib.a -o linked', '0 - /tmp/linked',
    '0 - /bin/make -f archive.mk',
    '0 - /bin/make -q -f archive.mk lib.a(helper.b)',
    '0 - /bin/make -q -f archive.mk lib.a((_helper))',
    '0 - /bin/rm helper.b', '0 - /bin/make -f empty.mk helper.b tiny.b plain.b',
    '0 - /bin/check 0407 helper.b', '0 - /bin/check 0407 tiny.b',
    '0 - /bin/check 0407 plain.b',
]
(work/'plan').write_text('\n'.join(plan)+'\n');extra['tmp/plan']=work/'plan'
image(extra,work/'hd.img')
driver=ROOT/'v7z8000/usr/sys/build/test_driver'
result=subprocess.run(list(map(str,[driver,'-c','30000000000','-d',work/'hd.img',
    '-o',work/'saved.img','-i','runner\\n','-w','NATIVE CC DONE','-I','exit\\n',
    '-x','NATIVE CC PASS'])),cwd=ROOT/'v7z8000/usr/sys/build',capture_output=True,timeout=900)
(work/'run.log').write_bytes(result.stdout+result.stderr)
result.check_returncode()
fs=Filesystem(work/'saved.img')
assert fs.read('/tmp/listing')==b'odd\n'
assert b'odd' in fs.read('/tmp/verbose')
archive=fs.read('/tmp/data.a');(work/'native.a').write_bytes(archive)
assert archive.startswith(b'!<arch>\n')
assert run(['ar','p',work/'native.a','odd']).stdout==fixtures['new']
assert fs.read('/tmp/big.out')==fixtures['big']
print('PASS native portable archives: host interoperability, odd/large members, replace/extract/delete, link and make member/symbol lookup')
