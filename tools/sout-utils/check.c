#include <a.out.h>
#include <stdio.h>
struct nlist symbols[] = { { "entry", 0, 0 }, { "datum", 0, 0 },
    { "saved", 0, 0 }, { "missing", 0, 0 }, { "", 0, 0 } };
main()
{
    if (nlist("combined", symbols) || symbols[0].n_type != (N_TEXT|N_EXT) ||
        symbols[0].n_value || symbols[1].n_type != (N_DATA|N_EXT) ||
        symbols[1].n_value != 256 || symbols[2].n_type != (N_BSS|N_EXT) ||
        symbols[2].n_value != 512 || symbols[3].n_type) return 1;
    if (nlist("split", symbols) || symbols[1].n_value || symbols[2].n_value != 256) return 2;
    if (nlist("legacy.b", symbols) || symbols[3].n_type) return 3;
    if (nlist("segexec", symbols) != -1 || symbols[0].n_type || symbols[1].n_value) return 4;
    if (nlist("wideabs", symbols) != -1 || symbols[0].n_type) return 5;
    if (nlist("badseg", symbols) != -1 || symbols[0].n_type) return 6;
    if (nlist("s0", symbols) || symbols[0].n_type) return 7;
    printf("nlist: legacy and NONSEG passed; unrepresentable addresses rejected\n");
    return 0;
}
