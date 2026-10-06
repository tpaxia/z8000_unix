"""Exercise 0411 programs under Unix, including an image larger than 64 KB."""
import pathlib
import struct
import subprocess
import sys
import tempfile

build = pathlib.Path(sys.argv[1]).resolve()
tools = pathlib.Path(__file__).resolve().parent
header = struct.unpack('>8H', (tools / 'splittest').read_bytes()[:16])
assert header[0] == 0o411 and sum(header[1:4]) > 65536, header
assert header[1] < 65536 and header[2] + header[3] < 65536, header
print(f'split image: text={header[1]}, data={header[2]}, BSS={header[3]}', flush=True)
# Link-time rejection must happen before size fields or addresses wrap.
pcc = tools.parent / 'PCC-z8000/z8000'
with tempfile.TemporaryDirectory(prefix='split-link-', dir=build) as directory:
    work = pathlib.Path(directory)
    (work / 'over.az8').write_text('.bss\n.comm _over,40000\n')
    subprocess.run([str(pcc / 'az8/az8'), '-o', 'over.b', 'over.az8'],
                   cwd=work, check=True, capture_output=True)
    (work / 'text.az8').write_text('.text\n.zerow 16000\n')
    subprocess.run([str(pcc / 'az8/az8'), '-o', 'text.b', 'text.az8'],
                   cwd=work, check=True, capture_output=True)
    objects = [tools / 'libc/crt0.b', tools / 'splitpad.b', tools / 'splittest.b']
    for name, flags, extra in [('combined', [], []),
                               ('data_overflow', ['-i'], [work / 'over.b']),
                               ('text_overflow', ['-i'], [work / 'text.b'])]:
        result = subprocess.run([str(pcc / 'ldz8'), '-x', *flags,
                                 *map(str, objects + extra), str(tools / 'libv7.a'),
                                 '-o', str(work / name)], capture_output=True)
        assert result.returncode != 0, name
        assert b'exceeds 16-bit address space' in result.stdout + result.stderr, name
    print('split linker: rejects combined, data-space, and instruction-space overflow', flush=True)
for command, expected in [('splittest', 'split: all checks passed'),
                          ('libctest', 'libc: 37 passed, 0 failed'),
                          ('signaltest', 'signal: all checks passed')]:
    marker = 'signal: complete' if command == 'signaltest' else expected
    result = subprocess.run(
        [str(build / 'test_driver'), '-c', '600000000', '-d', 'hd-split.img',
         '-i', command + '\\n', '-w', marker, '-I', 'exit\\n', '-x', expected],
        cwd=build, capture_output=True, timeout=60)
    (build / (command + '-split.log')).write_bytes(result.stdout + result.stderr)
    if result.returncode or b'FAIL' in result.stdout:
        sys.stdout.buffer.write(result.stdout + result.stderr)
        raise SystemExit(f'split {command}: failed')
    print(f'split {command}: passed', flush=True)
