/* Keep the existing PCC linker available during the s.out migration. */
#define main ldlegacy
#include "ldz8.c"
#undef main

extern int ldso();
int main(argc, argv)
int argc;
char **argv;
{
    int i;
    for (i = 1; i < argc; i++)
        if (!strcmp(argv[i], "-z")) return ldso(argc, argv);
    return ldlegacy(argc, argv);
}
