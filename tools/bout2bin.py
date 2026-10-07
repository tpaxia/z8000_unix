#!/usr/bin/env python3
"""Extract boot images from a.out.

0407 emits text+data. Split kernels emit text after the reserved 512-byte trap
area to handler.bin and initialized data to a separate handler-data.bin.

a.out header: 8 x 16-bit big-endian words (16 bytes)
  a_magic, a_text, a_data, a_bss, a_syms, a_entry, a_trsize, a_drsize
"""
import struct
import sys

def main():
    if len(sys.argv) not in (3, 4):
        print(f"usage: {sys.argv[0]} input text.bin [data.bin]", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1], 'rb') as f:
        data = f.read()

    if len(data) < 16:
        print("error: file too small for a.out header", file=sys.stderr)
        sys.exit(1)

    magic, tsize, dsize, bsize, ssize, entry, trsize, drsize = \
        struct.unpack('>8H', data[:16])

    if len(data) < 16 + tsize + dsize:
        sys.exit('error: truncated text/data image')

    if magic not in (0o407, 0o411):
        print(f"error: bad magic {oct(magic)}, expected 0407 or 0411", file=sys.stderr)
        sys.exit(1)

    if magic == 0o411:
        if len(sys.argv) != 4 or tsize < 512 or tsize > 65536 or dsize + bsize > 0xe000:
            sys.exit('error: invalid split kernel layout or missing data output')
        with open(sys.argv[3], 'wb') as f:
            f.write(data[16+tsize:16+tsize+dsize])
    if magic == 0o407 and sys.argv[1].endswith('handler.bout') and 0x200 + tsize + dsize + bsize > 0xe000:
        sys.exit('error: kernel overlaps MMU copy window at 0xe000')

    content = data[16 + (512 if magic == 0o411 else 0):16 + tsize + (dsize if magic == 0o407 else 0)]

    if sys.argv[1].endswith('handler.bout'):
        entries = struct.unpack('>6H', content[:12])
        if any(word & 0xff00 != 0xe800 for word in entries):
            sys.exit('error: fixed kernel entry table must contain six short unconditional jumps')

    with open(sys.argv[2], 'wb') as f:
        f.write(content)

    print(f"bout2bin: {sys.argv[1]} -> {sys.argv[2]}: "
          f"text={tsize} data={dsize} bss={bsize} ({len(content)} bytes)")

if __name__ == '__main__':
    main()
