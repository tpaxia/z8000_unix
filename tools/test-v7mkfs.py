#!/usr/bin/env python3
"""Verify V7 image contents across direct and indirect block boundaries."""
from pathlib import Path
import struct
import subprocess
import tempfile

builder = Path(__file__).resolve().with_name('v7mkfs')
with tempfile.TemporaryDirectory(prefix='v7mkfs-') as temporary:
    work = Path(temporary)
    sizes = [0, 10*512, 128*512, 138*512, 138*512+1, 267*512+17]
    payloads = [bytes((i*17+i//512) % 256 for i in range(size)) for size in sizes]
    lines = ['boot', '3200 96', 'd--755 0 0']
    for index, payload in enumerate(payloads):
        (work/f'p{index}').write_bytes(payload)
        lines.append(f'p{index} ---644 0 0 {work}/p{index}')
    (work/'proto').write_text('\n'.join(lines+['$', '']))
    subprocess.run([builder, work/'image', work/'proto'], check=True)
    disk = (work/'image').read_bytes()

    def file_data(number):
        offset = ((number+15)//8)*512 + ((number+15)%8)*64
        inode = disk[offset:offset+64]
        size = struct.unpack_from('>I', inode, 8)[0]
        addresses = [int.from_bytes(inode[12+3*i:15+3*i], 'big') for i in range(13)]
        def indirect(block, depth):
            assert block
            entries = struct.unpack('>128I', disk[block*512:(block+1)*512])
            for entry in entries:
                if not entry:
                    break
                if depth == 1:
                    yield entry
                else:
                    yield from indirect(entry, depth-1)
        blocks = addresses[:10]
        if size > 10*512:
            blocks += list(indirect(addresses[10], 1))
        if size > 138*512:
            blocks += list(indirect(addresses[11], 2))
        return b''.join(disk[b*512:(b+1)*512] for b in blocks)[:size]

    directory = file_data(2)
    entries = {directory[i+2:i+16].split(b'\0')[0].decode():
               int.from_bytes(directory[i:i+2], 'big')
               for i in range(0, len(directory), 16)}
    for index, payload in enumerate(payloads):
        assert file_data(entries[f'p{index}']) == payload, sizes[index]
    (work/'badproto').write_text('boot\n3200 96\nd--755 0 0\n'+'x'*4096+'\n')
    failed = subprocess.run([builder, work/'badimage', work/'badproto'], capture_output=True)
    assert failed.returncode != 0 and b'token too long' in failed.stderr
print('v7mkfs: six file sizes and oversized-token rejection passed')
