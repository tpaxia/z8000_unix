/* V7 strip's temporary-copy policy, with explicit target-format fields. */
#include "object.h"
#include <signal.h>

static int
copy(in, out, size)
FILE *in, *out;
long size;
{
    char b[512];
    int n;
    while (size) {
        n = size > 512 ? 512 : (int)size;
        if (fread(b, 1, n, in) != n || fwrite(b, 1, n, out) != n) return 0;
        size -= n;
    }
    return 1;
}

static int
strip(name, temp)
char *name, *temp;
{
    FILE *in, *out;
    struct object o;
    struct osymbol s;
    char h[24];
    long length;
    int n, i, ok;
    in = fopen(name, "rb");
    if (!in) { fprintf(stderr, "strip: cannot open %s\n", name); return 1; }
    fseek(in, 0L, 2); length = ftell(in);
    if (objread(&o, in, 0L, length) != 1) goto bad;
    /* Do not destroy a recognized file with unsupported symbol records. */
    for (i = 0; i < o.symbols; i++) if (!objsym(&o, i, &s)) goto bad;
    if (length == o.keep && !o.symbols) {
        fclose(in); return 0;
    }
    n = objstrip(&o, h);
    out = fopen(temp, "wb");
    if (!out) { fclose(in); fprintf(stderr, "strip: cannot create temporary file\n"); return 1; }
    ok = n && fwrite(h, 1, n, out) == n && !fseek(in, (long)n, 0) &&
        copy(in, out, o.keep-n);
    if (fclose(out)) ok = 0;
    fclose(in);
    if (!ok) { fprintf(stderr, "strip: %s-- copy failed\n", name); return 1; }
    in = fopen(temp, "rb");
    if (!in) return 1;
    /* Opening an existing file preserves its permissions, as in V7 strip. */
    out = fopen(name, "wb");
    if (!out) { fclose(in); fprintf(stderr, "strip: cannot rewrite %s\n", name); return 1; }
    ok = copy(in, out, o.keep);
    if (fclose(out)) ok = 0;
    fclose(in);
    if (!ok) fprintf(stderr, "strip: %s-- write failed\n", name);
    return !ok;
bad:
    fprintf(stderr, "strip: %s-- bad format\n", name); fclose(in); return 1;
}

int
main(argc, argv)
int argc;
char **argv;
{
    char temp[32];
    int i, fd, status;
    signal(SIGHUP, SIG_IGN); signal(SIGINT, SIG_IGN); signal(SIGQUIT, SIG_IGN);
    strcpy(temp, "/tmp/suXXXXXX");
#ifdef z8000
    if (mktemp(temp) != temp) {
        fprintf(stderr, "strip: cannot create temporary file\n"); return 2;
    }
    fd = creat(temp, 0600);
#else
    fd = mkstemp(temp);
#endif
    if (fd < 0) { fprintf(stderr, "strip: cannot create temporary file\n"); return 2; }
    close(fd); status = 0;
    for (i = 1; i < argc; i++) if (strip(argv[i], temp)) status = 1;
    unlink(temp); return status;
}
