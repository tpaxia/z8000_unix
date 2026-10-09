/* V7 nlist ABI adapter. SEG addresses require a different public interface. */
#include <a.out.h>
#include "object.h"

int
nlist(name, list)
char *name;
struct nlist *list;
{
    FILE *f;
    struct object o;
    struct osymbol s;
    struct nlist *p;
    long length;
    unsigned i;
    int k;
    for (p = list; p->n_name[0]; p++) p->n_type = p->n_value = 0;
    f = fopen(name, "rb");
    if (!f) return -1;
    fseek(f, 0L, 2); length = ftell(f);
    if (objread(&o, f, 0L, length) != 1 || o.segmented) goto bad;
    /* Validate the complete table before changing the caller's results. */
    for (i = 0; i < o.symbols; i++)
        if (!objsym(&o, i, &s) || s.value > 65535L) goto bad;
    for (i = 0; i < o.symbols; i++) {
        if (!objsym(&o, i, &s)) goto bad;
        for (p = list; p->n_name[0]; p++) {
            for (k = 0; k < 8; k++) if (p->n_name[k] != s.name[k]) break;
            if (k == 8) { p->n_type = s.type; p->n_value = s.value; break; }
        }
    }
    fclose(f); return 0;
bad:
    for (p = list; p->n_name[0]; p++) p->n_type = p->n_value = 0;
    fclose(f); return -1;
}
