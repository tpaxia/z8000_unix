#!/usr/bin/env python3
"""Convert b.out to flat binary (strip 32-byte header, emit text+data).

Used for kernel handler.bin which is loaded at a fixed address.
"""
import struct
import sys

def main():
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} input.bout output.bin", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1], 'rb') as f:
        data = f.read()

    if len(data) < 32:
        print("error: file too small for b.out header", file=sys.stderr)
        sys.exit(1)

    magic, tsize, dsize, bsize = struct.unpack('>4I', data[:16])

    if magic != 0o407:
        print(f"error: bad magic {oct(magic)}, expected 0407", file=sys.stderr)
        sys.exit(1)

    content = data[32:32 + tsize + dsize]

    with open(sys.argv[2], 'wb') as f:
        f.write(content)

    print(f"bout2bin: {sys.argv[1]} -> {sys.argv[2]}: "
          f"text={tsize} data={dsize} bss={bsize} ({len(content)} bytes)")

if __name__ == '__main__':
    main()
