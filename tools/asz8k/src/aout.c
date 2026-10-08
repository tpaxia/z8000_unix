/* Native V7 a.out output.  Instruction selection remains in asz8k.c.
 * Unidot output is retained unless -a is selected.  Disk fields are written
 * explicitly, rather than using the compiler's structure layout.
 */
#include <stdio.h>
#include "acom.h"
#include "obj.h"

int aflag;
static int stype[SECSIZ];
static long slen[SECSIZ], sizes[3], bases[3], nextbyte[3];
static long rsize[2], symsize, outpos;
static int rfile[2], symfile;

/* These are the on-disk values from PCC-z8000/z8000/b.out.h. */
#define AEXT 040
#define ATEXT 02
#define ADATA 03
#define ABSS 04
#define REXTERNAL 0xc000
#define RDATASEG 0x4000
#define RBSSSEG 0x8000

static
fail(message)
char *message;
{
    fprintf(ERROR, "a.out: %s\n", message);
    exit(1);
}

static
word(p, value)
char *p;
unsigned value;
{
    p[0] = value >> 8;
    p[1] = value;
}

static
putfile(fd, p, n)
int fd, n;
char *p;
{
    if (write(fd, p, n) != n) fail("output write failed");
}

static int
temporary(tag)
int tag;
{
    char name[30];
    int fd;
    sprintf(name, "/tmp/ao%d%c", getpid(), tag);
    fd = creat(name, 0600);
    if (fd < 0) fail("cannot create temporary file");
    close(fd);
    fd = open(name, 2);
    unlink(name);
    if (fd < 0) fail("cannot open temporary file");
    return fd;
}

static
seekout(pos)
long pos;
{
    if (pos != outpos && fseek(OBJECT, pos, 0)) fail("output seek failed");
    outpos = pos;
}

static
byteout(value)
int value;
{
    /* V7 putc can return -1 for a successfully buffered byte 0xff. */
    putc(value & 255, OBJECT);
    if (ferror(OBJECT)) fail("output write failed");
    outpos++;
}

/* Called between passes, while section sizes and symbol definitions still
 * describe the completed first pass.  Pass two resets section metadata.
 */
aobegin()
{
    unsigned sec, h, index;
    vmadr p, next;
    struct sytab *s;
    int kind, global, i;
    long value, total;
    char entry[12];

    if (segflg) fail("segmented a.out is not implemented");
    for (sec = 1; sec < secct; sec++) {
        s = (struct sytab *)rfetch(sectab[sec].se_sym);
        if (!strncmp(s->sy_str, "__text", 8)) kind = 0;
        else if (!strncmp(s->sy_str, "__data", 8)) kind = 1;
        else if (!strncmp(s->sy_str, "__bss", 8)) kind = 2;
        else fail("only __text, __data and __bss sections are supported");
        if (sectab[sec].se_atr & (SECOM|SEFIX))
            fail("common/fixed sections are not supported");
        if (sectab[sec].se_aln > 2) fail("section alignment exceeds four bytes");
        stype[sec] = kind + ATEXT;
        slen[sec] = sectab[sec].se_loc;
        sizes[kind] = (slen[sec] + 3) & ~3L;
    }
    bases[1] = sizes[0];
    bases[2] = sizes[0] + sizes[1];
    total = bases[2] + sizes[2];
    if (total > 65535L) fail("combined object sections exceed 65535 bytes");
    symfile = temporary('s');
    rfile[0] = temporary('t');
    rfile[1] = temporary('d');
    index = 0;
    for (h = 0; h < (1<<SHSHLOG); h++) {
        for (p = syhtab[h]; p; p = next) {
            s = (struct sytab *)rfetch(p);
            next = s->sy_lnk;
            if (s->sy_typ == STKEY || s->sy_typ == STSEC) continue;
            global = (s->sy_atr & SAGLO) || (uext && s->sy_typ == STUND);
            value = s->sy_val;
            if (s->sy_typ == STUND) {
                if (!global) fail("undefined local symbol");
                if (index + RBEXT >= RBMSK) fail("too many symbols");
                kind = 0;
                value = 0;
                s = (struct sytab *)wfetch(p);
                s->sy_typ = STLAB;
                s->sy_atr |= SAGLO;
                s->sy_rel = RBEXT + index;
                s->sy_val = 0;
            } else if (s->sy_rel == RBABS) {
                kind = 1;
                if (value < -32768L || value > 65535L) {
                    if (!global) continue; /* already resolved in expressions */
                    fail("absolute global symbol does not fit a.out");
                }
            } else {
                if (s->sy_rel >= SECSIZ || !stype[s->sy_rel])
                    fail("invalid symbol section");
                kind = stype[s->sy_rel];
                value += bases[kind - ATEXT];
                if (value < 0 || value > 65535L) fail("symbol value overflow");
            }
            for (i = 0; i < 8; i++) entry[i] = 0;
            for (i = 0; i < 8 && s->sy_str[i]; i++) entry[i] = s->sy_str[i];
            word(entry+8, kind | (global ? AEXT : 0));
            word(entry+10, (unsigned)value);
            putfile(symfile, entry, 12);
            index++;
            symsize += 12;
            if (symsize > 65535L) fail("symbol table too large");
        }
    }
    outpos = 0;
    for (value = 0; value < 16 + sizes[0] + sizes[1]; value++) byteout(0);
}

/* Called before emitb advances curloc.  Relocations are emitted at the first
 * byte of their field; the complete field is adjusted after pass two.
 */
aobyte(value, reloc)
unsigned value, reloc;
{
    unsigned base, action, info, symbol;
    int kind, width;
    char record[8];

    if (!cursec || cursec >= SECSIZ || !stype[cursec]) fail("absolute output is unsupported");
    kind = stype[cursec] - ATEXT;
    if (kind == 2) fail("initialized bytes in BSS");
    if (curloc < nextbyte[kind] || curloc >= slen[cursec])
        fail("overlapping output or pass size mismatch");
    base = reloc & RBMSK;
    action = reloc & RAMSK;
    if (base && action) {
        switch (action) {
        case RAA8: width = 1; info = 0; break;
        case RAA16M: width = 2; info = 0x1000; break;
        case RAA32M: width = 4; info = 0x2000; break;
        default: fail("unsupported relocation action");
        }
        if (curloc + width > slen[cursec]) fail("relocation exceeds section");
        symbol = 0;
        if (base >= RBEXT) {
            info |= REXTERNAL;
            symbol = base - RBEXT;
            if ((long)symbol * 12 >= symsize) fail("bad external symbol index");
        } else {
            if (base >= SECSIZ || !stype[base]) fail("bad relocation section");
            if (stype[base] == ADATA) info |= RDATASEG;
            if (stype[base] == ABSS) info |= RBSSSEG;
        }
        word(record, info);
        word(record+2, symbol);
        word(record+4, 0);
        word(record+6, (unsigned)curloc);
        putfile(rfile[kind], record, 8);
        rsize[kind] += 8;
        if (rsize[kind] > 65535L) fail("relocation table too large");
    }
    seekout(16 + bases[kind] + curloc);
    byteout((int)value);
    nextbyte[kind] = curloc + 1;
}

static unsigned
getword(p)
char *p;
{
    return ((p[0] & 255) << 8) | (p[1] & 255);
}

static
copyfile(fd, count)
int fd;
long count;
{
    char buf[256];
    int n;
    if (lseek(fd, 0L, 0) < 0) fail("temporary seek failed");
    while (count) {
        n = count > sizeof buf ? sizeof buf : (int)count;
        if (read(fd, buf, n) != n) fail("temporary read failed");
        if (fwrite(buf, 1, n, OBJECT) != n) fail("output write failed");
        outpos += n;
        count -= n;
    }
}

aofinish()
{
    int kind, width, i, c;
    unsigned info;
    long pos, value, add, remaining, last;
    char record[8], header[16];

    if (fflush(OBJECT)) fail("output flush failed");
    for (kind = 0; kind < 2; kind++) {
        lseek(rfile[kind], 0L, 0);
        last = 0;
        for (remaining = rsize[kind]; remaining; remaining -= 8) {
            if (read(rfile[kind], record, 8) != 8) fail("temporary read failed");
            info = getword(record);
            pos = getword(record+6);
            width = 1 << ((info >> 12) & 3);
            if (pos < last || pos + width > sizes[kind]) fail("overlapping relocation fields");
            last = pos + width;
            switch (info & 0xc000) {
            case RDATASEG: add = bases[1]; break;
            case RBSSSEG: add = bases[2]; break;
            default: add = 0;
            }
            if (!add) continue;
            pos += 16 + bases[kind];
            /* A positioning operation is required even for adjacent fields:
             * this switches the update stream from writing back to reading. */
            if (fseek(OBJECT, pos, 0)) fail("output seek failed");
            outpos = pos;
            value = 0;
            for (i = 0; i < width; i++) {
                c = getc(OBJECT);
                if (c == EOF) fail("output read failed");
                value = (value << 8) | c;
                outpos++;
            }
            value += add;
            seekout(pos);
            for (i = width-1; i >= 0; i--) byteout((int)(value >> (8*i)));
        }
    }
    seekout(16 + sizes[0] + sizes[1]);
    copyfile(rfile[0], rsize[0]);
    copyfile(rfile[1], rsize[1]);
    copyfile(symfile, symsize);
    word(header, 0407);
    word(header+2, (unsigned)sizes[0]);
    word(header+4, (unsigned)sizes[1]);
    word(header+6, (unsigned)sizes[2]);
    word(header+8, (unsigned)symsize);
    word(header+10, 0);
    word(header+12, (unsigned)rsize[0]);
    word(header+14, (unsigned)rsize[1]);
    seekout(0L);
    for (i = 0; i < 16; i++) byteout((int)header[i]);
    close(rfile[0]); close(rfile[1]); close(symfile);
}

/* Validate before section location counters narrow to 16 bits. */
aocheck()
{
    if (aflag && (curloc < 0 || curloc > 65535L)) fail("section size overflow");
}
