#include <stdio.h>

#define BUMP(x) ((x)+1)
#define TWICE(x) ((x)+(x))
#define CPPVALUE 6

#if CPPVALUE != 6
invalid_macro_expansion
#endif

main()
{
	FILE *p;
	p = stdout;
	if (fileno(p) != 1 || TWICE(BUMP(2)) != 6)
		return(1);
	if ((char)254 != -2)
		return(2);
	putc('X', p);
	putc('\n', p);
	puts("NATIVE CPP OK");
	return(0);
}
