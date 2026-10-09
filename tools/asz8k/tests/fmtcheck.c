#include <stdio.h>
#include "soutfmt.h"

main()
{
    char buf[SO_HEAD];
    int fd, i, tag;
    long n;
    fd = creat("format.bin", 0644);
    if (fd < 0) return 1;
    so_header(buf, SO_SMAG, 0x123456L, 0x789abcL, 48, 70,
        0x81203456L, 0x20);
    if (write(fd, buf, SO_HEAD) != SO_HEAD) return 2;
    so_segment(buf, 0x12, 0x1234, 0x5678, 0x9abc, SO_CODE|SO_BOUND);
    if (write(fd, buf, SO_SEG) != SO_SEG) return 3;
    so_symbol(buf, 0x81203456L, SO_TEXTSYM|SO_EXTERNAL|SO_SEGMENTED,
        0x12, "eightchr");
    if (write(fd, buf, SO_SYM) != SO_SYM) return 4;
    for (i = 0; i <= SO_R16; i++) {
        tag = so_reloc(1, 1, 4095, 0, i);
        so_put16(buf, (unsigned)tag);
        tag = so_reloc(0, 1, 255, SO_BSSSYM, i);
        so_put16(buf+2, (unsigned)tag);
        if (write(fd, buf, 4) != 4) return 5;
    }
    close(fd);
    so_put32(buf, 0x12345678L);
    n = so_get32(buf);
    if (n != 0x12345678L || so_get16(buf+2) != 0x5678) return 6;
    so_put32(buf, -1L);
    if (so_get32(buf) != -1L) return 12;
    if (so_reloc(1, 1, 4096, 0, 0) != -1) return 7;
    if (so_reloc(0, 1, 256, SO_TEXTSYM, 0) != -1) return 8;
    if (so_reloc(0, 0, 0, SO_DATASYM, 0) != 4) return 9;
    if (so_reloc(0, 0, 0, SO_TEXTSYM, SO_RSHORT) != -1) return 10;
    if (so_reloc(0, 1, 0, SO_TEXTSYM, 5) != -1) return 11;
    return 0;
}
