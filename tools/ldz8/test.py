#!/usr/bin/env python3
"""Link real assembler objects on host and V7; execute s.out under V7."""
from pathlib import Path
import json
import re
import shutil
import struct
import subprocess
import sys
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/native-cc'))
from build import image, compile_c, run
from selfhost import Filesystem
WORK = ROOT / 'tests/build/sout-link'
SYS = ROOT / 'v7z8000/usr/sys/build'
LD = ROOT / 'tests/build/ldz8-host/ldz8'
AS = ROOT / 'tests/build/asz8k-host/asz8k'
SOURCE = ROOT / 'tools/ldz8'


def link(args, output, failure=False):
    result = subprocess.run([str(LD), '-z', *args, '-o', output], cwd=WORK, capture_output=True)
    assert bool(result.returncode) == failure, result.stderr.decode(errors='replace')
    if failure:
        assert not (WORK / output).exists(), output
    return result


def layout(name):
    data = (WORK / name).read_bytes()
    h = struct.unpack('>HIIHHIHHH', data[:24])
    seg = [struct.unpack('>4B4HI', data[p:p+16]) for p in range(24, 24+h[3], 16)]
    assert len(data) == 24+h[3]+h[1]*(1 if h[6]&1 else 2)+h[4]
    assert sum(s[4]+s[5] for s in seg) == h[1]
    assert sum(s[6] for s in seg) == h[2]
    return data, h, seg


def main():
    WORK.mkdir(parents=True, exist_ok=True)
    run(['make', '-C', ROOT / 'tools/ldz8'])
    run(['make', '-C', ROOT / 'tools/asz8k'])
    shutil.copyfile(ROOT / 'tools/asz8k/src/asz8k.pd', WORK / 'asz8k.pd')
    for p in (SOURCE / 'tests').glob('*.8k*'):
        shutil.copyfile(p, WORK / p.name)
    shutil.copyfile(ROOT / 'tools/asz8k/tests/soutseg.8ks', WORK / 'seg.8ks')
    for name in ('start.8kn', 'help.8kn', 'seg.8ks', 'ext.8ks'):
        run([AS, '-z', *(['-s'] if name.endswith('s') else []), name], cwd=WORK)
    link(['-s', '-e', 'entry', 'start.so', 'help.so'], 'combined')
    link(['-s', '-i', '-e', 'entry', 'start.so', 'help.so'], 'split')
    for name, magic in [('combined', 0xe707), ('split', 0xe711)]:
        data, h, seg = layout(name)
        assert h[0] == magic and h[1:3] == (512, 256) and h[3:7] == (16, 0, 0, 1)
        assert seg[0][4:7] == (256, 256, 256)
        # External call points to the helper appended after the first object;
        # external data/BSS references use the selected I/D layout.
        start = (WORK / 'start.so').read_bytes()
        t = struct.unpack_from('>H', start, 28)[0]
        assert struct.unpack_from('>H', data, 42)[0] == t
    link(['-r', 'start.so'], 'pending.so')
    link(['-r', 'start.so', 'help.so'], 'partial.so')
    link(['-s', '-i', 'partial.so'], 'relinked')
    assert (WORK / 'relinked').read_bytes() == (WORK / 'split').read_bytes()
    link(['-s', '-i', 'pending.so', 'help.so'], 'resolved')
    assert (WORK / 'resolved').read_bytes() == (WORK / 'split').read_bytes()
    # Put the required member before another member to exercise archive scans.
    (WORK / 'unused.8kn').write_text('__text .sect\n .global unused\nunused: ret\n .end\n')
    run([AS, '-z', 'unused.8kn'], cwd=WORK)
    run(['ar', 'rc', 'libhelp.a', 'unused.so', 'help.so'], cwd=WORK)
    link(['-s', '-i', 'start.so', 'libhelp.a'], 'archive')
    assert (WORK / 'archive').read_bytes() == (WORK / 'split').read_bytes()
    for name, source in {
        'root.8kn': '__text .sect\n .global first\n call first\n ret\n .end\n',
        'first.8kn': '__text .sect\n .global first,later\nfirst: .word later\n .end\n',
        'later.8kn': '__text .sect\n .global later\nlater: ret\n .end\n',
        'abs.8ks': '__text .sect\n .global absolute\nabsolute .equ 9\n .end\n',
    }.items():
        (WORK/name).write_text(source)
        run([AS, '-z', *(['-s'] if name.endswith('s') else []), name], cwd=WORK)
    # Resolving first introduces later, whose member precedes first. A rescan
    # must find it, without ever selecting unused or exporting its symbols.
    run(['ar', 'rc', 'libchain.a', 'later.so', 'first.so', 'unused.so'], cwd=WORK)
    link(['-s', 'root.so', 'libchain.a'], 'chain')
    link(['-s', 'root.so', 'first.so', 'later.so'], 'directchain')
    assert (WORK/'chain').read_bytes() == (WORK/'directchain').read_bytes()
    # A symbol-index relocation may name a local record. The linker reads it
    # on demand rather than allocating every compiler label in native RAM.
    (WORK/'local.8kn').write_text('__text .sect\nlocal: .word local\n .end\n')
    run([AS,'-z','local.8kn'],cwd=WORK)
    local=bytearray((WORK/'local.so').read_bytes())
    size=struct.unpack_from('>I',local,2)[0]
    assert local[40+2*size+6:40+2*size+14].rstrip(b'\0') == b'local'
    struct.pack_into('>H',local,40+size,8) # symbol index zero, word offset
    (WORK/'localref.so').write_bytes(local)
    link(['-s','local.so'],'local')
    link(['-s','localref.so'],'localref')
    assert (WORK/'local').read_bytes() == (WORK/'localref').read_bytes()
    link(['-C', '3', '-D', '5', '-e', 'entry', 'seg.so', 'ext.so'], 'segexec')
    data, h, seg = layout('segexec')
    assert h[0] == 0xe607 and h[5] == 0x83000000
    assert len(seg) == 2 and [s[0] for s in seg] == [3, 5]
    assert seg[1][3] == 1 and seg[1][5:7] == (256, 256)
    # Independent expected instruction/address words from the assembled fixture.
    words = struct.unpack('>14H', data[56:84])
    assert words[1:3] == (0x8500, 6)
    assert words[4] == 0x0500 and words[6] == 0x0506
    assert words[7:10] == (0x8500, 0, 256)
    # .word entry requests only the offset; .long external+4 carries both.
    assert struct.unpack_from('>3H', data, 56+256) == (0, 0x8500, 10)
    link(['-r', 'seg.so', 'ext.so'], 'segpart.so')
    link(['-C', '3', '-D', '5', '-e', 'entry', 'segpart.so'], 'segrelink')
    assert (WORK / 'segexec').read_bytes() == (WORK / 'segrelink').read_bytes()
    link(['-i', '-C', '5', '-D', '3', '-e', 'entry', 'seg.so', 'ext.so'], 'segreverse')
    data, h, seg = layout('segreverse')
    assert h[0] == 0xe611 and h[5] == 0x85000000 and [s[0] for s in seg] == [3,5]
    assert [s[4:7] for s in seg] == [(0,256,256), (256,0,0)]
    link(['seg.so','ext.so','abs.so'], 'segabs')
    data, h, seg = layout('segabs')
    symbols = [struct.unpack('>IBB8s', data[p:p+14]) for p in range(56+h[1], len(data), 14)]
    assert next(s[:3] for s in symbols if s[3]==b'absolute') == (9,33,0)
    link(['start.so'], 'undefined', True)
    link(['start.so', 'help.so', 'help.so'], 'duplicate', True)
    link(['start.so', 'ext.so'], 'mixed', True)
    (WORK / 'far.8ks').write_text('__text .sect\n .global external\n__data .sect\n .block 300\nexternal: .word 0\n .end\n')
    run([AS, '-z', '-s', 'far.8ks'], cwd=WORK)
    link(['seg.so', 'far.so'], 'overflow', True)
    # Invalid relocation index and truncated objects must fail before success.
    corrupt = bytearray((WORK / 'start.so').read_bytes())
    im = struct.unpack_from('>I', corrupt, 2)[0]
    struct.pack_into('>H', corrupt, 40+im+2, 0xfff8)
    (WORK / 'corrupt.so').write_bytes(corrupt)
    link(['corrupt.so', 'help.so'], 'badreloc', True)
    relative = bytearray((WORK/'start.so').read_bytes())
    tag = struct.unpack_from('>H', relative, 40+im+2)[0]
    struct.pack_into('>H', relative, 40+im+2, tag|3)
    (WORK/'relative.so').write_bytes(relative)
    link(['-r', 'relative.so'], 'badrelative', True)
    (WORK / 'short.so').write_bytes(corrupt[:39])
    link(['short.so'], 'truncated', True)
    print('PASS host s.out: layouts, SEG relocations, archives, partial links and errors', flush=True)

    # Stage the exact shared linker sources and rebuild with the native V7 CC.
    files = {}
    for name in ('dispatch.c', 'ldso.c'):
        files['usr/src/ldz8/'+name] = SOURCE / name
    for name in ('soutfmt.c', 'soutfmt.h'):
        files['usr/src/ldz8/'+name] = ROOT / 'tools/asz8k/src' / name
    for name in ('start.so', 'help.so', 'seg.so', 'ext.so', 'libhelp.a', 'corrupt.so', 'far.so', 'relative.so',
                 'root.so', 'libchain.a', 'abs.so', 'local.so', 'localref.so'):
        files['usr/src/ldz8/'+name] = WORK / name
    fs = Filesystem(ROOT / 'tests/build/userland-native-sout/hd.img')
    for name in ('runner', 'sh'):
        p = WORK / name; p.write_bytes(fs.read('/bin/'+name)); files['bin/'+name] = p
    for name, path in {'lib/front': ROOT/'tests/build/native-environment-sout/native/lib/front',
                       'lib/back': ROOT/'tests/build/native-environment-sout/native/lib/back'}.items():
        if path.exists(): files[name] = path
    commands = []
    saved = Filesystem(WORK/'hd.img') if (WORK/'hd.img').exists() else None
    for n in ('dispatch','ldso','soutfmt'):
        deps = [n+'.c'] + (['soutfmt.h'] if n != 'dispatch' else [])
        cached = False
        if saved:
            try:
                cached = saved.read('/usr/src/ldz8/'+n+'.b')[:2]==b'\xe7\x07' and all(saved.read('/usr/src/ldz8/'+d) == files['usr/src/ldz8/'+d].read_bytes() for d in deps)
                if cached:
                    p = WORK/('saved-'+n+'.b'); p.write_bytes(saved.read('/usr/src/ldz8/'+n+'.b'))
                    files['usr/src/ldz8/'+n+'.b'] = p
            except KeyError: cached = False
        if not cached:
            commands.append(('compile-'+n, '0 - /bin/cc -O -c -Dunix=1 '+n+'.c'))
    commands += [('link-ldz8', '0 - /bin/cc -i dispatch.b ldso.b soutfmt.b -o ldz8')]
    trials = {
        'combined': '-s -e entry start.so help.so', 'split': '-s -i -e entry start.so help.so',
        'pending.so': '-r start.so', 'partial.so': '-r start.so help.so',
        'archive': '-s -i start.so libhelp.a', 'segexec': '-C 3 -D 5 -e entry seg.so ext.so',
        'segpart.so': '-r seg.so ext.so', 'segrelink': '-C 3 -D 5 -e entry segpart.so',
        'segreverse': '-i -C 5 -D 3 -e entry seg.so ext.so',
        'chain': '-s root.so libchain.a', 'segabs': 'seg.so ext.so abs.so',
        'resolved': '-s -i pending.so help.so', 'relinked': '-s -i partial.so',
        'local': '-s local.so', 'localref': '-s localref.so',
    }
    commands += [('native-'+name, '0 - ./ldz8 -z '+args+' -o '+name) for name,args in trials.items()]
    commands += [('bad-'+n, '1 - ./ldz8 -z '+args+' -o reject') for n,args in {
        'undefined': 'start.so', 'duplicate': 'start.so help.so help.so',
        'reloc': 'corrupt.so help.so', 'range': 'seg.so far.so', 'mode': 'start.so ext.so',
        'relative': '-r relative.so'}.items()]
    commands += [('exec-combined', '0 - ./combined'), ('exec-split', '0 - ./split')]
    for i, (_, command) in enumerate(commands):
        p = WORK / ('p%03d'%i); p.write_text(command+'\n'); files['tmp/'+p.name] = p
    # Loader malformed-image tests use a normal C caller to verify ENOEXEC and
    # that a failed exec preserves the calling process, then concurrent execs.
    compile_c(SOURCE / 'tests/check.c', WORK / 'check.b')
    run([LD, '-i', '-x', ROOT/'tools/libc/crt0.b', WORK/'check.b',
         ROOT/'tools/libv7.a', '-o', WORK/'check'])
    files['bin/check'] = WORK/'check'
    files['bin/combined'] = WORK/'combined'; files['bin/split'] = WORK/'split'
    bad = bytearray((WORK/'split').read_bytes())
    mutants = []
    for offset, fmt, value in [(18,'H',0),(10,'H',32),(14,'H',1),(34,'H',128),
                               (2,'I',1),(6,'I',1),(16,'H',1),(22,'H',1)]:
        b = bad.copy(); struct.pack_into('>'+fmt, b, offset, value); mutants.append(b)
    mutants += [bad[:39], bytearray((WORK/'segexec').read_bytes())]
    for i, b in enumerate(mutants):
        p = WORK/('bad%d'%i); p.write_bytes(b); files['bin/'+p.name] = p
    p = WORK/'checkplan'; p.write_text('0 - /bin/check\n'); files['tmp/checkplan'] = p
    image(files, WORK/'hd.img', blocks=12000, inodes=1024)
    records = []
    for i, (name, _) in enumerate(commands+ [('loader', ''), ('loader-lowram', '')]):
        plan = '/tmp/p%03d'%i if i<len(commands) else '/tmp/checkplan'
        print('START', name, flush=True)
        with (WORK/(name+'.log')).open('wb') as log:
            result = subprocess.run([str(SYS/'test_driver'), '-c', '200000000000',
                *(['-R', '320', '-S', '4096'] if name == 'loader-lowram' else []),
                '-d', str(WORK/'hd.img'), '-o', str(WORK/'next.img'),
                '-i', 'runner '+plan+' /usr/src/ldz8\\n',
                '-w', 'NATIVE CC DONE', '-I', 'exit\\n', '-x', 'NATIVE CC DONE'],
                cwd=SYS, stdout=log, stderr=subprocess.STDOUT, timeout=900)
        assert result.returncode == 0 and b'NATIVE CC PASS\r\n' in (WORK/(name+'.log')).read_bytes(), name
        if name.startswith('loader'):
            logdata = (WORK/(name+'.log')).read_bytes()
            shared = re.search(rb'Shared text peak mappings: (\d+)', logdata)
            assert shared and int(shared[1]) >= 4, 's.out text was not concurrently shared'
            assert b'Absent RAM accesses: 0' in logdata
            if name == 'loader-lowram':
                swap = re.search(rb'Swap sectors: (\d+) read, (\d+) written', logdata)
                assert swap and int(swap[1]) > 21 and int(swap[2]) > 21, 'no process/text swapping under pressure'
        (WORK/'next.img').replace(WORK/'hd.img'); records.append(name)
        print('PASS', name, flush=True)
    fs = Filesystem(WORK/'hd.img')
    for name in ('dispatch.c','ldso.c','soutfmt.c','soutfmt.h'):
        assert fs.read('/usr/src/ldz8/'+name) == files['usr/src/ldz8/'+name].read_bytes(), name
    for name in trials:
        assert fs.read('/usr/src/ldz8/'+name) == (WORK/name).read_bytes(), name
    native = fs.read('/usr/src/ldz8/ldz8')
    (WORK/'ldz8-native').write_bytes(native)
    sizes = dict(zip(('text','data','bss'), struct.unpack_from('>3H',native,28)))
    (WORK/'results.json').write_text(json.dumps({'steps': records, 'identical': list(trials), 'native': sizes}, indent=2)+'\n')
    print('PASS: native source rebuild;', len(trials), 'identical host/native links; s.out execution and ENOEXEC checks', sizes)


if __name__ == '__main__': main()
