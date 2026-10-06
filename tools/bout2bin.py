#!/usr/bin/env python3
"""Convert a.out to flat binary (strip 16-byte header, emit text+data).

Used for kernel handler.bin which is loaded at a fixed address.

a.out header: 8 x 16-bit big-endian words (16 bytes)
  a_magic, a_text, a_data, a_bss, a_syms, a_entry, a_trsize, a_drsize
"""
import struct
import sys

def main():
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} input output.bin", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1], 'rb') as f:
        data = f.read()

    if len(data) < 16:
        print("error: file too small for a.out header", file=sys.stderr)
        sys.exit(1)

    magic, tsize, dsize, bsize, ssize, entry, trsize, drsize = \
        struct.unpack('>8H', data[:16])

    if magic != 0o407:
        print(f"error: bad magic {oct(magic)}, expected 0407", file=sys.stderr)
        sys.exit(1)

    if sys.argv[1].endswith('handler.bout') and 0x200 + tsize + dsize + bsize > 0xe000:
        sys.exit('error: kernel overlaps MMU copy window at 0xe000')

    content = data[16:16 + tsize + dsize]

    with open(sys.argv[2], 'wb') as f:
        f.write(content)

    print(f"bout2bin: {sys.argv[1]} -> {sys.argv[2]}: "
          f"text={tsize} data={dsize} bss={bsize} ({len(content)} bytes)")

if __name__ == '__main__':
    main()
