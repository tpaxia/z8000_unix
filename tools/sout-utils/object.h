/* Machine-dependent object access; V7 int/pointer ABI remains unchanged. */
#include <stdio.h>
#include "soutfmt.h"
#ifndef z8000
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#else
extern char *malloc(), *realloc(), *mktemp();
#endif

struct object {
    FILE *file;
    long origin, length, text, data, bss, imageoff, symoff, keep;
    unsigned magic, flags, segments, symbols, symsize;
    int sout, segmented;
};
struct osymbol {
    char name[8];
    unsigned type, segment;
    unsigned long value;
    int letter;
};
extern int objread(), objsym(), objbytes(), objstrip();
