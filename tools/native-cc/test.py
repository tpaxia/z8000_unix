#!/usr/bin/env python3
"""Exercise the complete C pipeline, driver modes, and failures in Unix."""
import json
import subprocess
import sys
sys.dont_write_bytecode = True
from build import ROOT, PCC, WORK, BINUTILS, run, compile_c, image, module


def test(selected=(), sout=True):
    run(['cmake', '--build', ROOT / 'v7z8000/usr/sys/build',
         '--target', 'kernel', 'test_driver'])
    extra = {}
    for name in ['runner', 'check']:
        compile_c(ROOT / 'tools/native-cc' / (name + '.c'), WORK / (name + '.b'),sout=sout)
        run([ROOT/'tests/build/ldz8-host/ldz8', '-z', '-x', ROOT/'tests/build/sout-cc/crt0.b', WORK / (name + '.b'),
             ROOT/'tests/build/sout-cc/libc.a', '-o', WORK / name])
        extra['bin/' + name] = WORK / name
    fixtures = {
        'input.c': '''#include <stdio.h>
#include <probe.h>
#ifndef z8000
WRONG_TARGET
#endif
#ifndef z8002
WRONG_TARGET
#endif
#ifdef REMOVE
UNDEFINE_FAILED
#endif
long large = 100000L;
double value = 1.5;
main() {
    if (helper(BASE) + VALUE != 44) return 1;
    if (large * 3L != 300000L || value + 2.5 != 4.0) return 2;
    puts("DRIVER_OK");
    return 0;
}
''',
        'helper.c': 'helper(x) int x; { return x+2; }\n',
        'probe.h': '#define BASE 40\n',
        'badcpp.c': '#include <does_not_exist.h>\n',
        'badfront.c': 'main( {\n',
        'badasm.az8': '\t.text\n\tinvalid_instruction\n',
        'badlink.c': 'main() { return missing(); }\n',
        'front.c': '''main(argc, argv) int argc; char **argv; {
    char *getenv(), *value;
    value = getenv("CC_TEST");
    if (!value || strcmp(value, "inherited")) return 1;
    execv("/lib/front", argv);
    return 2;
}
''',
    }
    for name, source in fixtures.items():
        (WORK / name).write_text(source)
        extra['tmp/' + ('include/' if name == 'probe.h' else '') + name] = WORK / name
    compile_c(WORK / 'front.c', WORK / 'front.b',sout=sout)
    run([ROOT/'tests/build/ldz8-host/ldz8', '-z', '-x', ROOT/'tests/build/sout-cc/crt0.b', WORK / 'front.b',
         ROOT/'tests/build/sout-cc/libc.a', '-o', WORK / 'front'])
    # Install the environment-checking front-end wrapper as an executable.
    extra['bin/front'] = WORK / 'front'
    # A substitute optimizer makes driver failure propagation observable.
    (WORK / 'failopt.c').write_text('main() { return 1; }\n')
    compile_c(WORK / 'failopt.c', WORK / 'failopt.b',sout=sout)
    run([ROOT/'tests/build/ldz8-host/ldz8', '-z', '-x', ROOT/'tests/build/sout-cc/crt0.b', WORK / 'failopt.b',
         ROOT/'tests/build/sout-cc/libc.a', '-o', WORK / 'failopt'])
    extra['bin/oz8'] = WORK / 'failopt'
    reference = module('c2_reference', PCC / 'c2z8.py')
    # Generate the large optimizer fixture from source, without depending on
    # an earlier legacy assembler bootstrap's build directory.
    source=ROOT/'v7z8000/usr/src/cmd/cpp/cpp.c'
    pre=run(['cpp','-nostdinc','-undef','-Dz8000','-Dz8002','-Dunix=1',
             '-I'+str(source.parent),'-I'+str(ROOT/'v7z8000/usr/include'),source])
    assembly=run([PCC/'cz8/cz8'],input=pre.stdout).stdout.decode()
    assembly += ('! streamed fixture padding\n' * max(0, 1 + (65536-len(assembly))//27))
    assert len(assembly) > 65536
    (WORK / 'opt.az8').write_text(assembly)
    (WORK / 'opt.want').write_text(reference.compact(assembly))
    extra['tmp/opt.az8'] = WORK / 'opt.az8'
    extra['tmp/opt.want'] = WORK / 'opt.want'
    options = '-I/tmp/include -DVALUE=2 -DREMOVE -UREMOVE'
    plans = {}
    for mode in ['combined','split']:
        flag = '-i ' if mode == 'split' else ''
        plans['full-' + mode] = [
            '0 - /bin/cc ' + flag + options + ' -B/bin/ -t0 input.c helper.c -o result',
            '0 - /tmp/result', '0 - /bin/check ' + ('e711' if mode=='split' else 'e707') + ' result',
        ]
        plans['opt-' + mode] = [
            '0 - /bin/cc -O ' + flag + options + ' input.c helper.c -o result',
            '0 - /tmp/result', '0 - /bin/check ' + ('e711' if mode=='split' else 'e707') + ' result',
        ]
    extra['tmp/ccz8.c'] = PCC/'ccz8.c'
    plans['selfhost-driver'] = [
        '0 - /bin/cc -O -i -DTWOPASS ccz8.c -o cc',
        '0 - /tmp/cc -O -i '+options+' input.c helper.c -o result',
        '0 - /tmp/result', '0 - /bin/check e711 result',
    ]
    plans['stages'] = [
        '0 pre.i /bin/cc -E ' + options + ' input.c',
        '0 - /bin/check pre pre.i',
        '0 - /bin/cc -P ' + options + ' input.c',
        '0 - /bin/check pre input.i',
        '0 - /bin/cc -O -S ' + options + ' input.c helper.c',
        '0 - /bin/check asm input.az8',
        '0 - /bin/cc -i input.az8 helper.az8 -o result',
        '0 - /tmp/result', '0 - /bin/check e711 result',
    ]
    plans['objects'] = [
        '0 - /bin/cc -O -c ' + options + ' input.c helper.c',
        '0 - /bin/check e707 input.b',
        '0 - /bin/cc input.b helper.b -o result',
        '0 - /tmp/result', '0 - /bin/check e707 result',
    ]
    plans['default'] = [
        '0 - /bin/cc /usr/src/hello.c', '0 - /tmp/a.out',
        '0 - /bin/check e707 a.out', '0 - /bin/check absent hello.b',
    ]
    # Feed assembly on stdin through the shell; runner redirects stdout.
    (WORK / 'opt.sh').write_text('/lib/oz8 < /tmp/opt.az8\n')
    extra['tmp/opt.sh'] = WORK / 'opt.sh'
    plans['optimizer'] = [
        '0 opt.out /bin/sh /tmp/opt.sh',
        '0 - /bin/check same opt.out opt.want',
    ]
    (WORK / 'long.az8').write_text('x' * 1024 + '\n')
    (WORK / 'long.sh').write_text('/lib/oz8 < /tmp/long.az8\n')
    extra['tmp/long.az8'] = WORK / 'long.az8'
    extra['tmp/long.sh'] = WORK / 'long.sh'
    plans['opt-errors'] = [
        '1 - /bin/cc -O -B/bin/ -t1 helper.c -o rejected',
        '0 - /bin/check absent rejected',
        '1 long.out /bin/sh /tmp/long.sh',
        '0 - /bin/check',
    ]
    plans['errors'] = []
    for source in ['badcpp.c', 'badfront.c', 'badasm.az8', 'badlink.c']:
        plans['errors'].extend(['1 - /bin/cc ' + source + ' -o result', '0 - /bin/check'])
    for option in ['-o', '-R', '-t0', '-p']:
        plans['errors'].extend(['1 - /bin/cc ' + option, '0 - /bin/check'])
    if set(selected) - plans.keys():
        raise ValueError('unknown test case: ' + ', '.join(set(selected) - plans.keys()))
    records = []
    if selected and (WORK / 'results.json').exists():
        records = [r for r in json.loads((WORK / 'results.json').read_text())
                   if r['case'] not in selected]
    sysbuild = ROOT / 'v7z8000/usr/sys/build'
    for name, plan in plans.items():
        if selected and name not in selected:
            continue
        (WORK / 'plan').write_text('\n'.join(plan) + '\n')
        extra['tmp/plan'] = WORK / 'plan'
        image(extra,WORK/'hd.img',sout=sout)
        # Restored clock statistics push optimized split I/D just past 2B cycles.
        cycles = '200000000000' if name in ('selfhost-driver','optimizer') else '12000000000'
        result = subprocess.run(list(map(str, [sysbuild / 'test_driver', '-c', cycles,
            '-d', WORK / 'hd.img', '-i', 'runner\\n', '-w', 'NATIVE CC DONE',
            '-I', 'exit\\n', '-x', 'NATIVE CC PASS'])), cwd=sysbuild, capture_output=True,
            timeout=240)
        (WORK / (name + '.log')).write_bytes(result.stdout + result.stderr)
        if result.returncode or b'NATIVE CC PASS' not in result.stdout:
            print(result.stdout.decode(errors='replace')[-4000:])
            raise RuntimeError(name + ' failed; see log')
        if name == 'default':
            assert b'Hello from native C' in result.stdout
        elif name not in ('errors', 'optimizer', 'opt-errors'):
            assert b'DRIVER_OK' in result.stdout
        records.append({'case': name, 'commands': len(plan), 'passed': True})
        print(records[-1], flush=True)
    (WORK / 'results.json').write_text(json.dumps(records, indent=2) + '\n')
    # Leave a clean, usable compiler image rather than the last test fixture.
    image(destination=WORK/'hd.img',sout=sout)


if __name__ == '__main__':
    sout=True
    WORK=ROOT/'tests/build/native-cc-sout'
    test([a for a in sys.argv[1:] if a!='--sout'],sout)
