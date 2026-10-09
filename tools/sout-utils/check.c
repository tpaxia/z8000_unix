#include <a.out.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
struct nlist symbols[] = { { "entry", 0, 0 }, { "datum", 0, 0 },
    { "saved", 0, 0 }, { "missing", 0, 0 }, { "", 0, 0 } };
main()
{
    int i;
    struct stat st;
    static char *old[] = { "old0407", "old0410", "old0411", "old0405",
        "old0407.a", "old0410.a", "old0411.a", "old0405.a" };
    if (nlist("combined", symbols) || symbols[0].n_type != (N_TEXT|N_EXT) ||
        symbols[0].n_value || symbols[1].n_type != (N_DATA|N_EXT) ||
        symbols[1].n_value != 256 || symbols[2].n_type != (N_BSS|N_EXT) ||
        symbols[2].n_value != 512 || symbols[3].n_type) return 1;
    if (nlist("split", symbols) || symbols[1].n_value || symbols[2].n_value != 256) return 2;
    for (i=0; i<8; i++) {
        symbols[0].n_type = N_TEXT|N_EXT;
        symbols[0].n_value = 123;
        if (nlist(old[i], symbols) != -1 || symbols[0].n_type ||
            symbols[0].n_value) return 3;
        if (stat(old[i], &st) || (st.st_mode & 0777) != 0751) return 8;
    }
    if (nlist("segexec", symbols) != -1 || symbols[0].n_type || symbols[1].n_value) return 4;
    if (nlist("wideabs", symbols) != -1 || symbols[0].n_type) return 5;
    if (nlist("badseg", symbols) != -1 || symbols[0].n_type) return 6;
    if (nlist("s0", symbols) || symbols[0].n_type) return 7;
    printf("nlist: NONSEG passed; obsolete and unrepresentable formats rejected\n");
    return 0;
}
