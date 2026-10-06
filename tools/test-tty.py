"""Run terminal modes with input delivered only after settings are installed."""
import pathlib
import subprocess
import sys

build = pathlib.Path(sys.argv[1]).resolve()
for mode, typed in [('echo', 'drop@ab#c\\n'), ('cooked', 'drop@ab#c\\n'),
                    ('raw', 'Q'), ('break', '#')]:
    result = subprocess.run(
        [str(build / 'test_driver'), '-c', '400000000', '-d', 'hd-tty.img',
         '-i', f'ttytest {mode}\\n', '-w', 'tty: ready', '-I', typed,
         '-x', 'tty: all checks passed'], cwd=build, capture_output=True, timeout=60)
    output = result.stdout
    if result.returncode or b'tty: FAIL' in output:
        sys.stdout.buffer.write(output + result.stderr)
        raise SystemExit(f'tty {mode}: failed')
    # Check the captured console transcript; the driver also streams output live.
    transcript = output.split(b'Console output: "', 1)[1]
    if mode in ('echo', 'cooked'):
        echoes = [token in transcript for token in (b'drop@', b'ab#c')]
        if echoes != [mode == 'echo'] * 2:
            raise SystemExit(f'tty {mode}: incorrect echo behavior')
    if mode == 'raw' and b'rawbytes:\x80\xff:end' not in output:
        raise SystemExit('tty raw: high-bit output changed or lost')
    print(f'tty {mode}: passed')
