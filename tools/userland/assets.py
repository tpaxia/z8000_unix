"""Read the original V7 data archives (PDP-11 header byte order)."""
import struct
from pathlib import Path


def oldmembers(path):
    data = path.read_bytes()
    if data[:2] != b'\x65\xff':
        raise ValueError('bad V7 archive magic: '+str(path))
    offset = 2
    while offset < len(data):
        header = data[offset:offset+26]
        if len(header) != 26:
            raise ValueError('truncated V7 archive header')
        name = header[:14].split(b'\0')[0].decode('ascii')
        if not name or Path(name).name != name or name in ('.', '..'):
            raise ValueError('invalid V7 archive member')
        high, low = struct.unpack('<HH', header[22:26])
        size = (high << 16) | low
        offset += 26
        if offset+size > len(data):
            raise ValueError('truncated V7 archive member')
        yield name, data[offset:offset+size]
        offset += size+(size & 1)
