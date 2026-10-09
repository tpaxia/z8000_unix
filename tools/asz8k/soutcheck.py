"""Independent s.out layout checks and host/native serializer checks."""
from pathlib import Path
import struct
import subprocess

SOURCE = Path(__file__).resolve().parent


def decode(data):
    magic, image, bss, segbytes, symbytes, entry, flags, codesz, lines = struct.unpack('>HIIHHIHHH', data[:24])
    assert magic in (0xe607, 0xe707, 0xe611, 0xe711)
    assert segbytes % 16 == 0 and symbytes % 14 == 0
    assert codesz == lines == entry == flags == 0
    start = 24 + segbytes
    assert len(data) == start + image * 2 + symbytes
    segments = [struct.unpack('>4B4HI', data[p:p+16]) for p in range(24, start, 16)]
    assert sum(s[4]+s[5] for s in segments) == image
    assert sum(s[6] for s in segments) == bss
    symbols = [struct.unpack('>IBB8s', data[p:p+14]) for p in range(start+2*image, len(data), 14)]
    reloc = struct.unpack('>%dH' % (image//2), data[start+image:start+2*image])
    for tag in reloc:
        if tag & 8:
            assert tag & 7 <= 4
            assert tag >> 4 < len(symbols)
        elif tag:
            assert (tag >> 1) & 3
            if tag & 1:
                assert tag >> 8 < len(segments)
                assert (tag >> 4) & 7 <= 4
            else:
                assert tag in (2, 4, 6)
    return segments, symbols, reloc


def check(work):
    golden = struct.pack('>HIIHHIHHH', 0xe607, 0x123456, 0x789abc, 48, 70, 0x81203456, 32, 0, 0)
    golden += struct.pack('>4B4HI', 0x12, 0, 0, 0, 0x1234, 0x5678, 0x9abc, 129, 0)
    golden += struct.pack('>IBB8s', 0x81203456, 98, 0x12, b'eightchr')
    golden += b''.join(struct.pack('>HH', 0xfff8+i, 0xff07+(i<<4)) for i in range(5))
    subprocess.run(['cc', '-std=gnu89', '-w', '-I'+str(SOURCE/'src'),
                    str(SOURCE/'tests/fmtcheck.c'), str(SOURCE/'src/soutfmt.c'),
                    '-o', str(work/'hostfmt')], check=True)
    subprocess.run([str(work/'hostfmt')], cwd=work, check=True)
    assert (work/'format.bin').read_bytes() == golden
    assert (work/'nativefmt.bin').read_bytes() == golden
    for name in ('seg', 'soutseg', 'soutnon'):
        data = (work/(name+'.so')).read_bytes()
        segments, symbols, reloc = decode(data)
        if name == 'soutseg':
            assert len(segments) == 3
            assert [(s[4], s[5], s[6]) for s in segments] == [(22, 0, 0), (0, 6, 0), (0, 0, 4)]
            index = next(i for i, s in enumerate(symbols) if s[3].rstrip(b'\0') == b'external')
            assert reloc == (0, index*16+9, index*16+8, 0, 0x125, 0, index*16+10, 0x115, 0x105,
                             0x207, 0, 3, index*16+9, index*16+8)
            assert data[24+48+8:24+48+10] == bytes.fromhex('0100')
            assert data[24+48+14:24+48+20] == bytes.fromhex('810000000000')
            assert data[24+48+24:24+48+28] == bytes.fromhex('80000004')
            for value, type_, segment, symbol in symbols:
                if symbol.rstrip(b'\0') in (b'entry', b'datum', b'saved'):
                    assert value == 0 and type_ & 64
        if name == 'soutnon':
            assert len(segments) == 1 and segments[0][4:7] == (10, 8, 4)
            index = next(i for i, s in enumerate(symbols) if s[3].rstrip(b'\0') == b'external')
            assert reloc == (0, index*16+8, 0, 4, 0, 2, 6, index*16+8, 6)
            assert data[40+6:40+8] == bytes.fromhex('000a')
            assert data[40+12:40+14] == bytes.fromhex('0012')
            assert data[40+16:40+18] == bytes.fromhex('0010')
    print('PASS: independent s.out layout/relocation checks and host/native serializer bytes')
