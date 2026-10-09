/* V7 size presentation, using the machine-dependent object reader. */
#include "object.h"
int
main(argc, argv)
int argc;
char **argv;
{
    struct object o;
    FILE *f;
    long length, sum;
    int i, status;
    char *name;
    status = 0;
    for (i = 1; i < argc || (i == 1 && argc == 1); i++) {
        name = argc == 1 ? "a.out" : argv[i];
        f = fopen(name, "rb");
        if (!f) { fprintf(stderr, "size: cannot open %s\n", name); status = 1; continue; }
        fseek(f, 0L, 2); length = ftell(f);
        if (objread(&o, f, 0L, length) != 1) {
            fprintf(stderr, "size: %s-- bad format\n", name); status = 1;
        } else {
            if (argc > 2) printf("%s: ", name);
            sum = o.text+o.data+o.bss;
            printf("%ld+%ld+%ld = %ldb = 0%lob\n", o.text, o.data, o.bss, sum, sum);
        }
        fclose(f);
    }
    return status;
}
