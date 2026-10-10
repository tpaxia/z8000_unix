#!/usr/bin/env python3
"""Extract the split s.out kernel's instruction and data images."""
import struct
import sys
from pathlib import Path


def kernel_image(data, data_limit=0xe000):
    if len(data) < 40:
        raise ValueError('truncated s.out header')
    magic, image, bss, segments, symbols, entry, flags = struct.unpack_from('>HIIHHIH', data)
    text, initialized, uninitialized = struct.unpack_from('>3H', data, 28)
    if magic != 0xe711 or segments != 16 or flags != 1:
        raise ValueError('kernel must be a linked split NONSEG s.out')
    if image != text + initialized or bss != uninitialized or len(data) != 40 + image + symbols:
        raise ValueError('invalid s.out image lengths')
    if text < 524 or initialized + uninitialized > data_limit:
        raise ValueError('invalid split kernel layout')
    code = data[40:40 + text]
    if any(word & 0xff00 != 0xe800 for word in struct.unpack_from('>6H', code, 512)):
        raise ValueError('fixed kernel entry table must contain six short unconditional jumps')
    return code, data[40 + text:40 + image], uninitialized


if __name__ == '__main__':
    if len(sys.argv) not in (4, 5):
        sys.exit('usage: sout2bin.py input.sout text.bin data.bin [data-limit]')
    try:
        limit = int(sys.argv[4], 0) if len(sys.argv) == 5 else 0xe000
        code, initialized, bss = kernel_image(Path(sys.argv[1]).read_bytes(), limit)
    except ValueError as error:
        sys.exit(str(error))
    Path(sys.argv[2]).write_bytes(code[512:])
    Path(sys.argv[3]).write_bytes(initialized)
    print('sout2bin: text=%d data=%d bss=%d' % (len(code), len(initialized), bss))
