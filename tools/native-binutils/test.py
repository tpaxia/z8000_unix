#!/usr/bin/env python3
"""Run native asz8k/ldz8 in Unix; compare objects and executables with host tools."""
import json
import argparse
import sys
sys.dont_write_bytecode = True
from build import ROOT, PCC, WORK, run, compile_c

SYSBUILD = ROOT / 'v7z8000/usr/sys/build'


def test(native_az8=None, native_ldz8=None):
    native_az8 = native_az8 or ROOT/'tests/build/native-environment-sout/native/bin/asz8k'
    native_ldz8 = native_ldz8 or ROOT/'tests/build/native-environment-sout/native/bin/ldz8'
    WORK.mkdir(parents=True,exist_ok=True)
    run(['make','-C',ROOT/'tools','libv7.a','libc/crt0.b','v7mkfs','sh','init'])
    compile_c(ROOT / 'tools/native-binutils/runner.c', WORK / 'runner.b')
    run([ROOT/'tests/build/ldz8-host/ldz8', '-x', ROOT / 'tools/libc/crt0.b', WORK / 'runner.b',
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
    # Many branches share the same range tables. Per-branch copies exhaust
    # the native assembler's data space before this file can be assembled.
    dense = WORK / 'dense.az8'
    dense.write_text('\t.text\n\t.globl _main\n_main:\n\tclr r0\n\tjr .L9999\n' +
                     ''.join('.L%d:\n\tjr .L9999\n' % i for i in range(700)) +
                     '.L9999:\n\tret\n')
    cases = [('archive', WORK / 'input.c'),
             ('long', PCC / 'test/larith.c'),
             ('float', PCC / 'test/regress/float_storage.c'),
             ('byte', PCC / 'test/regress/asm_byte_regs.az8'),
             ('branches', dense)]
    for name, source in cases:
        if source.suffix == '.c':
            compile_c(source, WORK / 'input.b')
        else:
            (WORK / 'input.az8').write_bytes(source.read_bytes())
        # Exercise initialized padding with the shared assembler directive.
        with (WORK / 'input.az8').open('a') as assembly:
            assembly.write('\n\t.data\n\t.even\n\t.space 8\n\t.word 0x1234\n')
        run([ROOT/'tests/build/asz8k-host/asz8k', '-c', '-o', 'input.b', 'input.az8'], cwd=WORK)
        for mode in ['combined', 'split']:
            flag = '-i' if mode == 'split' else '-x'
            run([ROOT/'tests/build/ldz8-host/ldz8', '-x', flag, ROOT / 'tools/libc/crt0.b', WORK / 'input.b',
                 WORK / 'lib.a', ROOT / 'tools/libv7.a', '-o', WORK / 'expected'])
            (WORK / 'proto').write_text(f'''boot
2400 96
d--755 0 0
bin d--755 0 0
 sh ---755 0 0 {ROOT}/tools/sh
 asz8k ---755 0 0 {native_az8.resolve()}
 ldz8 ---755 0 0 {native_ldz8.resolve()}
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
usr d--755 0 0
 lib d--755 0 0
  asz8k.pd ---644 0 0 {ROOT}/tools/asz8k/src/asz8k.pd
 $
$
tmp d--777 0 0
 input.az8 ---644 0 0 {WORK}/input.az8
 lib.a ---644 0 0 {WORK}/lib.a
 $
$
''')
            run([ROOT / 'tools/v7mkfs', WORK / 'hd.img', WORK / 'proto'])
            # Include serial hex dumps of the dense fixture's image, symbols
            # and relocation; completing the link alone is not the endpoint.
            result = run([SYSBUILD / 'test_driver', '-c', '20000000000', '-d', WORK / 'hd.img',
                          '-i', 'runner ' + flag + '\\n', '-w', 'NATIVE TOOLS PASS',
                          '-I', 'exit\\n', '-x', 'NATIVE TOOLS PASS'], cwd=SYSBUILD, timeout=180)
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
    from pathlib import Path
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--native-asz8k', type=Path)
    parser.add_argument('--native-ldz8', type=Path)
    args = parser.parse_args()
    test(args.native_asz8k, args.native_ldz8)
