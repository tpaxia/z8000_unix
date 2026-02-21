#!/usr/bin/env python3
"""Convert b.out (PCC/az8 linker output) to V7 a.out format.

b.out header: 8 x 32-bit BE longs (32 bytes)
  magic, tsize, dsize, bsize, ssize, rtsize, rdsize, entry

V7 a.out header (struct exec): 8 x 16-bit BE words (16 bytes)
  a_magic, a_text, a_data, a_bss, a_syms, a_entry, a_unused, a_flag

For 0407 (FMAGIC): text and data are contiguous, no relocation.
We merge text+data into the data segment (a_text=0, a_data=tsize+dsize)
since exec() does `ux_dsize += ux_tsize; ux_tsize = 0` anyway.
"""
import struct
import sys

def main():
    if len(sys.argv) != 3:
        print(f"usage: {sys.argv[0]} input.bout output", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1], 'rb') as f:
        data = f.read()

    if len(data) < 32:
        print("error: file too small for b.out header", file=sys.stderr)
        sys.exit(1)

    # Parse b.out header (8 x 32-bit big-endian)
    magic, tsize, dsize, bsize, ssize, rtsize, rdsize, entry = \
        struct.unpack('>8I', data[:32])

    if magic != 0o407:
        print(f"error: bad magic {oct(magic)}, expected 0407", file=sys.stderr)
        sys.exit(1)

    # Extract text+data content (skip 32-byte header)
    content = data[32:32 + tsize + dsize]

    # Build V7 a.out header: merge text into data for 0407
    a_magic = 0o407
    a_text  = 0
    a_data  = tsize + dsize
    a_bss   = bsize
    a_syms  = 0
    a_entry = entry
    a_unused = 0
    a_flag  = 0

    hdr = struct.pack('>8H', a_magic, a_text, a_data, a_bss,
                      a_syms, a_entry, a_unused, a_flag)

    with open(sys.argv[2], 'wb') as f:
        f.write(hdr)
        f.write(content)

    total = tsize + dsize
    print(f"bout2v7: {sys.argv[1]} -> {sys.argv[2]}: "
          f"text={tsize} data={dsize} bss={bsize} entry={entry:#x} "
          f"({len(hdr) + len(content)} bytes)")

if __name__ == '__main__':
    main()
