"""Compare a native a.out object with its independently checked Unidot output."""
import struct


def compare(unidot, aout):
    sections = {0: 'absolute'}
    bodies = {}
    relocations = []
    externals = []
    pos = 0
    while pos < len(unidot):
        kind, count = unidot[pos:pos+2]
        block = unidot[pos+2:pos+2+count]
        assert len(block) == count
        pos += count+2
        if kind == 3:
            at = 0
            while at < count:
                end = block.index(0, at+3)
                name = block[at+3:end].decode()
                sections[len(sections)] = name
                bodies[name] = bytearray()
                at = end+1
        elif kind in (4, 5):
            at = 0
            while at < count:
                end = block.index(0, at+5)
                if kind == 4 and block[at+4] == 255:
                    externals.append(block[at+5:end].decode())
                at = end+1
        elif kind == 6:
            offset = int.from_bytes(block[:4], 'little')
            name = sections[block[4]]
            length = block[5]
            body = bodies[name]
            if len(body) < offset+length:
                body.extend(bytes(offset+length-len(body)))
            body[offset:offset+length] = block[6:6+length]
            for at in range(6+length, count, 3):
                site = offset + block[at] - 6
                action = int.from_bytes(block[at+1:at+3], 'little')
                base = action & 0xfff
                width = {0x4000: 1, 0x2000: 2, 0xa000: 4}[action & 0xf000]
                target = ('external', externals[base-256]) if base >= 256 else ('section', sections[base])
                relocations.append((name, site, width, target))
        elif kind == 11:
            assert pos == len(unidot)
            break
        else:
            assert kind == 1, ('unexpected Unidot block', kind)
    magic, text, data, bss, symbols, entry, tr, dr = struct.unpack('>8H', aout[:16])
    assert magic == 0o407 and entry == 0
    assert len(aout) == 16+text+data+tr+dr+symbols
    sizes = {'__text': text, '__data': data, '__bss': bss}
    bases = {'__text': 0, '__data': text, '__bss': text+data}
    for name, body in bodies.items():
        assert sizes[name] == (len(body)+3) & ~3, (name, sizes[name], len(body))
        body.extend(bytes(sizes[name]-len(body)))
    for name, site, width, target in relocations:
        if target[0] == 'section':
            body = bodies[name]
            value = int.from_bytes(body[site:site+width], 'big') + bases[target[1]]
            body[site:site+width] = (value % (1 << (8*width))).to_bytes(width, 'big')
    assert aout[16:16+text] == bodies.get('__text', b'')
    assert aout[16+text:16+text+data] == bodies.get('__data', b'')
    names = []
    start = 16+text+data+tr+dr
    for at in range(start, len(aout), 12):
        names.append(aout[at:at+8].split(b'\0')[0].decode())
    actual = []
    start = 16+text+data
    for section, length in [('__text', tr), ('__data', dr)]:
        for at in range(start, start+length, 8):
            info, index, site = struct.unpack('>HHI', aout[at:at+8])
            assert not (info & 0x0fff)
            base = info >> 14
            target = ('external', names[index]) if base == 3 else ('section', ('__text', '__data', '__bss')[base])
            actual.append((section, site, 1 << ((info >> 12) & 3), target))
        start += length
    assert sorted(actual) == sorted(relocations)
