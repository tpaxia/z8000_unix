"""Check V7 shell interpretation of normal and signal wait statuses."""
import pathlib
import subprocess
import sys

build = pathlib.Path(sys.argv[1]).resolve()
for mode, status, signalled in [('exit', 15, False), ('term', 143, True)]:
    commands = f'signaltest {mode}\\necho status:$?\\nexit\\n'
    result = subprocess.run(
        [str(build / 'test_driver'), '-c', '400000000', '-d', 'hd-preempt.img',
         '-i', commands, '-x', f'status:{status}'],
        cwd=build, capture_output=True, timeout=60)
    output = result.stdout
    (build / f'signal-shell-{mode}.log').write_bytes(output + result.stderr)
    transcript = output.split(b'Console output: "', 1)[-1].split(b'"', 1)[0]
    if (result.returncode or f'status:{status}\\r\\n'.encode() not in transcript
            or (b'Terminated' in transcript) != signalled
            or b'core dumped' in transcript):
        sys.stdout.buffer.write(output + result.stderr)
        raise SystemExit(f'signal shell {mode}: failed')
    print(f'signal shell {mode}: status {status}, reporting passed')
