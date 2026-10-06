#!/usr/bin/env python3
"""Exercise the complete C pipeline, driver modes, and failures in Unix."""
import json
import subprocess
import sys
sys.dont_write_bytecode = True
from build import ROOT, PCC, WORK, run, compile_c, image


def test(selected=()):
    extra = {}
    for name in ['runner', 'check']:
        compile_c(ROOT / 'tools/native-cc' / (name + '.c'), WORK / (name + '.b'))
        run([PCC / 'ldz8', '-x', ROOT / 'tools/libc/crt0.b', WORK / (name + '.b'),
             ROOT / 'tools/libv7.a', '-o', WORK / name])
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
    compile_c(WORK / 'front.c', WORK / 'front.b')
    run([PCC / 'ldz8', '-x', ROOT / 'tools/libc/crt0.b', WORK / 'front.b',
         ROOT / 'tools/libv7.a', '-o', WORK / 'front'])
    # Install the environment-checking front-end wrapper as an executable.
    extra['bin/front'] = WORK / 'front'
    options = '-I/tmp/include -DVALUE=2 -DREMOVE -UREMOVE'
    plans = {}
    for mode in ['0407', '0411']:
        flag = '-i ' if mode == '0411' else ''
        plans['full-' + mode] = [
            '0 - /bin/cc ' + flag + options + ' -B/bin/ -t0 input.c helper.c -o result',
            '0 - /tmp/result', '0 - /bin/check ' + mode + ' result',
        ]
    plans['stages'] = [
        '0 pre.i /bin/cc -E ' + options + ' input.c',
        '0 - /bin/check pre pre.i',
        '0 - /bin/cc -P ' + options + ' input.c',
        '0 - /bin/check pre input.i',
        '0 - /bin/cc -S ' + options + ' input.c helper.c',
        '0 - /bin/check asm input.az8',
        '0 - /bin/cc -i input.az8 helper.az8 -o result',
        '0 - /tmp/result', '0 - /bin/check 0411 result',
    ]
    plans['objects'] = [
        '0 - /bin/cc -c ' + options + ' input.c helper.c',
        '0 - /bin/check 0407 input.b',
        '0 - /bin/cc input.b helper.b -o result',
        '0 - /tmp/result', '0 - /bin/check 0407 result',
    ]
    plans['default'] = [
        '0 - /bin/cc /usr/src/hello.c', '0 - /tmp/a.out',
        '0 - /bin/check 0407 a.out', '0 - /bin/check absent hello.b',
    ]
    plans['errors'] = []
    for source in ['badcpp.c', 'badfront.c', 'badasm.az8', 'badlink.c']:
        plans['errors'].extend(['1 - /bin/cc ' + source + ' -o result', '0 - /bin/check'])
    for option in ['-o', '-R', '-t0', '-O']:
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
        image(extra)
        result = subprocess.run(list(map(str, [sysbuild / 'test_driver', '-c', '2000000000',
            '-d', WORK / 'hd.img', '-i', 'runner\\n', '-w', 'NATIVE CC DONE',
            '-I', 'exit\\n', '-x', 'NATIVE CC PASS'])), cwd=sysbuild, capture_output=True, timeout=60)
        (WORK / (name + '.log')).write_bytes(result.stdout + result.stderr)
        if result.returncode or b'NATIVE CC PASS' not in result.stdout:
            print(result.stdout.decode(errors='replace')[-4000:])
            raise RuntimeError(name + ' failed; see log')
        if name == 'default':
            assert b'Hello from native C' in result.stdout
        elif name != 'errors':
            assert b'DRIVER_OK' in result.stdout
        records.append({'case': name, 'commands': len(plan), 'passed': True})
        print(records[-1], flush=True)
    (WORK / 'results.json').write_text(json.dumps(records, indent=2) + '\n')
    # Leave a clean, usable compiler image rather than the last test fixture.
    image()


if __name__ == '__main__':
    test(sys.argv[1:])
