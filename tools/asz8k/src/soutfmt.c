#include "soutfmt.h"

unsigned
so_get16(p)
char *p;
{
    return ((unsigned)(p[0] & 255) << 8) | (p[1] & 255);
}

long
so_get32(p)
char *p;
{
    long high;
    high = so_get16(p);
    if (high & 0x8000L) high -= 65536L;
    return high * 65536L + so_get16(p+2);
}

int
so_put16(p, value)
char *p;
unsigned value;
{
    p[0] = value >> 8;
    p[1] = value;
    return 0;
}

int
so_put32(p, value)
char *p;
long value;
{
    unsigned long bits;
    bits = value;
    so_put16(p, (unsigned)(bits >> 16));
    so_put16(p+2, (unsigned)value);
    return 0;
}

int
so_header(p, magic, image, bss, segments, symbols, entry, flags)
char *p;
unsigned magic, segments, symbols, flags;
long image, bss, entry;
{
    int i;
    for (i = 0; i < SO_HEAD; i++) p[i] = 0;
    so_put16(p, magic);
    so_put32(p+2, image);
    so_put32(p+6, bss);
    so_put16(p+10, segments);
    so_put16(p+12, symbols);
    so_put32(p+14, entry);
    so_put16(p+18, flags);
    return 0;
}

int
so_segment(p, number, code, data, bss, attrs)
char *p;
unsigned number, code, data, bss, attrs;
{
    int i;
    for (i = 0; i < SO_SEG; i++) p[i] = 0;
    p[0] = number;
    so_put16(p+4, code);
    so_put16(p+6, data);
    so_put16(p+8, bss);
    so_put16(p+10, attrs);
    return 0;
}

int
so_symbol(p, value, type, segment, name)
char *p, *name;
long value;
unsigned type, segment;
{
    int i;
    so_put32(p, value);
    p[4] = type;
    p[5] = segment;
    for (i = 0; i < 8; i++) p[6+i] = 0;
    for (i = 0; i < 8 && name[i]; i++) p[6+i] = name[i];
    return 0;
}

/* Return -1 instead of silently truncating a relocation's index or type.
 * Resolved type is N_CODE/N_DATA/N_BSS; externals use a symbol-table index.
 */
int
so_reloc(external, segmented, index, type, action)
int external, segmented;
unsigned index, type, action;
{
    if (action > SO_R16) return -1;
    if (external) {
        if (index > 4095) return -1;
        return (index << 4) | SO_REXT | action;
    }
    if (type < SO_TEXTSYM || type > SO_BSSSYM) return -1;
    type = (type-1) << 1;
    if (!segmented) {
        if (index || action) return -1;
        return type;
    }
    if (index > 255) return -1;
    return (index << 8) | (action << 4) | type | 1;
}
