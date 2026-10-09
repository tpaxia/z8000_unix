/* V7 nm selection, ordering and archive presentation; machine object access
 * is explicit rather than fread of target C structures. */
#include "object.h"
#include <arport.h>
#include <ctype.h>

static int numeric, undefined, reverse = 1, global, unsorted, prefix;
static int status;

static int
compare(a, b)
struct osymbol *a, *b;
{
    int i;
    if (numeric) {
        if (a->segment != b->segment) return a->segment > b->segment ? reverse : -reverse;
        if (a->value != b->value) return a->value > b->value ? reverse : -reverse;
    }
    for (i = 0; i < 8; i++) if (a->name[i] != b->name[i])
        return (a->name[i]&255) > (b->name[i]&255) ? reverse : -reverse;
    return 0;
}

static int
names(o, path, member, many)
struct object *o;
char *path, *member;
int many;
{
    struct osymbol *symbols, s;
    unsigned i, count;
    int kind, c;
    char *select;
    select = member ? member : path;
    if (!o->symbols) {
        fprintf(stderr, "nm: %s-- no name list\n", select); return 0;
    }
    /* Bound allocation on the native 16-bit heap. -p streams the table. */
    symbols = NULL;
    if (!unsorted) {
        if (o->symbols > 60000L/sizeof(s)) goto memory;
        symbols = (struct osymbol *)malloc(o->symbols*sizeof(s));
        if (!symbols) goto memory;
    }
    count = 0;
    for (i = 0; i < o->symbols; i++) {
        if (!objsym(o, i, &s)) {
            fprintf(stderr, "nm: %s-- bad symbol table\n", select);
            status = 1; if (symbols) free((char *)symbols); return 0;
        }
        if (global && !(s.type & SO_EXTERNAL)) continue;
        kind = s.type & 31;
        switch (kind) {
        case 0: c = s.value ? 'c' : 'u'; break;
        case 2: c = 't'; break;
        case 3: c = 'd'; break;
        case 4: c = 'b'; break;
        case 024: c = 'r'; break;
        case 037: c = 'f'; break;
        default: c = 'a'; break;
        }
        if (undefined && c != 'u') continue;
        s.letter = s.type & SO_EXTERNAL ? toupper(c) : c;
        if (unsorted) {
            if (!count && (member || many) && !prefix) printf("\n%s:\n", select);
            if (prefix) { if (member) printf("%s:", path); printf("%s:", select); }
            if (!undefined) {
                if (c == 'u') printf(o->segmented ? "           " : "      ");
                else if (o->segmented && kind >= 2 && kind <= 4)
                    printf("%03o:%06lo", s.segment, s.value);
                else if (o->segmented) printf("    %06lo", s.value);
                else printf("%06lo", s.value);
                printf(" %c ", s.letter);
            }
            printf("%.8s\n", s.name);
        } else symbols[count] = s;
        count++;
    }
    if (symbols) {
        qsort(symbols, count, sizeof(s), compare);
        if ((member || many) && !prefix) printf("\n%s:\n", select);
        for (i = 0; i < count; i++) {
            s = symbols[i]; kind = s.type & 31;
            if (prefix) { if (member) printf("%s:", path); printf("%s:", select); }
            if (!undefined) {
                if (s.letter == 'u' || s.letter == 'U') printf(o->segmented ? "           " : "      ");
                else if (o->segmented && kind >= 2 && kind <= 4)
                    printf("%03o:%06lo", s.segment, s.value);
                else if (o->segmented) printf("    %06lo", s.value);
                else printf("%06lo", s.value);
                printf(" %c ", s.letter);
            }
            printf("%.8s\n", s.name);
        }
        free((char *)symbols);
    }
    return 1;
memory:
    fprintf(stderr, "nm: out of memory on %s (use -p to stream)\n", select);
    status = 1; return 0;
}

static int
file(path, many)
char *path;
int many;
{
    FILE *f;
    struct object o;
    struct ar_disk d;
    struct ar_member m;
    char magic[8];
    long length, off, next;
    int archive, r;
    f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "nm: cannot open %s\n", path); status = 1; return 0; }
    fseek(f, 0L, 2); length = ftell(f); rewind(f);
    archive = fread(magic, 1, 8, f) == 8 && !strncmp(magic, ARMAG, 8);
    off = archive ? 8 : 0;
    if (archive && many && !prefix) printf("\n%s:\n", path);
    do {
        if (archive) {
            if (off == length) break;
            if (off > length || length-off < 60 || fseek(f, off, 0) ||
                fread((char *)&d, 1, 60, f) != 60) goto bad;
            /* An optional GNU symbol index has no object contents. */
            if (d.name[0] == '/' && d.name[1] == ' ') {
                m.ar_size = arnum(d.size, 10, 10);
                if (d.end[0] != '`' || d.end[1] != '\n' || m.ar_size < 0) goto bad;
                m.ar_name[0] = 0;
            } else if (!ardecode(&d, &m)) goto bad;
            off += 60;
            if (m.ar_size > length-off) goto bad;
            next = off+m.ar_size+(m.ar_size&1);
            if (next > length) goto bad;
            if (!m.ar_name[0] || !strcmp(m.ar_name, "__.SYMDEF")) { off = next; continue; }
            r = objread(&o, f, off, m.ar_size);
        } else { next = length; r = objread(&o, f, 0L, length); }
        if (r < 0 || (!archive && !r)) goto bad;
        if (r == 1) names(&o, path, archive ? m.ar_name : NULL, many);
        off = next;
    } while (archive);
    fclose(f); return 1;
bad:
    fprintf(stderr, "nm: %s-- bad format\n", path); status = 1;
    fclose(f); return 0;
}

int
main(argc, argv)
int argc;
char **argv;
{
    char *p;
    int first, i;
    first = 1;
    if (argc > 1 && argv[1][0] == '-' && argv[1][1]) {
        for (p = argv[1]+1; *p; p++) switch (*p) {
        case 'n': numeric = 1; break;
        case 'g': global = 1; break;
        case 'u': undefined = 1; break;
        case 'r': reverse = -1; break;
        case 'p': unsorted = 1; break;
        case 'o': prefix = 1; break;
        default: fprintf(stderr, "nm: invalid argument -%c\n", *p); return 1;
        }
        first++;
    }
    if (first == argc) file("a.out", 0);
    for (i = first; i < argc; i++) file(argv[i], argc-first > 1);
    return status;
}
