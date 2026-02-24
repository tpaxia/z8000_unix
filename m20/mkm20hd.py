#!/usr/bin/env python3
"""Build an M20 PCOS-bootable hard disk image.

Creates an HD image with geometry record, VDB/extent sector, SAV-wrapped
bootloader, and raw test kernel at known sectors.

Layout matches the PCOS HD boot structure:
  Sector 0:  Geometry/boot record (cluster_size, VDB pointer, etc.)
  Sector 32: VDB/extent record (extent count + extent entries)
  Sector 33: Bootloader SAV file (SAV header + code)
  Sector 64: Test kernel (raw binary, loaded by bootloader)

Usage: python3 mkm20hd.py <bootloader.bin> <testkernel.bin> <output.img>
"""

import struct
import sys

SECTOR_SIZE = 256
TOTAL_SECTORS = 34560  # 6 heads * 180 cyl * 32 sec/track

# HD image layout (matching PCOS conventions)
GEOM_SECTOR = 0
VDB_SECTOR = 32       # VDB sector (pointed to by offset 0x2A in sector 0)
BOOT_SECTOR = 33      # Bootloader SAV file
KERNEL_SECTOR = 64    # Test kernel (raw binary)
KERNEL_SECTORS = 2


def make_geometry_sector():
    """Sector 0: geometry/boot record.

    Copied byte-for-byte from the working PCOS m20_pcos.img sector 0.
    The BIOS reads cluster_size from offset 0x08 and VDB sector from offset 0x2A.
    PCOS uses cluster_size=60 (BOOT_STARTSEC=60*192=11520) and VDB at sector 32.
    """
    # Exact bytes from working PCOS image sector 0
    sector = bytearray(SECTOR_SIZE)
    pcos_sec0 = bytes([
        0x02, 0x00, 0x00, 0xB3, 0x06, 0x20, 0x01, 0x00,
        0x00, 0x3C, 0x00, 0x38, 0x00, 0x88, 0x1E, 0x00,
        0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x01, 0x00, 0xB2, 0x00, 0x00, 0x00, 0xB2,
        0x00, 0x00, 0x00, 0xB2, 0x00, 0x00, 0x00, 0xB2,
        0x00, 0x00, 0x00, 0x20, 0x50, 0x43, 0x4F, 0x53,
        0x00, 0x00, 0x00, 0x13, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        # 0x40
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x01, 0xFF, 0xFF, 0x02, 0x00, 0x06, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    ])
    sector[:len(pcos_sec0)] = pcos_sec0
    return sector


def make_vdb_sector(code_len):
    """VDB/extent record.

    offset 0x00: validation longword (4 bytes) — must match the BIOS's computed
                 value from BOOT_SECCOUNT/BOOT_EXPCOUNT after loading.
    offset 0x04: extent_count (number of extents describing the boot file)
    offset 0x1A: first extent entry: 2-byte sector + 2-byte count + 2-byte unused

    The BIOS validation (bios.s lines 2712-2723) computes:
      expcount = (SAV_HEADER_SIZE + code_len) & 0xFF
      seccount_adj = seccount - 1  (if expcount != 0)
      validation = {0x00, high(seccount_adj), low(seccount_adj), low(expcount)}
    and compares this 32-bit value with the first longword of the VDB sector.
    """
    SAV_HEADER_SIZE = 0x18  # bytes before code data in SAV
    sector = bytearray(SECTOR_SIZE)

    # Compute validation longword
    expcount = (SAV_HEADER_SIZE + code_len) & 0xFF
    seccount = 1  # single-sector SAV
    seccount_adj = seccount - 1 if expcount != 0 else seccount
    validation = ((seccount_adj >> 8) << 16) | ((seccount_adj & 0xFF) << 8) | expcount
    struct.pack_into('>I', sector, 0x00, validation)

    struct.pack_into('>H', sector, 0x04, 1)      # extent count = 1
    struct.pack_into('>HHH', sector, 0x1A,
                     BOOT_SECTOR,                  # sector number = 33
                     1,                            # sector count = 1
                     0)                            # unused
    return sector


def make_sav_sector(code):
    """SAV absolute file wrapping the bootloader binary.

    SAV format:
      0x00: 0x0101 magic
      0x02: version string (10 bytes)
      0x0C: entry point (4 bytes, segmented: <2>:0000)
      0x10: descriptor count (2 bytes)
      0x12: load descriptor: target addr (4 bytes) + word count (2 bytes)
      0x18: code data
    """
    max_code = SECTOR_SIZE - 24  # header is 24 bytes
    if len(code) > max_code:
        print(f"Error: bootloader binary ({len(code)} bytes) exceeds "
              f"SAV sector capacity ({max_code} bytes)", file=sys.stderr)
        sys.exit(1)

    # Pad code to even length for word count
    if len(code) % 2:
        code = code + b'\x00'

    sav = bytearray(SECTOR_SIZE)
    # Magic
    sav[0:2] = b'\x01\x01'
    # Version string
    sav[2:12] = b'UNIX\x00\x00\x00\x00\x00\x00'
    # Entry point: segment 6, offset 0 (user memory, safe from boot_final)
    struct.pack_into('>HH', sav, 0x0C, 0x8600, 0x0000)
    # Descriptor count
    struct.pack_into('>H', sav, 0x10, 1)
    # Load descriptor: target <6>:0000, word count
    struct.pack_into('>HH', sav, 0x12, 0x8600, 0x0000)
    struct.pack_into('>H', sav, 0x16, len(code) // 2)
    # Code
    sav[0x18:0x18 + len(code)] = code
    return sav


def main():
    if len(sys.argv) != 4:
        print(f"Usage: {sys.argv[0]} <bootloader.bin> <testkernel.bin> <output.img>",
              file=sys.stderr)
        sys.exit(1)

    bootloader_path = sys.argv[1]
    kernel_path = sys.argv[2]
    output_path = sys.argv[3]

    with open(bootloader_path, 'rb') as f:
        bootloader_bin = f.read()
    with open(kernel_path, 'rb') as f:
        kernel_bin = f.read()

    max_kernel = KERNEL_SECTORS * SECTOR_SIZE
    if len(kernel_bin) > max_kernel:
        print(f"Error: test kernel ({len(kernel_bin)} bytes) exceeds "
              f"{KERNEL_SECTORS} sectors ({max_kernel} bytes)", file=sys.stderr)
        sys.exit(1)

    # Build the full disk image
    img = bytearray(TOTAL_SECTORS * SECTOR_SIZE)

    # Sector 0: geometry record
    geom = make_geometry_sector()
    img[GEOM_SECTOR * SECTOR_SIZE:(GEOM_SECTOR + 1) * SECTOR_SIZE] = geom

    # Sector 32: VDB/extent record (needs bootloader code length for validation)
    # Pad code to even length (same as make_sav_sector does)
    padded_len = len(bootloader_bin) + (len(bootloader_bin) % 2)
    vdb = make_vdb_sector(padded_len)
    img[VDB_SECTOR * SECTOR_SIZE:(VDB_SECTOR + 1) * SECTOR_SIZE] = vdb

    # Sector 33: bootloader SAV
    sav = make_sav_sector(bootloader_bin)
    img[BOOT_SECTOR * SECTOR_SIZE:(BOOT_SECTOR + 1) * SECTOR_SIZE] = sav

    # Sectors 64-65: test kernel (raw binary)
    off = KERNEL_SECTOR * SECTOR_SIZE
    img[off:off + len(kernel_bin)] = kernel_bin

    with open(output_path, 'wb') as f:
        f.write(img)

    print(f"HD image: {output_path} ({len(img)} bytes, {TOTAL_SECTORS} sectors)")
    print(f"  Geometry record:  sector {GEOM_SECTOR}")
    print(f"  VDB/extent:       sector {VDB_SECTOR}")
    print(f"  Bootloader SAV:   sector {BOOT_SECTOR} ({len(bootloader_bin)} bytes code)")
    print(f"  Test kernel:      sector {KERNEL_SECTOR}-{KERNEL_SECTOR + KERNEL_SECTORS - 1} "
          f"({len(kernel_bin)} bytes)")


if __name__ == '__main__':
    main()
