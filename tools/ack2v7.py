#!/usr/bin/env python3
"""Convert ACK .out binary to V7 a.out format (big-endian, magic 0407).

ACK .out header (little-endian):
  oh_magic(2) oh_stamp(2) oh_flags(2) oh_nsect(2)
  oh_nrelo(2) oh_nname(2) oh_nemit(4) oh_nchar(4)
  = 20 bytes

ACK section header (little-endian, 20 bytes each):
  os_base(4) os_size(4) os_foff(4) os_flen(4) os_lign(4)

V7 a.out header (big-endian, 16 bytes):
  ux_mag(2) ux_tsize(2) ux_dsize(2) ux_bsize(2)
  ux_ssize(2) ux_entloc(2) ux_unused(2) ux_relflg(2)
"""

import struct
import sys

ACK_MAGIC = 0x0202
V7_MAGIC = 0o407  # 0x0107

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} input.out output", file=sys.stderr)
        sys.exit(1)

    infile, outfile = sys.argv[1], sys.argv[2]

    with open(infile, 'rb') as f:
        data = f.read()

    # Parse ACK header (little-endian)
    if len(data) < 20:
        print("Error: file too small for ACK header", file=sys.stderr)
        sys.exit(1)

    magic, stamp, flags, nsect, nrelo, nname, nemit, nchar = \
        struct.unpack_from('<HHHHHH II', data, 0)

    if magic != ACK_MAGIC:
        print(f"Error: bad ACK magic 0x{magic:04X} (expected 0x{ACK_MAGIC:04X})",
              file=sys.stderr)
        sys.exit(1)

    print(f"  ACK header: nsect={nsect} nrelo={nrelo} nname={nname} nemit={nemit}")

    # Parse section headers (little-endian)
    sections = []
    for i in range(nsect):
        off = 20 + i * 20
        os_base, os_size, os_foff, os_flen, os_lign = \
            struct.unpack_from('<IIIII', data, off)
        sections.append({
            'base': os_base, 'size': os_size,
            'foff': os_foff, 'flen': os_flen, 'lign': os_lign
        })
        print(f"  sect[{i}]: base=0x{os_base:04X} size={os_size} "
              f"foff=0x{os_foff:04X} flen={os_flen}")

    # Entry point is section 0 base
    entry = sections[0]['base'] if sections else 0

    # Compute total loaded size and BSS
    # Find the extent of all sections with file data
    load_end = 0
    total_end = 0
    for s in sections:
        if s['flen'] > 0:
            end = s['base'] + s['flen']
            if end > load_end:
                load_end = end
        end = s['base'] + s['size']
        if end > total_end:
            total_end = end

    dsize = load_end    # data size = all loaded bytes
    bsize = total_end - load_end  # BSS = remainder

    print(f"  Entry: 0x{entry:04X}")
    print(f"  Data (merged): {dsize} bytes")
    print(f"  BSS: {bsize} bytes")

    # Build the merged program data
    program = bytearray(dsize)
    for s in sections:
        if s['flen'] > 0:
            src = data[s['foff']:s['foff'] + s['flen']]
            off = s['base'] - entry  # relative to entry point
            program[off:off + len(src)] = src

    # Write V7 a.out: 16-byte big-endian header + program data
    hdr = struct.pack('>HHHHHHHH',
                      V7_MAGIC,   # ux_mag
                      0,          # ux_tsize (0 for 0407)
                      dsize,      # ux_dsize
                      bsize,      # ux_bsize
                      0,          # ux_ssize
                      entry,      # ux_entloc
                      0,          # ux_unused
                      0)          # ux_relflg

    with open(outfile, 'wb') as f:
        f.write(hdr)
        f.write(program)

    print(f"  Output: {outfile} ({len(hdr) + len(program)} bytes)")

if __name__ == '__main__':
    main()
