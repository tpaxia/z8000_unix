/* Direct ZEUS s.out relocatable output. The existing encoder supplies Unidot
 * actions; this backend translates them, without changing instruction choice.
 * Initial section contract: __text, __data, __bss; no fixed/common sections.
 */
#include <stdio.h>
#include "acom.h"
#include "obj.h"
#include "soutfmt.h"

int zflag;
static int kinds[SECSIZ], segments[3], nseg, symbols, symfd, relfd;
static int present[3];
static long lengths[SECSIZ], sizes[3], bases[3], files[3], nextbyte[3];
static long image, start, outpos, records;

static int
fail(message)
char *message;
{
    fprintf(ERROR, "s.out: %s\n", message);
    exit(1);
    return 0;
}

static int
tempfile(tag)
int tag;
{
    char name[30];
    int fd;
    sprintf(name, "/tmp/so%d%c", getpid(), tag);
    fd = creat(name, 0600);
    if (fd < 0) fail("cannot create temporary file");
    close(fd);
    fd = open(name, 2);
    unlink(name);
    if (fd < 0) fail("cannot open temporary file");
    return fd;
}

static int
seekout(pos)
long pos;
{
    if (fseek(OBJECT, pos, 0)) fail("output seek failed");
    outpos = pos;
    return 0;
}

static int
putbyte(value)
int value;
{
    putc(value & 255, OBJECT);
    if (ferror(OBJECT)) fail("output write failed");
    outpos++;
    return 0;
}

static int
putblock(p, n)
char *p;
int n;
{
    if (fwrite(p, 1, n, OBJECT) != n) fail("output write failed");
    outpos += n;
    return 0;
}

static int
putrel(pos, value)
long pos;
unsigned value;
{
    char bytes[2];
    so_put16(bytes, value);
    seekout(start + image + pos);
    putblock(bytes, 2);
    return 0;
}

sobegin()
{
    unsigned sec, h;
    vmadr p, next;
    struct sytab *s;
    int kind, global, i, type, seg;
    long value;
    char entry[SO_SYM];

    for (sec = 1; sec < secct; sec++) {
        s = (struct sytab *)rfetch(sectab[sec].se_sym);
        if (!strncmp(s->sy_str, "__text", 8)) kind = 0;
        else if (!strncmp(s->sy_str, "__data", 8)) kind = 1;
        else if (!strncmp(s->sy_str, "__bss", 8)) kind = 2;
        else fail("only __text, __data and __bss sections are supported");
        if (sectab[sec].se_atr & (SECOM|SEFIX)) fail("common/fixed sections are unsupported");
        if (sectab[sec].se_aln > 1) fail("section alignment exceeds two bytes");
        kinds[sec] = kind + SO_TEXTSYM;
        present[kind] = 1;
        lengths[sec] = sectab[sec].se_loc;
        sizes[kind] = (lengths[sec] + 1) & ~1L;
        if (sizes[kind] > 65535L) fail("section size overflow");
    }
    if (objectseg) {
        /* Each present section is an independent, unbound logical segment.
         * The linker chooses final physical segment numbers and placement. */
        for (i = 0; i < 3; i++) if (present[i]) segments[i] = nseg++;
    } else {
        nseg = 1;
        bases[1] = sizes[0];
        bases[2] = sizes[0] + sizes[1];
        if (bases[2] + sizes[2] > 65535L) fail("combined object exceeds 65535 bytes");
    }
    image = sizes[0] + sizes[1];
    start = SO_HEAD + (long)nseg * SO_SEG;
    files[1] = sizes[0];
    symfd = tempfile('s');
    relfd = tempfile('r');
    for (h = 0; h < (1<<SHSHLOG); h++) {
        for (p = syhtab[h]; p; p = next) {
            s = (struct sytab *)rfetch(p);
            next = s->sy_lnk;
            if (s->sy_typ == STKEY || s->sy_typ == STSEC) continue;
            if (pccflg && !strcmp(s->sy_str,".")) continue;
            global = (s->sy_atr & SAGLO) || (uext && s->sy_typ == STUND);
            value = s->sy_val;
            seg = 0;
            if (s->sy_typ == STUND || s->sy_typ == STCOM) {
                if (!global) fail("undefined local symbol");
                if (symbols + RBEXT >= RBMSK) fail("too many symbols");
                type = SO_UNDEF;
                if (s->sy_typ == STUND) value = 0;
                s = (struct sytab *)wfetch(p);
                s->sy_typ = STLAB;
                s->sy_atr |= SAGLO;
                s->sy_rel = RBEXT + symbols;
                s->sy_val = 0;
            } else if (s->sy_rel == RBABS) type = SO_ABS;
            else {
                if (s->sy_rel >= SECSIZ || !kinds[s->sy_rel]) fail("invalid symbol section");
                type = kinds[s->sy_rel];
                kind = type - SO_TEXTSYM;
                if (value < 0 || value > sizes[kind]) fail("symbol value overflow");
                value += bases[kind];
                seg = segments[kind];
            }
            /* Recovered ZEUS scrt0.o stores section-relative symbol offsets,
             * not encoded long addresses. sn_segt selects the segment. */
            if (objectseg && type != SO_ABS) type |= SO_SEGMENTED;
            so_symbol(entry, value, type | (global ? SO_EXTERNAL : 0), seg, s->sy_str);
            if (write(symfd, entry, SO_SYM) != SO_SYM) fail("symbol write failed");
            symbols++;
            if ((long)symbols * SO_SYM > 65535L) fail("symbol table too large");
        }
    }
    for (value = 0; value < start + 2*image; value++) putbyte(0);
}

sobyte(value, reloc)
unsigned value, reloc;
{
    int kind, width, external, action, tag;
    unsigned base, op, index, type;
    long pos;
    char record[8];

    if (!cursec || cursec >= SECSIZ || !kinds[cursec]) fail("absolute output is unsupported");
    kind = kinds[cursec] - SO_TEXTSYM;
    if (kind == 2) fail("initialized bytes in BSS");
    if (curloc < nextbyte[kind] || curloc >= lengths[cursec]) fail("overlapping output or pass size mismatch");
    pos = files[kind] + curloc;
    base = reloc & RBMSK;
    op = reloc & RAMSK;
    if (base && op) {
        width = 2;
        switch (op) {
        case RAA16M: case RAZOF: action = SO_ROFF; break;
        case RAZSS: action = SO_RSHORT; break;
        case RAZLS: action = SO_RSEG; width = 4; break;
        default: fail("relocation cannot be represented in Z8000 s.out");
        }
        if ((curloc & 1) || curloc + width > lengths[cursec]) fail("unaligned or out-of-range relocation");
        external = base >= RBEXT;
        if (external) {
            index = base - RBEXT;
            type = SO_UNDEF;
            if (index >= symbols) fail("bad external symbol index");
        } else {
            if (base >= SECSIZ || !kinds[base]) fail("bad relocation section");
            type = kinds[base];
            index = segments[type - SO_TEXTSYM];
        }
        tag = so_reloc(external, objectseg, index, type, action);
        if (tag == -1) fail("unrepresentable relocation");
        putrel(pos, (unsigned)tag);
        if (width == 4) {
            tag = so_reloc(external, objectseg, index, type, SO_ROFF);
            if (tag == -1) fail("unrepresentable offset relocation");
            putrel(pos+2, (unsigned)tag);
        }
        so_put16(record, reloc);
        so_put16(record+2, kind);
        so_put32(record+4, curloc);
        if (write(relfd, record, 8) != 8) fail("relocation write failed");
        records++;
    }
    seekout(start + pos);
    putbyte((int)value);
    nextbyte[kind] = curloc + 1;
}

sofinish()
{
    int i, kind, width, c;
    unsigned op, base, seg, v;
    long count, pos, add;
    unsigned long value;
    char record[SO_HEAD];

    if (fflush(OBJECT)) fail("output flush failed");
    if (lseek(relfd, 0L, 0) < 0) fail("relocation seek failed");
    for (count = records; count; count--) {
        if (read(relfd, record, 8) != 8) fail("relocation read failed");
        base = so_get16(record) & RBMSK;
        op = so_get16(record) & RAMSK;
        kind = so_get16(record+2);
        pos = start + files[kind] + so_get32(record+4);
        width = op == RAZLS ? 4 : 2;
        seekout(pos);
        value = 0;
        for (i = 0; i < width; i++) {
            c = getc(OBJECT);
            if (c == EOF) fail("image read failed");
            value = (value << 8) | c;
        }
        seg = 0;
        add = 0;
        if (base < RBEXT) {
            kind = kinds[base] - SO_TEXTSYM;
            seg = segments[kind];
            add = bases[kind];
        }
        if (op == RAZLS) {
            value = (value & 65535L) | ((unsigned long)(0x8000 | (seg << 8)) << 16);
        } else if (op == RAZSS) {
            value = (value & 255) | (seg << 8);
        } else {
            /* Word addends are stored modulo 65536, including expressions
             * such as a BSS symbol minus two. Match a.out word arithmetic. */
            value = (value + add) & 65535L;
        }
        seekout(pos);
        for (i = width-1; i >= 0; i--) putbyte((int)(value >> (8*i)));
    }
    seekout(start + 2*image);
    if (lseek(symfd, 0L, 0) < 0) fail("symbol seek failed");
    for (i = 0; i < symbols; i++) {
        if (read(symfd, record, SO_SYM) != SO_SYM) fail("symbol read failed");
        putblock(record, SO_SYM);
    }
    seekout(0L);
    so_header(record, objectseg ? SO_SMAG : SO_NMAG,
        image, sizes[2], nseg * SO_SEG, symbols * SO_SYM, 0L, 0);
    putblock(record, SO_HEAD);
    if (!objectseg) {
        v = (sizes[0] ? SO_CODE : 0) | (sizes[1] ? SO_DATA : 0) | (sizes[2] ? SO_BSS : 0);
        so_segment(record, 0, (unsigned)sizes[0], (unsigned)sizes[1], (unsigned)sizes[2], v);
        putblock(record, SO_SEG);
    } else for (i = 0; i < 3; i++) if (present[i]) {
        so_segment(record, segments[i], i == 0 ? (unsigned)sizes[i] : 0,
            i == 1 ? (unsigned)sizes[i] : 0, i == 2 ? (unsigned)sizes[i] : 0, 1 << i);
        putblock(record, SO_SEG);
    }
    close(symfd);
    close(relfd);
}

socheck()
{
    if (zflag && (curloc < 0 || curloc > 65535L)) fail("section size overflow");
}
