"""Host-side resource summaries for the two supported NONSEG file layouts."""
import struct


def sizes(data, sout=False):
    magic=struct.unpack_from('>H',data)[0]
    if sout:
        assert magic==0xe711,hex(magic)
        image,bss,segments,symbols=struct.unpack_from('>IIHH',data,2)
        assert segments==16 and struct.unpack_from('>H',data,18)[0]==1
        text,initialized,uninitialized=struct.unpack_from('>3H',data,28)
        assert text+initialized==image and uninitialized==bss
        assert len(data)==40+image+symbols
    else:
        assert magic==0o411,hex(magic)
        text,initialized,uninitialized=struct.unpack_from('>3H',data,2)
    return dict(zip(('text','data','bss'),(text,initialized,uninitialized)))
