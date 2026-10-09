#include <a.out.h>
/* No direct stdio calls: nlist's reader must bring in stdio before fakcu. */
struct nlist symbols[] = { { "_main", 0, 0 }, { "", 0, 0 } };
main(argc, argv)
int argc;
char **argv;
{
    return nlist(argv[0], symbols) ||
        symbols[0].n_type != (N_TEXT|N_EXT);
}
