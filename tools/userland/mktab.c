/* Preserve V7 nroff's terminal resource layout. This is a data resource,
 * not an object-format bridge or an executable loader. */
#include "object.h"

main(argc, argv)
int argc;
char **argv;
{
    struct object o;
    struct osymbol s;
    FILE *in, *out;
    char h[16], b[512];
    long length, left;
    int i, n, ok;
    if (argc != 3 || !strcmp(argv[1],argv[2])) return 1;
    in = fopen(argv[1], "rb");
    if (!in) return 1;
    fseek(in, 0L, 2); length = ftell(in);
    if (objread(&o, in, 0L, length) != 1 || !o.sout || o.segmented ||
        o.text || o.bss || !o.data || o.data > 65535L) goto bad;
    for (i = 0; i < o.symbols; i++)
        if (!objsym(&o, i, &s) || !(s.type & 31)) goto bad;
    /* A partial NONSEG data-only link has zero data base. Only resolved
     * data-address words and absolute words may remain in its relocation. */
    if (!(o.flags & SO_STRIP)) for (left = 0; left < o.data; left += 2) {
        if (!objbytes(&o, o.keep+left, b, 2) ||
            (so_get16(b) != 0 && so_get16(b) != 4)) goto bad;
    }
    for (i = 0; i < 16; i++) h[i] = 0;
    so_put16(h, 0411); so_put16(h+4, (unsigned)o.data);
    out = fopen(argv[2], "wb");
    if (!out) goto bad;
    ok = fwrite(h, 1, 16, out) == 16;
    for (left = 0; ok && left < o.data; left += n) {
        n = o.data-left > 512 ? 512 : (int)(o.data-left);
        ok = objbytes(&o, o.imageoff+left, b, n) && fwrite(b, 1, n, out) == n;
    }
    if (fclose(out)) ok = 0;
    fclose(in);
    if (!ok) unlink(argv[2]);
    return !ok;
bad:
    fprintf(stderr, "mktab: invalid data-only terminal resource\n");
    fclose(in); return 1;
}
