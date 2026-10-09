#include "object.h"

int
objbytes(o, off, p, n)
struct object *o;
long off;
char *p;
int n;
{
    if (off < 0 || off > o->length || n > o->length-off) return 0;
    return fseek(o->file, o->origin+off, 0) == 0 &&
        fread(p, 1, n, o->file) == n;
}

/* 0: another format; -1: malformed/unsupported recognized object; 1: valid.
 * Every seek is bounded by the containing file or archive member. */
int
objread(o, f, origin, length)
struct object *o;
FILE *f;
long origin, length;
{
    char h[24], b[16];
    unsigned sb, sy, attr, code, data, bss;
    long image, total, bs, expected;
    int i;
    o->file = f; o->origin = origin; o->length = length;
    o->text = o->data = o->bss = 0;
    if (!objbytes(o, 0L, h, 2)) return -1;
    o->magic = so_get16(h);
    o->sout = o->magic == SO_SMAG || o->magic == SO_NMAG ||
        o->magic == SO_SID || o->magic == SO_NID;
    o->segmented = o->magic == SO_SMAG || o->magic == SO_SID;
    if (!o->sout) {
        /* Reject obsolete objects, including archive members, without
         * interpreting their headers. Ordinary archive data is skippable. */
        return o->magic == 0407 || o->magic == 0410 ||
            o->magic == 0411 || o->magic == 0405 ? -1 : 0;
    }
    if (!objbytes(o, 0L, h, 24)) return -1;
    sb = so_get16(h+10); sy = so_get16(h+12);
    o->flags = so_get16(h+18);
    if (!sb || sb%16 || sy%14 || (o->flags & ~SO_STRIP) ||
        so_get16(h+20) || so_get16(h+22)) return -1;
    o->segments = sb/16;
    if (o->segments > 256 || (!o->segmented && o->segments != 1)) return -1;
    image = so_get32(h+2); bs = so_get32(h+6);
    if (image < 0 || image > 16777216L || bs < 0 || bs > 16777216L) return -1;
    total = 0;
    for (i = 0; i < o->segments; i++) {
        if (!objbytes(o, 24+i*16L, b, 16)) return -1;
        attr = so_get16(b+10);
        code = so_get16(b+4); data = so_get16(b+6); bss = so_get16(b+8);
        /* Current assembler/linker contract: no code/data offsets or lines. */
        if ((attr & ~0xc7) || b[1] || b[2] ||
            (b[3] && !(attr & 64)) || so_get32(b+12) ||
            ((code | data | bss) & 1) ||
            (!o->segmented && ((b[0]&255) || (attr & ~7))) ||
            (o->segmented && !(attr & SO_BOUND) && (b[0]&255) != i) ||
            (o->segmented && (attr & SO_BOUND) && (b[0]&255) > 127) ||
            ((attr & 64) && (!(attr & SO_BOUND) || !bss || (b[3]&255)*256L != data)) ||
            (code && !(attr & SO_CODE)) || (data && !(attr & SO_DATA)) ||
            (bss && !(attr & SO_BSS))) return -1;
        o->text += code; o->data += data; o->bss += bss;
        total += code+ (long)data;
    }
    if (total != image || o->bss != bs) return -1;
    o->imageoff = 24L+sb; o->keep = o->imageoff+image;
    o->symoff = o->keep+((o->flags & SO_STRIP) ? 0 : image);
    expected = o->symoff+sy;
    o->symsize = 14; o->symbols = sy/14;
    return expected == length ? 1 : -1;
}

int
objsym(o, index, s)
struct object *o;
unsigned index;
struct osymbol *s;
{
    char b[16];
    unsigned kind, attr;
    long base, size;
    int i;
    if (index >= o->symbols || !objbytes(o,
        o->symoff+index*(long)o->symsize, b, o->symsize)) return 0;
    s->value = (unsigned long)so_get32(b) & 0xffffffffL; s->type = b[4]&255;
    s->segment = b[5]&255;
    for (i = 0; i < 8; i++) s->name[i] = b[6+i];
    kind = s->type & 31;
    if ((s->type & 128) || kind > 4 || (s->name[0]&128) || !s->name[0] ||
        (kind != SO_ABS && s->value > 65535L) ||
        (!kind && !(s->type & SO_EXTERNAL)) ||
        (kind != SO_ABS && !!(s->type & SO_SEGMENTED) != o->segmented)) return 0;
    if (kind >= SO_TEXTSYM) {
        if (s->segment >= o->segments ||
            !objbytes(o, 24+s->segment*16L, b, 16)) return 0;
        attr = so_get16(b+10);
        size = so_get16(b+4+(kind-2)*2);
        base = 0;
        if (!o->segmented) {
            if (kind >= SO_DATASYM && o->magic == SO_NMAG) base += so_get16(b+4);
            if (kind == SO_BSSSYM) base += so_get16(b+6);
        } else if (kind == SO_BSSSYM && (attr & 64)) base = (b[3]&255)*256L;
        if (s->value < base || s->value > base+size) return 0;
        /* Display the physical segment for bound executables, logical
         * segment for relocatable objects. Values are section offsets. */
        if (attr & SO_BOUND) s->segment = b[0]&255;
    }
    return 1;
}

/* Update only format fields. Caller copies the validated initialized image. */
int
objstrip(o, p)
struct object *o;
char *p;
{
    int n;
    n = 24;
    if (!objbytes(o, 0L, p, n)) return 0;
    so_put16(p+12, 0); so_put16(p+18, o->flags | SO_STRIP);
    return n;
}
