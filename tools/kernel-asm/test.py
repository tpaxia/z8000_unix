#!/usr/bin/env python3
"""Compare host/native machine assembly and raw links against pre-migration images."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys

sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT/'tools/native-cc'))
from build import compile_c, image, run
from selfhost import Filesystem

EXPECTED = {
    'rom.bin': '312981ae72c377027781ad8ff35f0be9f19dbdc5ddcd15e607cf6ceba2f357dc',
    'kernel.bin': '90d2bfaf2f51be6a9867aa333a522f69556c10d858fcfb1207c11b928dc53046',
    'fpe.bin': 'c782219072e0ec137eefa88ef518f837a8775803ffe9307d846c8cb2084b93f4',
    'board.bin': '038e7b64c24cac3ac485506647efa459185005db6b18a03868db15c0d9b5614e',
}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--kernel-build', type=Path, default=ROOT/'v7z8000/usr/sys/build')
    p.add_argument('--host-only', action='store_true')
    args = p.parse_args()
    system = args.kernel_build.resolve()
    work = ROOT/'tests/build/kernel-asm'
    work.mkdir(parents=True, exist_ok=True)
    run(['make', '-C', ROOT/'tools/asz8k'])
    run(['make', '-C', ROOT/'tools/ldz8'])
    assembler = ROOT/'tests/build/asz8k-host/asz8k'
    linker = ROOT/'tests/build/ldz8-host/ldz8'
    shutil.copyfile(ROOT/'tools/asz8k/src/asz8k.pd', work/'asz8k.pd')
    sources = {
        'rom.s': ROOT/'v7z8000/usr/sys/machine/boards/unixv7/z8001/emurom.s',
        'trap.s': ROOT/'v7z8000/usr/sys/machine/z8000/z8001/trap.s',
        'unix.s': ROOT/'v7z8000/usr/sys/machine/z8000/z8001/unix.s',
        'block.s': ROOT/'mame/boot/unixv7/z8001/block.s',
    }
    for name, source in sources.items():
        shutil.copyfile(source, work/name)
    run([sys.executable, ROOT/'tools/fpe/translate.py', ROOT/'v7z8000/usr/sys/fpe/fpe.z8k', work/'core.s'])
    handoff = sources['rom.s'].read_text().split('initboot:', 1)[1]
    (work/'board.s').write_text((ROOT/'mame/boot/unixv7/z8001/rom.s').read_text()+'\n.org 0x200\n'+handoff)
    # Small independent probe covers mixed modes, cross-object relocation,
    # nonzero origin, contiguous data/BSS and canonical byte-shift encoding.
    (work/'mixed.s').write_text('.text\n.global entry,datum\nentry:\n.segm\n'
        'ld r2,datum\n.unsegm\nsrlb rh1,#4\n.word datum\n.data\n.word entry\n.bss\n.space 4\n')
    (work/'datum.s').write_text('.data\n.global datum\ndatum: .word 0x1234\n')
    (work/'pcc.az8').write_text('.text\n.globl entry,target\nentry:\n'
        'calr target\n.Llonglocal1:\noutb rl3,@r2\nout r0,#0xb0\n'
        '.word .Llonglocal1,.Llonglocal2\n.Llonglocal2:\nret\n')
    (work/'callee.az8').write_text('.text\n.globl target\ntarget:\nret\n')
    (work/'legacy.b').write_bytes(struct.pack('>8H',0o407,2,0,0,0,0,0,0)+bytes.fromhex('9e08'))
    commands = []
    for name in ('rom', 'trap', 'unix', 'core', 'board', 'block', 'mixed', 'datum'):
        commands.append((name+'-as', f'asz8k -gs -o {name}.so {name}.s'))
    for name in ('pcc','callee'):
        commands.append((name+'-as', f'asz8k -c -o {name}.so {name}.az8'))
    links = {
        'rom.bin': '-C 0 rom.so', 'kernel.bin': '-C 1 -M 512 trap.so',
        'fpe.bin': '-C 127 -M 61440 unix.so core.so',
        'board.bin': '-C 0 -M 2048 board.so',
        'block.bin': '-C 3 -T 65024 block.so',
        'mixed.bin': '-C 3 -T 256 mixed.so datum.so',
    }
    for name, options in links.items():
        commands.append((name+'-ld', f'ldz8 -b {options} -o {name}'))
    commands.append(('pcc-ld','ldz8 -i -s pcc.so callee.so -o pcc.out'))
    commands += [
        ('bad-limit', 'ldz8 -z -b -M 100 rom.so -o reject'),
        ('bad-origin', 'ldz8 -z -b -T 65534 rom.so -o reject'),
        ('bad-partial', 'ldz8 -z -b -r rom.so -o reject'),
        ('bad-split', 'ldz8 -z -b -i rom.so -o reject'),
        ('bad-undefined', 'ldz8 -z -b unix.so -o reject'),
        ('bad-legacy-as', 'asz8k -a -o reject rom.s'),
        ('bad-legacy-ld', 'ldz8 legacy.b -o reject'),
    ]
    for name, command in commands:
        argv = command.split()
        argv[0] = str(assembler if argv[0]=='asz8k' else linker)
        result = subprocess.run(argv, cwd=work, capture_output=True)
        assert bool(result.returncode) == name.startswith('bad-'), (name, result.stderr)
        if name.startswith('bad-'):
            assert not (work/'reject').exists(), name
    for name, digest in EXPECTED.items():
        assert hashlib.sha256((work/name).read_bytes()).hexdigest()==digest, name
    mixed = (work/'mixed.bin').read_bytes()
    assert mixed == bytes.fromhex('61028300010eb21100fc010e01001234'), mixed.hex()
    pcc = (work/'pcc.out').read_bytes()
    assert pcc[:2] == bytes.fromhex('e711')
    assert pcc[40:44] == bytes.fromhex('5f000010'), pcc[40:44].hex()
    assert struct.unpack_from('>2H',pcc,50)==(4,14)
    assert len((work/'block.bin').read_bytes())==512
    if args.host_only:
        print('PASS: host machine assembly, four pre-migration image hashes and raw-link failures')
        return

    # Seed target binaries from the same C sources; then run them inside V7.
    # Python stages files and runs the machine, never assembles/links Z8000 code.
    files = {'usr/lib/asz8k.pd': work/'asz8k.pd'}
    sizes = {}
    for tool, source_list, flags in [
        ('asz8k', sorted((ROOT/'tools/asz8k/src').glob('*.c')), []),
        ('ldz8', [ROOT/'tools/ldz8/dispatch.c', ROOT/'tools/ldz8/ldso.c',
                  ROOT/'tools/asz8k/src/soutfmt.c'],
         ['-I'+str(ROOT/'PCC-z8000/z8000'), '-I'+str(ROOT/'tools/asz8k/src')]),
    ]:
        directory = work/tool
        directory.mkdir(exist_ok=True)
        objects = []
        for source in source_list:
            obj = directory/(source.stem+'.b')
            compile_c(source, obj, flags,sout=True)
            objects.append(obj)
        target = directory/tool
        run([linker, '-z', '-i', '-s', ROOT/'tests/build/sout-cc/crt0.b',
             *objects, ROOT/'tests/build/sout-cc/libc.a', '-o', target])
        sizes[tool] = struct.unpack_from('>3H',target.read_bytes(),28)
        files['bin/'+tool] = target
    for name in ('runner', 'sh'):
        target = ROOT/'tests/build/native-cc-sout'/name
        files['bin/'+name] = target
    for source in list(work.glob('*.s'))+list(work.glob('*.az8')):
        files['usr/src/machine/'+source.name] = source
    files['usr/src/machine/legacy.b']=work/'legacy.b'
    for index, (name, command) in enumerate(commands):
        plan = work/('p%03d'%index)
        plan.write_text(('1' if name.startswith('bad-') else '0')+' - /bin/'+command+'\n')
        files['tmp/'+plan.name] = plan
    image(files, work/'hd.img', blocks=12000, inodes=1024)
    for index, (name, _) in enumerate(commands):
        print('START native', name, flush=True)
        with (work/(name+'.log')).open('wb') as log:
            result = subprocess.run([str(system/'test_driver'), '-c', '300000000000',
                '-d', str(work/'hd.img'), '-o', str(work/'next.img'),
                '-i', f'runner /tmp/p{index:03d} /usr/src/machine\\n',
                '-w', 'NATIVE CC DONE', '-I', 'exit\\n', '-x', 'NATIVE CC DONE'],
                cwd=system, stdout=log, stderr=subprocess.STDOUT, timeout=900)
        data = (work/(name+'.log')).read_bytes()
        assert result.returncode==0 and b'NATIVE CC PASS\r\n' in data and b'Absent RAM accesses: 0' in data, name
        (work/'next.img').replace(work/'hd.img')
        print('PASS native', name, flush=True)
    fs = Filesystem(work/'hd.img')
    identical = [name+'.so' for name in ('rom','trap','unix','core','board','block','mixed','datum','pcc','callee')]+list(links)+['pcc.out']
    for name in identical:
        assert fs.read('/usr/src/machine/'+name)==(work/name).read_bytes(), name
    (work/'results.json').write_text(json.dumps({'native_sizes': sizes,
        'commands': len(commands), 'identical': identical}, indent=2)+'\n')
    print('PASS: host/native machine assembly and raw linking;', sizes)


if __name__=='__main__':
    main()
