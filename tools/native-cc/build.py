#!/usr/bin/env python3
"""Build a bootable disk containing the native two-pass C toolchain."""
from pathlib import Path
import importlib.util
import json
import shutil
import struct
import sys
sys.dont_write_bytecode = True

ROOT = Path(__file__).resolve().parents[2]
PCC = ROOT / 'PCC-z8000/z8000'
WORK = ROOT / 'tests/build/native-cc-sout'
PASSES = ROOT / 'tests/build/native-pcc'
BINUTILS = ROOT / 'tests/build/native-binutils'


def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


helpers = module('binutils_build', ROOT / 'tools/native-binutils/build.py')
run, compile_c = helpers.run, helpers.compile_c


def image(extra_files=None, destination=None, blocks=6000, inodes=512, modes=None, sout=True, owners=None):
    """Install target tools, V7 headers, and optional test fixtures."""
    run([sys.executable, ROOT / 'tools/export-headers.py', '--check'])
    seed=ROOT/'tests/build/sout-cc'
    toolwork=ROOT/'tests/build/native-cc-sout'
    files = {
        'bin/cc':seed/'seed/cc.out',
        'bin/asz8k':seed/'seed/asz8k.out',
        'bin/ldz8':seed/'seed/ldz8.out',
        'bin/sh':toolwork/'sh', 'etc/init':toolwork/'init',
        'bin/echo':toolwork/'echo', 'bin/cat':toolwork/'cat',
        'lib/cpp':toolwork/'cpp', 'lib/oz8':toolwork/'oz8',
        'lib/front':PASSES/'target-front/front', 'lib/back':PASSES/'target-back/back',
        'lib/crt0.b':seed/'crt0.b', 'lib/libc.a':seed/'libc.a',
        'usr/lib/asz8k.pd':ROOT/'tools/asz8k/src/asz8k.pd',
        'usr/src/hello.c':ROOT/'tools/native-cc/hello.c',
    }
    for target, source in files.items():
        if target.startswith(('bin/','etc/')) or target in ('lib/cpp','lib/front','lib/back','lib/oz8'):
            if source.read_bytes()[:2] not in (b'\xe7\x07',b'\xe7\x11'):
                raise ValueError('bootstrap executable is not NONSEG s.out: '+str(source))
    includes = ROOT / 'v7z8000/usr/include'
    for path in includes.rglob('*'):
        if path.is_file():
            files['usr/include/' + str(path.relative_to(includes))] = path
    files.update(extra_files or {})
    tree = {'tmp': {}, 'dev': {'console': 'c--644 0 0 0 0', 'tty': 'c--644 0 0 2 0',
                              'null': 'c--666 0 0 4 2',
                              'mem': 'c--600 0 0 4 0', 'kmem': 'c--600 0 0 4 1',
                              'swap': 'b--600 0 0 1 1', 'rhd': 'c--600 0 0 3 0'}}
    for target, source in files.items():
        parts = target.split('/')
        node = tree
        for part in parts[:-1]:
            node = node.setdefault(part, {})
        mode = '755' if parts[0] in ('bin', 'etc') or target in (
            'lib/cpp', 'lib/front', 'lib/back', 'lib/oz8') else '644'
        if modes and target in modes:
            mode = '%03o' % (modes[target] & 0o777)
        permissions = (modes or {}).get(target, 0)
        flags = '-' + ('u' if permissions & 0o4000 else '-') + ('g' if permissions & 0o2000 else '-')
        uid, gid = (owners or {}).get(target, (0, 0))
        node[parts[-1]] = flags + mode + ' %d %d ' % (uid, gid) + str(source)

    def directory(node, parent=""):
        lines = []
        for name, value in sorted(node.items()):
            assert len(name) <= 14, name
            if isinstance(value, dict):
                target = parent + '/' + name if parent else name
                mode = (modes or {}).get(target, 0o777 if name == 'tmp' else 0o755)
                uid, gid = (owners or {}).get(target, (0, 0))
                lines.append(name + ' d--%03o %d %d' % (mode & 0o777, uid, gid))
                lines.extend(directory(value, target))
            else:
                lines.append(name + ' ' + value)
        return lines + ['$']

    destination = Path(destination) if destination else WORK / 'hd.img'
    proto = destination.with_suffix('.proto')
    proto.write_text('boot\n%d %d\nd--755 0 0\n' % (blocks, inodes) + '\n'.join(directory(tree)) + '\n')
    run([ROOT / 'tools/v7mkfs', destination, proto])


def build(no_compact=False):
    """Cross-compile the minimum native environment without legacy objects."""
    from object_format import sizes
    WORK.mkdir(parents=True,exist_ok=True)
    seed=module('sout_seed',ROOT/'tools/sout-cc/build.py')
    seed.library(); seed.seeds()
    run(['make','-C',PCC/'test','../oz8'])
    run([sys.executable,ROOT/'tools/pcc-native/prepare.py',PASSES])
    linker=ROOT/'tests/build/ldz8-host/ldz8'
    runtime=ROOT/'tests/build/sout-cc'
    report={}
    def program(name,sources,flags=(),directory=None,split=True):
        directory=Path(directory or WORK/(name+'-objects'))
        directory.mkdir(parents=True,exist_ok=True)
        objects=[]
        for source in sources:
            obj=directory/(source.stem+'.b')
            compile_c(source,obj,flags,sout=True,compact=not no_compact)
            objects.append(obj)
        output=WORK/name
        run([linker,'-z',*(['-i'] if split else []),'-s',runtime/'crt0.b',
             *objects,runtime/'libc.a','-o',output])
        if split: report[name]=sizes(output.read_bytes(),True)
        else: report[name]=dict(zip(('text','data','bss'),struct.unpack_from('>3H',output.read_bytes(),28)))
        print('SEED',name,report[name],flush=True)
        return output
    for phase,names in [
        ('front','cgram xdefs scan pftn trees optim code local comm1 frontglue'),
        ('back','reader local2 order match allo comm2 table backglue')]:
        directory=PASSES/('sout-'+phase)
        output=program(phase,[PASSES/(n+'.c') for n in names.split()],
            ['-DBUG1','-DBUG2','-DBUG3','-DBUG4','-I'+str(PASSES)],directory)
        destination=PASSES/('target-'+phase)/phase
        destination.parent.mkdir(parents=True,exist_ok=True)
        shutil.copyfile(output,destination)
    regen=module('regen',PCC/'cz8/regen_cgram.py')
    yaccdir=WORK/'yacc';yaccdir.mkdir(exist_ok=True)
    yacc=regen.build_yacc(yaccdir)
    cppsrc=ROOT/'v7z8000/usr/src/cmd/cpp'
    shutil.copyfile(cppsrc/'cpy.y',yaccdir/'cpy.y')
    run([yacc,'cpy.y'],cwd=yaccdir)
    program('cpp',[cppsrc/'cpp.c',yaccdir/'y.tab.c'],['-I'+str(cppsrc)])
    program('oz8',[PCC/'oz8.c'])
    program('init',[ROOT/'tools/init.c'],split=False)
    for name in ('echo','cat'):
        program(name,[ROOT/'tools'/(name+'.c')],split=False)
    shell=ROOT/'v7z8000/usr/src/cmd/sh'
    program('sh',[shell/(n+'.c') for n in
        'args blok builtin cmd ctype error expand fault io macro main msg name print service setbrk stak string word xec'.split()],
        ['-I'+str(shell)],split=False)
    for name in ('runner','check','normal'):
        program(name,[ROOT/'tools/native-cc'/(name+'.c')],split=False)
    run(['make','-C',ROOT/'tools','v7mkfs'])
    (WORK/'sizes.json').write_text(json.dumps(report,indent=2)+'\n')
    image(sout=True)
    print('Native s.out compiler disk:',WORK/'hd.img',flush=True)


if __name__ == '__main__':
    if any(arg not in ('--no-compact','--sout','--image') for arg in sys.argv[1:]):
        raise SystemExit('usage: build.py [--no-compact] [--image]')
    WORK=ROOT/'tests/build/native-cc-sout'
    if '--image' in sys.argv[1:] and all((WORK/name).exists() for name in ('sh','init','echo','cat')):
        image(destination=WORK/'hd.img')
    else:
        build('--no-compact' in sys.argv[1:])
