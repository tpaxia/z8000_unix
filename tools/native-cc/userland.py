#!/usr/bin/env python3
"""Build original V7 essential commands with native make and cc; test the disk."""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import time
sys.dont_write_bytecode = True
from build import ROOT, PCC, compile_c, image, run
from selfhost import Filesystem

WORK = ROOT / 'tests/build/userland'
CMD = ROOT / 'v7z8000/usr/src/cmd'
ORIGINAL = ROOT / 'v7unix/usr/src/cmd'
SYS = ROOT / 'v7z8000/usr/sys/build'
NATIVE = ROOT / 'tests/build/native-environment/native'
COMMANDS = 'cat echo ls pwd mkdir rmdir ln cp mv rm chmod chown chgrp wc grep tail sort uniq tee cmp date sleep sync kill test ed'.split()
SUPPORT = 'ar make yacc'.split()


def audit():
    """Inventory source units, not a claim that each unit is one executable."""
    inventory = []
    for source in sorted(ORIGINAL.iterdir()):
        files = sorted(p for p in source.rglob('*') if p.is_file()) if source.is_dir() else [source]
        changed, missing = [], []
        for path in files:
            relative = path.relative_to(ORIGINAL)
            target = CMD / relative
            if not target.is_file(): missing.append(str(relative))
            elif path.read_bytes() != target.read_bytes(): changed.append(str(relative))
        name = source.stem if source.is_file() else source.name
        inventory.append({'source_unit': source.name, 'files': len(files),
                          'changed': changed, 'missing': missing,
                          'essential_batch': name in COMMANDS,
                          'development_support': name in SUPPORT + ['sh', 'cpp', 'cp', 'rm', 'mv', 'cmp']})
    for name in COMMANDS:
        assert (CMD / (name+'.c')).read_bytes() == (ORIGINAL / (name+'.c')).read_bytes(), name
    original_bin = sorted(p.name for p in (ROOT/'v7unix/bin').iterdir())
    installed = set(COMMANDS + SUPPORT + ['sh', 'cc'])
    report = {'source_units': inventory, 'essential_commands': COMMANDS,
              'unchanged_essential_sources': len(COMMANDS),
              'original_bin': original_bin,
              'original_bin_names_not_installed': sorted(set(original_bin)-installed),
              'z8000_tool_names': {'as': 'az8', 'ld': 'ldz8'}}
    (WORK/'audit.json').write_text(json.dumps(report, indent=2)+'\n')
    return report


def setup(reuse=False):
    WORK.mkdir(parents=True, exist_ok=True)
    audit()
    for name in SUPPORT:
        if not (NATIVE/'bin'/name).is_file():
            raise SystemExit('Build tools/native-cc/environment.py first: missing native ' + name)
    run(['cmake', '--build', SYS, '--target', 'kernel', 'test_driver'])
    run(['make', '-C', ROOT/'tools', 'libv7.a', 'libc/crt0.b', 'sh', 'init', 'v7mkfs'])
    extra = {'bin/'+name: NATIVE/'bin'/name for name in SUPPORT}
    modes = {}
    records = []
    if reuse:
        fs = Filesystem(WORK/'hd.img')
        records = json.loads((WORK/'results.json').read_text())[:len(COMMANDS)]
        assert [r['name'] for r in records] == COMMANDS, 'complete the command builds first'
        assert fs.read('/lib/libc.a') == (ROOT/'tools/libv7.a').read_bytes(), 'libc changed: use --setup'
        assert fs.read('/lib/crt0.b') == (ROOT/'tools/libc/crt0.b').read_bytes(), 'startup changed: use --setup'
        saved = WORK/'compiled'
        saved.mkdir(exist_ok=True)
        for name in COMMANDS:
            assert fs.read('/usr/src/cmd/'+name+'.c') == (CMD/(name+'.c')).read_bytes(), name
            output = saved/name
            output.write_bytes(fs.read('/usr/src/cmd/'+name))
            extra['usr/src/cmd/'+name] = output
            modes['usr/src/cmd/'+name] = 0o755
    extra['usr/lib/yaccpar'] = PCC/'yacc/yaccpar'
    for name in ['runner', 'check']:
        compile_c(ROOT/'tools/native-cc'/(name+'.c'), WORK/(name+'.b'))
        run([PCC/'ldz8', '-x', ROOT/'tools/libc/crt0.b', WORK/(name+'.b'),
             ROOT/'tools/libv7.a', '-o', WORK/name])
        extra['bin/'+name] = WORK/name
    rules = ['CC=/bin/cc', 'CFLAGS=-O -Dunix=1 -i', 'all: '+' '.join(COMMANDS)+' ar']
    steps = []
    def stage(name, content, destination):
        path = WORK/name
        path.write_text(content)
        extra[destination] = path
    def plan(name, directory, commands):
        filename = 'p%03d' % len(steps)
        stage(filename, '\n'.join('0 - '+c for c in commands)+'\n', 'tmp/'+filename)
        steps.append({'name':name, 'directory':directory, 'plan':'/tmp/'+filename})
    for name in COMMANDS:
        extra['usr/src/cmd/'+name+'.c'] = CMD/(name+'.c')
        rules += [name+': '+name+'.c', '\t$(CC) $(CFLAGS) '+name+'.c -o '+name]
        plan(name, '/usr/src/cmd', ['/bin/make '+name])
    extra['usr/src/cmd/ar.c'] = CMD/'ar.c'
    rules += ['ar: ar.c /usr/include/arport.h', '\t$(CC) $(CFLAGS) ar.c -o ar']
    # Install only after every original source builds; bootstrap tools stay usable.
    rules += ['install: all'] + ['\t./cp '+name+' /bin/'+name for name in COMMANDS+['ar']]
    rules += ['\t/bin/chmod 4755 /bin/mkdir /bin/rmdir /bin/mv']
    stage('cmd.mk', '\n'.join(rules)+'\n', 'usr/src/cmd/makefile')
    plan('install', '/usr/src/cmd', ['/bin/make install'])
    stage('passwd', 'root::0:0:Superuser:/:\n', 'etc/passwd')
    stage('group', 'root::0:\n', 'etc/group')
    stage('input', 'pear\napple\napple\nbanana\n', 'tmp/input')
    stage('sorted', 'apple\nbanana\npear\n', 'tmp/sorted')
    stage('edited', 'APPLE\nbanana\npear\n', 'tmp/edited')
    stage('tail', 'banana\npear\n', 'tmp/tail')
    stage('ed.script', '1s/apple/APPLE/\nw /tmp/edited.out\nq\n', 'tmp/ed.script')
    stage('main.c', '#include <stdio.h>\nmain(){if(answer()!=42)return(1);puts("USERLAND BUILD OK");return(0);}\n', 'usr/src/demo/main.c')
    stage('answer.c', 'answer(){return(42);}\n', 'usr/src/demo/answer.c')
    stage('grammar.y', '%{\n#include <stdio.h>\n%}\n%%\nstart: = { puts("USERLAND YACC OK"); };\n%%\nyylex(){return(0); }\nyyerror(s) char *s; {return(0); }\nmain(){return(yyparse()); }\n', 'usr/src/demo/grammar.y')
    stage('demo.mk', '''all: demo parser
demo: main.b libanswer.a
	/bin/cc -i main.b libanswer.a -o demo
main.b: main.c
	/bin/cc -O -c main.c
answer.b: answer.c
	/bin/cc -O -c answer.c
libanswer.a: answer.b
	/bin/ar rc libanswer.a answer.b
y.tab.c: grammar.y /usr/lib/yaccpar
	/bin/yacc grammar.y
y.tab.b: y.tab.c
	/bin/cc -O -c y.tab.c
parser: y.tab.b
	/bin/cc -i y.tab.b -o parser
clean:
	/bin/rm -f main.b answer.b libanswer.a demo y.tab.c y.tab.b parser
''', 'usr/src/demo/makefile')
    extra['usr/src/demo/syscalls.c'] = ROOT/'tools/native-cc/userland-syscalls.c'
    # Original V7 errexit also exits inside failing if conditions.
    stage('smoke.sh', '''PATH=/bin
export PATH || exit 1
cd /tmp || exit 1
mkdir probe || exit 1
cp input probe/data || exit 1
ln probe/data probe/link || exit 1
cmp probe/data probe/link || exit 1
mv probe/link probe/moved || exit 1
chmod 600 probe/data || exit 1
chown 0 probe/data || exit 1
chgrp 0 probe/data || exit 1
ls -l probe > listing || exit 1
grep data listing > found || exit 1
pwd > cwd || exit 1
sort input | uniq | tee unique || exit 1
cmp unique sorted || exit 1
tail -2 unique > last || exit 1
cmp last tail || exit 1
wc -l unique > count || exit 1
grep 3 count > counted || exit 1
echo hello > greeting || exit 1
grep hello greeting > greeted || exit 1
cat unique > copied || exit 1
cmp copied sorted || exit 1
ed - unique < ed.script || exit 1
cmp edited.out edited || exit 1
/bin/test -f probe/data || exit 1
/bin/test -d probe || exit 1
if /bin/test -f /tmp/missing; then exit 1; fi
if cmp input sorted; then exit 1; fi
if mkdir probe; then exit 1; fi
rm probe/moved probe/data || exit 1
rmdir probe || exit 1
/bin/test ! -d probe || exit 1
sleep 1 || exit 1
sleep 100 &
pid=$!
kill $pid
status=$?
if /bin/test $status -ne 0 -a $status -ne 143; then exit 1; fi
wait
status=$?
if /bin/test $status -ne 0 -a $status -ne 143; then exit 1; fi
date > date.out || exit 1
/bin/test -s date.out || exit 1
sync || exit 1
echo USERLAND COMMANDS OK || exit 1
''', 'tmp/smoke')
    plan('commands', '/tmp', ['/bin/sh /tmp/smoke'])
    plan('native-project', '/usr/src/demo', ['/bin/make clean', '/bin/make',
        '/usr/src/demo/demo', '/usr/src/demo/parser', '/bin/make', '/bin/cc -O -i syscalls.c -o syscalls',
        '/usr/src/demo/syscalls', '/bin/cc -O syscalls.c -o syscallsn',
        '/usr/src/demo/syscallsn'])
    image(extra, WORK/'hd.img', blocks=24000, inodes=1024, modes=modes)
    (WORK/'steps.json').write_text(json.dumps(steps,indent=2)+'\n')
    (WORK/'results.json').write_text(json.dumps(records,indent=2)+'\n')


def summarize():
    fs = Filesystem(WORK/'hd.img')
    sizes = {}
    for name in COMMANDS:
        data = fs.read('/bin/'+name)
        assert fs.read('/usr/src/cmd/'+name+'.c') == (CMD/(name+'.c')).read_bytes(), name
        assert data == fs.read('/usr/src/cmd/'+name), name
        header = struct.unpack('>8H', data[:16])
        assert header[0] == 0o411
        sizes[name] = dict(zip(['text','data','bss'],header[1:4]))
        sizes[name]['source_sha256'] = hashlib.sha256((CMD/(name+'.c')).read_bytes()).hexdigest()
    for actual, expected in [('unique','sorted'),('edited.out','edited'),('last','tail')]:
        assert fs.read('/tmp/'+actual) == fs.read('/tmp/'+expected)
    assert fs.read('/tmp/cwd') == b'/tmp\n'
    assert fs.read('/tmp/greeting') == b'hello\n'
    assert int(fs.read('/tmp/count').split()[0]) == 3
    assert b'-rw-------' in fs.read('/tmp/listing')
    assert fs.read('/usr/src/demo/libanswer.a').startswith(b'!<arch>\n')
    assert b'USERLAND BUILD OK' in (WORK/'native-project.log').read_bytes()
    assert b'USERLAND YACC OK' in (WORK/'native-project.log').read_bytes()
    assert (WORK/'native-project.log').read_bytes().count(b'USERLAND SYSCALLS OK\r\n') == 2
    assert b'USERLAND COMMANDS OK' in (WORK/'commands.log').read_bytes()
    (WORK/'summary.json').write_text(json.dumps(sizes,indent=2)+'\n')
    print('PASS',len(COMMANDS),'unchanged V7 commands, native make/archive build and syscall checks',flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--setup',action='store_true')
    parser.add_argument('--reuse-commands',action='store_true',
                        help='recreate the test disk, keeping verified command binaries when sources/libc/startup match')
    parser.add_argument('--audit',action='store_true')
    parser.add_argument('--limit',type=int)
    args = parser.parse_args()
    WORK.mkdir(parents=True,exist_ok=True)
    if args.audit:
        audit(); print(WORK/'audit.json'); return
    if args.setup and args.reuse_commands: parser.error('choose --setup or --reuse-commands')
    if args.setup or args.reuse_commands: setup(args.reuse_commands)
    steps=json.loads((WORK/'steps.json').read_text())
    records=json.loads((WORK/'results.json').read_text())
    pending=steps[len(records):]
    if args.limit is not None: pending=pending[:args.limit]
    for step in pending:
        name=step['name'];print('START',name,flush=True);start=time.monotonic()
        with (WORK/(name+'.log')).open('wb') as log:
            r=subprocess.run(list(map(str,[SYS/'test_driver','-c','20000000000',
                '-d',WORK/'hd.img','-o',WORK/'next.img','-i',
                'runner %s %s\\n'%(step['plan'],step['directory']),
                '-w','NATIVE CC DONE','-I','exit\\n','-x','NATIVE CC DONE'])),
                cwd=SYS,stdout=log,stderr=subprocess.STDOUT,timeout=600)
        if r.returncode or b'NATIVE CC PASS' not in (WORK/(name+'.log')).read_bytes():
            raise SystemExit('FAILED '+name+': '+str(WORK/(name+'.log')))
        (WORK/'next.img').replace(WORK/'hd.img')
        records.append({'name':name,'seconds':round(time.monotonic()-start,2)})
        (WORK/'results.json').write_text(json.dumps(records,indent=2)+'\n')
        print('PASS',records[-1],flush=True)
    if len(records)==len(steps): summarize()


if __name__=='__main__': main()
