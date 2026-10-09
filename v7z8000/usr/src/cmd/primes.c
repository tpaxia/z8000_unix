/* Z8000 implementation of V7 primes.s: small primes, then an 8000-number
 * segmented bit sieve crossed out with the original 210-wheel candidates.
 */
#include <stdio.h>
#include "num56.h"

static int small[] = {2,3,5,7,11,13,17,19,23,29,31,37,41,43,
	47,53,59,61,67,71,73,79,83,89,97};
static char table[1000];
static unsigned char bits[] = {1,2,4,8,16,32,64,128};

main(argc, argv)
int argc;
char **argv;
{
	struct number n, high, q, out;
	long d, limit, offset, rem;
	int input, i, count, left, w, last;
	for(;;) {
		input = nread(&n, argc>1 ? argv[1] : (char *)0);
		if(!input) return(0);
		if(input>0) break;
		fputs("Ouch.\n", stderr);
		if(argc>1) return(0);
	}
	for(i=0; i<25; i++)
		if(nsmall(&n, (long)small[i])<=0) {
			nset(&out, (long)small[i]);
			nprint(&out);
		}
	if(nsmall(&n, 100L)<0) nset(&n, 100L);
	if(!(n.word[3]&1) && nadd(&n, 1L)) return(0);
	for(;;) {
		count = 4000;
		high = n;
		last = nadd(&high, 7998L);
		if(last) {
			count = (65535L-n.word[3])/2+1;
			high.word[0] = 255;
			high.word[1] = high.word[2] = high.word[3] = 65535;
		}
		for(i=0; i<1000; i++) table[i]=0;
		left = count;
		limit = nroot(&high);
		d = 3;
		w = 1;
		while(d<=limit && left) {
			rem = ndiv(&n, d, &q);
			offset = rem ? d-rem : 0;
			if(offset&1) offset += d;
			/* Keep the divisor itself; only its multiples are composite. */
			if(nsmall(&n, d)<=0) offset += 2*d;
			while(offset<2*count) {
				i = offset >> 3;
				if(!(table[i]&bits[offset&7])) {
					table[i] |= bits[offset&7];
					left--;
				}
				offset += 2*d;
			}
			if(d==3) d=5;
			else if(d==5) d=7;
			else if(d==7) d=11;
			else {
				d += wheel[w++];
				if(w==48) w=0;
			}
		}
		out = n;
		for(i=0; i<count; i++) {
			if(!(table[(2*i)>>3]&bits[(2*i)&7])) nprint(&out);
			if(i+1<count) nadd(&out, 2L);
		}
		fflush(stdout);
		if(last || nadd(&n, 8000L)) return(0);
	}
}
