/* ZEUS s.out linker, shared by host and V7 builds. Images and relocation
 * words are streamed: no 64K image allocation in the native data segment.
 * Initial layout: coalesce code, data and BSS; unbound input sections only.
 */
#include <stdio.h>
#include "soutfmt.h"
#ifndef z8000
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#else
extern char *calloc(), *malloc(), *strcpy();
extern long atol();
#endif

struct lsym {
    struct lsym *next;
    char name[9];
    long value;
    int kind, index;
    struct lobj *owner;
    int section;
};
struct lseg {
    long size[3], base[3], dest[3], file[2];
};
struct lobj {
    struct lobj *next;
    char *path;
    long origin, length, image, start;
    int segmented, split, nseg, nsym;
    struct lseg *seg;
    struct lsym **sym;
};
static struct lobj *objs, *tail;
static struct lsym *syms, *stail;
static long total[3], address[3];
static int partial, split, strip, mode = -1, count, created;
static int raw;
static long textbase;
static long limit = 65536L;
static unsigned cseg = 0, dseg = 1;
static char *output = "a.out", *entryname, *libpath = "/lib/lib";

static int die(s)
char *s;
{
    fprintf(stderr, "ldz8 s.out: %s\n", s);
    if (created) unlink(output);
    exit(1);
    return 0;
}
static char *alloc(n)
unsigned n;
{
    char *p;
    p = calloc(n, 1);
    if (!p) die("out of memory");
    return p;
}
static int get(f, pos, p, n)
FILE *f;
long pos;
char *p;
int n;
{
    if (fseek(f, pos, 0) || fread(p, 1, n, f) != n)
        die("truncated input or read error");
    return 0;
}
static struct lsym *find(name)
char *name;
{
    struct lsym *s;
    for (s = syms; s; s = s->next)
        if (!strcmp(name, s->name)) return s;
    return NULL;
}
static struct lsym *enter(name)
char *name;
{
    struct lsym *s;
    if (s = find(name)) return s;
    if (count == 4096) die("symbol table exceeds relocation index range");
    s = (struct lsym *)alloc(sizeof(*s));
    strcpy(s->name, name);
    s->index = count++;
    if (stail) stail->next = s; else syms = s;
    stail = s;
    return s;
}

/* Parse before selecting an archive member: rejected members cannot change
 * the global symbol table or consume output section space. */
static struct lobj *readobj(f, path, off, len)
FILE *f;
char *path;
long off, len;
{
    char h[24], b[16], name[9];
    struct lobj *o;
    struct lsym *s;
    struct lseg *g;
    unsigned magic, flags, sb, nb, type, attrs;
    long sum, bs, cp, dp, sp, value;
    int i, k, segment;
    get(f, off, h, 24);
    magic = so_get16(h); flags = so_get16(h+18);
    if (magic != SO_SMAG && magic != SO_NMAG && magic != SO_SID && magic != SO_NID)
        die("input is not a Z8000 s.out object");
    sb = so_get16(h+10); nb = so_get16(h+12);
    if (flags || so_get16(h+20) || so_get16(h+22) || !sb || sb%16 || nb%14)
        die("unsupported object header, stripped relocation or line records");
    o = (struct lobj *)alloc(sizeof(*o));
    o->path = path; o->origin = off; o->length = len;
    o->segmented = magic == SO_SMAG || magic == SO_SID;
    o->split = magic == SO_SID || magic == SO_NID;
    o->nseg = sb/16; o->nsym = nb/14;
    if (o->nseg > (o->split ? 256 : 128) || (!o->segmented && o->nseg != 1))
        die("invalid segment table length");
    o->image = so_get32(h+2); o->start = off+24+sb;
    if (o->image < 0 || o->image > 16777216L ||
        24L+sb+2*o->image+nb > len) die("invalid image size");
    o->seg = (struct lseg *)alloc(o->nseg*sizeof(*g));
    o->sym = (struct lsym **)alloc((o->nsym ? o->nsym : 1)*sizeof(s));
    sum = bs = 0;
    for (i = 0; i < o->nseg; i++) {
        get(f, off+24+i*16L, b, 16);
        attrs = so_get16(b+10);
        if ((b[0]&255) != i || b[1] || b[2] || b[3] || (attrs&~7) || so_get32(b+12))
            die("input requires unsupported bound or offset segment placement");
        g = &o->seg[i];
        for (k = 0; k < 3; k++) {
            g->size[k] = so_get16(b+4+2*k);
            if ((g->size[k]&1) || (g->size[k] && !(attrs&(1<<k))))
                die("odd section size or inconsistent segment attributes");
        }
        g->base[1] = o->split ? 0 : g->size[0];
        g->base[2] = g->base[1]+g->size[1];
        if (o->segmented) {
            /* The assembler emits one section per logical segment. */
            if ((!!g->size[0]+!!g->size[1]+!!g->size[2]) > 1)
                die("segmented input must have one section per segment");
            g->base[0] = g->base[1] = g->base[2] = 0;
        }
        sum += g->size[0]+g->size[1]; bs += g->size[2];
    }
    if (sum != o->image || bs != so_get32(h+6)) die("inconsistent image totals");
    cp = o->start; dp = cp;
    for (i = 0; i < o->nseg; i++) dp += o->seg[i].size[0];
    for (i = 0; i < o->nseg; i++) {
        o->seg[i].file[0] = cp; o->seg[i].file[1] = dp;
        cp += o->seg[i].size[0]; dp += o->seg[i].size[1];
    }
    sp = o->start+2*o->image;
    for (i = 0; i < o->nsym; i++) {
        get(f, sp+i*14L, b, 14);
        type = b[4]&255; segment = b[5]&255; value = so_get32(b);
        if ((b[6]&128) || (type&128) || (type&31)>4 || !b[6])
            die("unsupported long/debug symbol or empty name");
        if (!!(type&SO_SEGMENTED) != o->segmented && (type&31) != SO_ABS)
            die("inconsistent symbol address mode");
        for (k = 0; k < 8; k++) name[k] = b[6+k]; name[8] = 0;
        if (!(type&31) && (!(type&SO_EXTERNAL) || value < 0 || value > 65535L))
            die("invalid common or local undefined symbol");
        if ((type&31) >= SO_TEXTSYM) {
            if (segment >= o->nseg) die("symbol segment index outside table");
            k = (type&31)-SO_TEXTSYM; g = &o->seg[segment];
            if (value < g->base[k] || value > g->base[k]+g->size[k])
                die("symbol outside section");
        }
        /* Keep externals only. Local relocation targets can be read from the
         * validated on-disk table without retaining every compiler label. */
        if (type&SO_EXTERNAL) {
            s = (struct lsym *)alloc(sizeof(*s)); strcpy(s->name, name);
            s->kind = type&31; s->value = value; s->owner = o; s->section = segment;
            s->index = SO_EXTERNAL; o->sym[i] = s;
        }
    }
    return o;
}
static int wanted(o)
struct lobj *o;
{
    int i;
    struct lsym *s, *g;
    for (i = 0; i < o->nsym; i++) {
        s = o->sym[i]; if (!s) continue;
        g = find(s->name);
        /* V7 does not pull a library function to satisfy a common array. */
        if (s->index && g && !g->kind &&
            ((s->kind && !(g->value && s->kind == SO_TEXTSYM)) ||
             (s->value && !g->value))) return 1;
    }
    return 0;
}
static int discard(o)
struct lobj *o;
{
    int i;
    for (i = 0; i < o->nsym; i++) if (o->sym[i]) free(o->sym[i]);
    free(o->sym); free(o->seg); free(o);
    return 0;
}
static int add(o)
struct lobj *o;
{
    int i, k;
    struct lsym *s, *g;
    if (mode == -1) mode = o->segmented;
    if (mode != o->segmented) die("cannot mix segmented and nonsegmented objects");
    for (i = 0; i < o->nseg; i++) for (k = 0; k < 3; k++) {
        o->seg[i].dest[k] = total[k]; total[k] += o->seg[i].size[k];
        if (total[k] > 65535L) die("coalesced section exceeds 16-bit size");
    }
    for (i = 0; i < o->nsym; i++) {
        s = o->sym[i];
        if (!s) continue;
        g = enter(s->name);
        if (s->kind) {
            /* V7 keeps a common variable from being replaced by text. */
            if (g->kind || (g->value && s->kind == SO_TEXTSYM)) {
                fprintf(stderr, "ldz8 s.out: symbol %s in %s\n", s->name, o->path);
                die("multiply defined external symbol");
            }
            g->kind = s->kind; g->value = s->value;
            g->owner = o; g->section = s->section;
        } else if (!g->kind && s->value > g->value) g->value = s->value;
        free(s); o->sym[i] = g;
    }
    if (tail) tail->next = o; else objs = o; tail = o;
    return 0;
}
static int load(path)
char *path;
{
    FILE *f;
    char b[60], num[11];
    long len, off, size;
    int changed, used, i;
    struct lobj *o, *p;
    f = fopen(path, "rb");
    if (!f) die(path);
    fseek(f, 0L, 2); len = ftell(f);
    if (len < 24) die("input too short");
    get(f, 0L, b, 8);
    if (strncmp(b, "!<arch>\n", 8)) add(readobj(f, path, 0L, len));
    else do {
        changed = 0; off = 8;
        while (off < len) {
            get(f, off, b, 60); off += 60;
            if (b[58] != '`' || b[59] != '\n') die("bad portable archive header");
            for (i = 0; i < 10; i++) { num[i] = b[48+i];
                if (num[i] != ' ' && (num[i] < '0' || num[i] > '9')) die("bad archive member size"); }
            num[10] = 0; size = atol(num);
            if (size < 0 || size > len-off) die("truncated archive member");
            used = 0;
            for (p = objs; p; p = p->next)
                if (!strcmp(p->path, path) && p->origin == off) used = 1;
            /* Skip portable archive indexes; long member names are not needed
             * for V7's fourteen-character filesystem names. */
            if (!used && b[0] != '/') {
                o = readobj(f, path, off, size);
                if (wanted(o)) { add(o); changed = 1; } else discard(o);
            }
            off += size+(size&1);
            if (off > len) die("missing archive alignment byte");
        }
    } while (changed);
    fclose(f);
    return 0;
}
static long symval(s)
struct lsym *s;
{
    int k;
    if (s->kind < SO_TEXTSYM) return s->value;
    k = s->kind-SO_TEXTSYM;
    if (!s->owner) return s->value+address[k];
    return s->value-s->owner->seg[s->section].base[k]+
        s->owner->seg[s->section].dest[k]+address[k];
}
static unsigned segnum(k)
int k;
{
    if (partial) return k;
    return k == 0 ? cseg : dseg;
}
static int put(f, pos, p, n)
FILE *f;
long pos;
char *p;
int n;
{
    if (fseek(f, pos, 0) || fwrite(p, 1, n, f) != n) die("output write failed");
    return 0;
}

/* Relocate one word. Native unsigned arithmetic deliberately wraps the
 * offset addend at 16 bits, including assembler-emitted negative addends. */
static unsigned relocate(in, o, tag, word, newtag)
FILE *in;
struct lobj *o;
unsigned tag, word, *newtag;
{
    struct lsym *s, local;
    char b[14];
    struct lseg *g;
    long delta, value;
    unsigned action, idx;
    int kind, k;
    *newtag = 0;
    if (!tag) return word;
    if (tag&SO_REXT) {
        action = tag&7; idx = tag>>4;
        if (action > SO_RSHORT) die("relative relocation not emitted by this assembler yet");
        if (!mode && action) die("segmented relocation in nonsegmented object");
        if (idx >= o->nsym) die("relocation symbol index outside table");
        s = o->sym[idx];
        if (!s) {
            get(in, o->start+2*o->image+idx*14L, b, 14);
            local.kind = b[4]&31; local.value = so_get32(b);
            local.section = b[5]&255; local.owner = o; s = &local;
        }
        kind = s->kind; value = symval(s);
        if (!kind) {
            if (!partial) die("undefined external symbol");
            *newtag = so_reloc(1, mode, s->index, 0, action);
            return word;
        }
        delta = value;
    } else {
        if (mode) {
            if (!(tag&1) || (tag&128)) die("bad segmented relocation tag");
            idx = tag>>8; action = (tag>>4)&7;
        } else {
            if (tag != 2 && tag != 4 && tag != 6) die("bad nonsegmented relocation tag");
            idx = 0; action = 0;
        }
        kind = ((tag>>1)&3)+1;
        if (kind < 2 || idx >= o->nseg) die("relocation section outside table");
        k = kind-2; g = &o->seg[idx];
        delta = g->dest[k]+address[k]-g->base[k];
    }
    if (action > SO_RSHORT) die("relative relocation not emitted by this assembler yet");
    if (!mode && action) die("segmented relocation in nonsegmented object");
    if (kind == SO_ABS && action) die("segmented address of absolute symbol");
    k = kind-SO_TEXTSYM;
    if (action == SO_RSEG) word = (word&0x80ff) | (segnum(k)<<8);
    else if (action == SO_RSHORT) {
        value = (word&255)+delta;
        if (value < 0 || value > 255) die("short segmented offset exceeds 255");
        word = (segnum(k)<<8) | (unsigned)value;
    } else word = (unsigned)(word+delta)&65535;
    if (partial && kind != SO_ABS)
        *newtag = so_reloc(0, mode, mode ? k : 0, kind, action);
    return word;
}
static int writeout()
{
    FILE *f, *in;
    struct lobj *o;
    struct lsym *s;
    char b[24], r[2];
    long start, image, pos, dest, ep, n, rp;
    unsigned word, tag, nt, magic, attrs;
    int i, k, ns, sk;
    if (!objs) die("no objects selected");
    if (!partial) for (s = syms; s; s = s->next) if (!s->kind && s->value) {
        n = s->value;
        s->kind = SO_BSSSYM; s->value = total[2]; s->owner = NULL;
        total[2] += (n+1)&~1L;
    }
    for (k = 0; k < 3; k++) {
        if (!partial && !raw) total[k] = (total[k]+255)&~255L;
        if (total[k] > 65535L) die("padded section exceeds 16-bit size");
    }
    address[0] = textbase;
    address[1] = raw ? textbase+total[0] : (mode || split) ? 0 : total[0];
    address[2] = mode && partial ? 0 : address[1]+total[1];
    if (raw && address[2]+total[2] > limit) die("raw image exceeds its memory limit");
    if ((!mode && !split && total[0]+total[1]+total[2] > 65536L) ||
        ((mode || split) && total[1]+total[2] > 65536L)) die("data address space overflow");
    if (!partial) for (s = syms; s; s = s->next) if (!s->kind) {
        if (!strcmp(s->name,"_etext")) k = 0;
        else if (!strcmp(s->name,"_edata")) k = 1;
        else if (!strcmp(s->name,"_end")) k = 2;
        else continue;
        s->kind = SO_TEXTSYM+k; s->value = total[k]; s->owner = NULL;
    }
    for (s = syms; s; s = s->next) if (!s->kind && !partial) {
        fprintf(stderr, "undefined: %s\n", s->name); die("unresolved symbols");
    }
    ep = address[0];
    if (entryname) {
        s = find(entryname);
        if (!s || s->kind != SO_TEXTSYM) die("entry point is not a defined code symbol");
        ep = symval(s);
    }
    if (ep&1 || (!partial && (ep < address[0] || ep >= address[0]+total[0]))) die("invalid entry point");
    if (mode && !partial) ep += (0x8000L+cseg*256L)*65536L;
    ns = mode ? (partial ? 3 : 2) : 1;
    start = raw ? 0 : 24+ns*16; image = total[0]+total[1];
    magic = mode ? (split ? SO_SID : SO_SMAG) : (split ? SO_NID : SO_NMAG);
    /* Never truncate an input that is also named as the output. */
    for (o = objs; o; o = o->next) if (!strcmp(output, o->path)) die("output is an input file");
    f = fopen(output, "wb"); if (!f) die("cannot create output"); created = 1;
    so_header(b, magic, image, total[2], ns*16, strip ? 0 : count*14,
        ep, partial ? 0 : SO_STRIP);
    if (!raw) put(f, 0L, b, 24);
    for (i = 0; !raw && i < ns; i++) {
        if (mode) {
            sk = partial || cseg < dseg ? i : 1-i;
            attrs = (total[sk] ? 1<<sk : 0) | (partial ? 0 : SO_BOUND);
            if (!partial && sk==1 && total[2]) attrs |= SO_BSS;
            so_segment(b, partial ? sk : segnum(sk), sk==0 ? (unsigned)total[0] : 0,
                sk==1 ? (unsigned)total[1] : 0,
                (sk==2 || (!partial && sk==1)) ? (unsigned)total[2] : 0, attrs);
            /* BSS is after initialized data in the same physical segment. */
            if (sk == 1 && !partial && total[1] && total[2]) { b[3] = total[1]>>8; so_put16(b+10, attrs|64); }
        } else so_segment(b, 0, (unsigned)total[0], (unsigned)total[1], (unsigned)total[2],
            (total[0]?1:0)|(total[1]?2:0)|(total[2]?4:0));
        put(f, 24+i*16L, b, 16);
    }
    /* Initialize padding and relocation once, using bounded blocks. */
    for (i = 0; i < sizeof(b); i++) b[i] = 0;
    for (pos = 0; pos < image*(partial ? 2 : 1); pos += n) {
        n = image*(partial ? 2 : 1)-pos; if (n > sizeof(b)) n = sizeof(b);
        put(f, start+pos, b, (int)n);
    }
    for (o = objs; o; o = o->next) {
        in = fopen(o->path, "rb"); if (!in) die("cannot reopen input");
        for (i = 0; i < o->nseg; i++) for (k = 0; k < 2; k++) {
            for (pos = 0; pos < o->seg[i].size[k]; pos += 2) {
                rp = o->seg[i].file[k]+pos;
                get(in, rp, b, 2); word = so_get16(b);
                get(in, o->start+o->image+rp-o->start, r, 2); tag = so_get16(r);
                word = relocate(in, o, tag, word, &nt); so_put16(b, word);
                dest = (k ? total[0] : 0)+o->seg[i].dest[k]+pos;
                put(f, start+dest, b, 2);
                if (partial) { so_put16(r, nt); put(f, start+image+dest, r, 2); }
            }
        }
        fclose(in);
    }
    pos = start+image*(partial ? 2 : 1);
    if (!raw && !strip) for (s = syms; s; s = s->next) {
        k = s->kind >= 2 ? s->kind-2 : 0;
        sk = k ? (dseg < cseg ? 0 : 1) : (cseg < dseg ? 0 : 1);
        so_symbol(b, symval(s), s->kind|SO_EXTERNAL|
            (mode && s->kind != SO_ABS ? SO_SEGMENTED : 0),
            mode && s->kind >= 2 ? (partial ? k : sk) : 0, s->name);
        put(f, pos, b, 14); pos += 14;
    }
    if (fclose(f)) die("output close failed");
    chmod(output, partial ? 0644 : 0755); created = 0;
    return 0;
}

int ldso(argc, argv)
int argc;
char **argv;
{
    int i;
    char *a, *name;
    unsigned v;
    for (i = 1; i < argc; i++) {
        a = argv[i];
        if (!strcmp(a,"-z") || !strcmp(a,"-x")) continue;
        if (!strcmp(a,"-i")) { split = 1; continue; }
        if (!strcmp(a,"-r")) { partial = 1; continue; }
        if (!strcmp(a,"-s")) { strip = 1; continue; }
        if (!strcmp(a,"-b")) { raw = 1; continue; }
        if (!strcmp(a,"-o") || !strcmp(a,"-e") || !strcmp(a,"-L") ||
            !strcmp(a,"-u") || !strcmp(a,"-C") || !strcmp(a,"-D") || !strcmp(a,"-T") || !strcmp(a,"-M")) {
            if (++i == argc) die("missing option argument");
            if (a[1]=='o') output = argv[i];
            if (a[1]=='e') entryname = argv[i];
            if (a[1]=='L') libpath = argv[i];
            if (a[1]=='T') {
                name = argv[i]; textbase = 0;
                if (!*name) die("empty text offset");
                while (*name) { if (*name<'0' || *name>'9' || textbase>65535L) die("invalid decimal text offset"); textbase=textbase*10+*name++-'0'; }
                if (textbase>65535L || (textbase&1)) die("text offset must be even and below 65536");
            }
            if (a[1]=='M') {
                name = argv[i]; limit = 0;
                if (!*name) die("empty memory limit");
                while (*name) { if (*name<'0' || *name>'9' || limit>65536L) die("invalid decimal memory limit"); limit=limit*10+*name++-'0'; }
                if (!limit || limit>65536L) die("memory limit must be 1..65536");
            }
            if (a[1]=='u') { if (strlen(argv[i])>8) die("symbol name exceeds eight characters"); enter(argv[i]); }
            if (a[1]=='C' || a[1]=='D') {
                name = argv[i]; v = 0;
                if (!*name) die("empty segment number");
                while (*name) { if (*name<'0' || *name>'9' || v>127) die("invalid decimal segment number"); v=v*10+*name++-'0'; }
                if (v>127) die("segment number exceeds 127");
                if (a[1]=='C') cseg = v; else dseg = v;
            }
            continue;
        }
        if (a[0]=='-' && a[1]=='l' && a[2]) {
            name = alloc(strlen(libpath)+strlen(a+2)+3);
            strcpy(name, libpath); strcat(name, a+2); strcat(name,".a"); load(name);
        } else if (a[0]=='-') die("unsupported s.out linker option");
        else load(a);
    }
    if (partial && (strip || split)) die("-r cannot be combined with -s or -i yet");
    if (raw && (partial || split)) die("raw output cannot be partial or split I/D");
    if (!raw && (textbase || limit!=65536L)) die("text offset and memory limit require raw output");
    if (raw) dseg = cseg;
    if (mode && cseg == dseg && !raw) die("code and data segments must differ");
    return writeout();
}
