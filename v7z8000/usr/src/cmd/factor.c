/* Z8000 implementation of V7 factor.s: repeated division and 210-wheel. */
#include <stdio.h>
#include "num56.h"

main(argc, argv)
int argc;
char **argv;
{
	struct number n, q, f;
	long d, limit;
	int input, i;
	for(;;) {
		input = nread(&n, argc>1 ? argv[1] : (char *)0);
		if(!input) return(0);
		if(input<0) {
			fputs("Ouch.\n", stderr);
			if(argc>1) return(0);
			continue;
		}
		putchar('\n');
		limit = nroot(&n);
		d = 2;
		i = 1;
		while(d<=limit) {
			if(ndiv(&n, d, &q)==0) {
				n = q;
				nset(&f, d);
				fputs("     ", stdout);
				nprint(&f);
				limit = nroot(&n);
			} else if(d==2) d=3;
			else if(d==3) d=5;
			else if(d==5) d=7;
			else if(d==7) d=11;
			else {
				d += wheel[i++];
				if(i==48) i=0;
			}
		}
		if(nsmall(&n, 1L)>0) {
			fputs("     ", stdout);
			nprint(&n);
		}
		fflush(stdout);
		if(argc>1) return(0);
	}
}
