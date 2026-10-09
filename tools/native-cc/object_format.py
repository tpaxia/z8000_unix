"""Host-side summaries of NONSEG s.out and V7 terminal data resources."""
import struct


def terminal_sizes(data):
    # nroff consumes this original data layout; it is not an executable.
    assert struct.unpack_from('>H',data)[0]==0o411
    return dict(zip(('text','data','bss'),struct.unpack_from('>3H',data,2)))


def sizes(data, sout=True):
    if not sout:
        raise ValueError('obsolete executable format: use s.out')
    magic=struct.unpack_from('>H',data)[0]
    assert magic in (0xe707,0xe711),hex(magic)
    image,bss,segments,symbols=struct.unpack_from('>IIHH',data,2)
    assert segments==16 and struct.unpack_from('>H',data,18)[0]==1
    text,initialized,uninitialized=struct.unpack_from('>3H',data,28)
    assert text+initialized==image and uninitialized==bss
    assert len(data)==40+image+symbols
    return dict(zip(('text','data','bss'),(text,initialized,uninitialized)))
