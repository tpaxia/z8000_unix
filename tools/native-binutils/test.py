#!/usr/bin/env python3
"""Run native az8/ldz8 in Unix; compare objects and executables with host tools."""
import json
import sys
sys.dont_write_bytecode = True
from build import ROOT, PCC, WORK, run, compile_c

SYSBUILD = ROOT / 'v7z8000/usr/sys/build'


def test():
    compile_c(ROOT / 'tools/native-binutils/runner.c', WORK / 'runner.b')
    run([PCC / 'ldz8', '-x', ROOT / 'tools/libc/crt0.b', WORK / 'runner.b',
         ROOT / 'tools/libv7.a', '-o', WORK / 'runner'])
    # The first archive member adds and then rolls back more than two blocks
    # of symbols; the selected member subsequently reuses these slots.
    sources = {
        'unused': '\n'.join('int u%04d = %d;' % (i, i) for i in range(90)),
        'helper': 'helper(x) int x; { return x+2; }\n',
        'input': '\n'.join('int g%04d;' % i for i in range(90)) + '''
int data = 40;
int *pointer = &data;
int arena[40];
main() {
    arena[39] = helper(*pointer);
    g0089 = arena[39];
    if (g0000 || g0089 != 42) return 1;
    puts("PROGRAM OK");
    return 0;
}
''',
    }
    for name, source in sources.items():
        (WORK / (name + '.c')).write_text(source)
        compile_c(WORK / (name + '.c'), WORK / (name + '.b'))
    (WORK / 'lib.a').unlink(missing_ok=True)
    run(['ar', 'cr', WORK / 'lib.a', WORK / 'unused.b', WORK / 'helper.b'])
    records = []
    cases = [('archive', WORK / 'input.c'),
             ('long', PCC / 'test/larith.c'),
             ('float', PCC / 'test/regress/float_storage.c'),
             ('byte', PCC / 'test/regress/asm_byte_regs.az8')]
    for name, source in cases:
        if source.suffix == '.c':
            compile_c(source, WORK / 'input.b')
        else:
            (WORK / 'input.az8').write_bytes(source.read_bytes())
        # Exercise dot-assignment padding, whose count is a long in K&R C.
        with (WORK / 'input.az8').open('a') as assembly:
            assembly.write('\n\t.data\n\t.even\n\t. = . + 8\n\t.word 0x1234\n')
        run([PCC / 'az8/az8', '-o', 'input.b', 'input.az8'], cwd=WORK)
        for mode in ['0407', '0411']:
            flag = '-i' if mode == '0411' else '-x'
            run([PCC / 'ldz8', '-x', flag, ROOT / 'tools/libc/crt0.b', WORK / 'input.b',
                 WORK / 'lib.a', ROOT / 'tools/libv7.a', '-o', WORK / 'expected'])
            (WORK / 'proto').write_text(f'''boot
2400 96
d--755 0 0
bin d--755 0 0
 sh ---755 0 0 {ROOT}/tools/sh
 az8 ---755 0 0 {WORK}/az8/az8
 ldz8 ---755 0 0 {WORK}/ldz8/ldz8
 runner ---755 0 0 {WORK}/runner
 $
lib d--755 0 0
 crt0.b ---644 0 0 {ROOT}/tools/libc/crt0.b
 libv7.a ---644 0 0 {ROOT}/tools/libv7.a
 $
dev d--755 0 0
 console c--644 0 0 0 0
 tty c--644 0 0 2 0
 $
etc d--755 0 0
 init ---755 0 0 {ROOT}/tools/init
 $
tmp d--777 0 0
 input.az8 ---644 0 0 {WORK}/input.az8
 lib.a ---644 0 0 {WORK}/lib.a
 $
$
''')
            run([ROOT / 'tools/v7mkfs', WORK / 'hd.img', WORK / 'proto'])
            result = run([SYSBUILD / 'test_driver', '-c', '900000000', '-d', WORK / 'hd.img',
                          '-i', 'runner ' + flag + '\\n', '-w', 'NATIVE TOOLS PASS',
                          '-I', 'exit\\n', '-x', 'NATIVE TOOLS PASS'], cwd=SYSBUILD, timeout=60)
            (WORK / (name + '-' + mode + '.log')).write_bytes(result.stdout + result.stderr)
            assert b'NATIVE TOOLS PASS' in result.stdout
            if name == 'archive':
                assert b'PROGRAM OK' in result.stdout
            for guest, expected in [('input.b', 'input.b'), ('result', 'expected')]:
                data = result.stdout.split(('BEGIN /tmp/' + guest + '\r\n').encode(), 1)[1]
                data = bytes.fromhex(data.split(('END /tmp/' + guest).encode(), 1)[0].decode())
                (WORK / (name + '-' + mode + '-' + guest)).write_bytes(data)
                assert data == (WORK / expected).read_bytes(), (mode, guest)
            records.append({'case': name, 'mode': mode, 'object_identical': True,
                            'executable_identical': True, 'execution_passed': True})
            print(records[-1], flush=True)
    (WORK / 'results.json').write_text(json.dumps(records, indent=2) + '\n')


if __name__ == '__main__':
    test()
