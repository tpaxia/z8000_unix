/* Exact integers for the V7 factor/primes range, 1 through 2^56-1.
 * The PDP-11 programs used 56-bit floating significands. IEEE double has
 * only 53 bits, so keep four most-significant-first 16-bit limbs instead.
 */
struct number { unsigned short word[4]; };
long ndiv();

static
nzero(a)
struct number *a;
{
	return(!(a->word[0]|a->word[1]|a->word[2]|a->word[3]));
}

static
nset(a, n)
struct number *a;
long n;
{
	a->word[0] = a->word[1] = 0;
	a->word[2] = n >> 16;
	a->word[3] = n;
}

static
nsmall(a, n)
struct number *a;
long n;
{
	if(a->word[0] || a->word[1]) return(1);
	if(a->word[2] != (unsigned short)(n>>16))
		return(a->word[2] > (unsigned short)(n>>16) ? 1 : -1);
	if(a->word[3] != (unsigned short)n)
		return(a->word[3] > (unsigned short)n ? 1 : -1);
	return(0);
}

static
nadd(a, n)
struct number *a;
long n;
{
	int i;
	for(i=3; i>=0; i--) {
		n += (long)a->word[i];
		a->word[i] = n;
		n >>= 16;
	}
	return(n || a->word[0]>255);
}

/* Like the original atof/getch: spaces are ignored, the first other
 * non-digit terminates the number, and zero/empty input ends the session.
 */
static
nread(a, text)
struct number *a;
char *text;
{
	int c, i, overflow;
	long carry;
	nset(a, 0L);
	overflow = 0;
	for(;;) {
		c = text ? *text++ : getchar();
		if(c==' ') continue;
		if(c<'0' || c>'9') break;
		carry = c-'0';
		for(i=3; i>=0; i--) {
			carry += (long)a->word[i]*10;
			a->word[i] = carry;
			carry >>= 16;
		}
		if(carry || a->word[0]>255) overflow = 1;
	}
	if(overflow) return(-1);
	return(!nzero(a));
}

static
nprint(a)
struct number *a;
{
	struct number n, q;
	char buf[18];
	int i;
	n = *a;
	i = 0;
	do {
		buf[i++] = '0'+ndiv(&n, 10L, &q);
		n = q;
	} while(!nzero(&n));
	while(i) putchar(buf[--i]);
	putchar('\n');
}

/* Integer square root, one base-four digit per iteration. All temporary
 * values fit positive signed longs, including at the 56-bit boundary.
 */
static long
nroot(a)
struct number *a;
{
	long root, rem, trial;
	int bit;
	root = rem = 0;
	for(bit=55; bit>0; bit-=2) {
		rem = rem*4 + ((a->word[3-bit/16] >> (bit%16-1))&3);
		trial = root*4+1;
		root *= 2;
		if(rem>=trial) {
			rem -= trial;
			root++;
		}
	}
	return(root);
}

/* Original 210-wheel: candidates relatively prime to 2, 3, 5 and 7. */
static char wheel[48] = {
	10,2,4,2,4,6,2,6,4,2,4,6,6,2,6,4,
	2,6,4,6,8,4,2,4,2,4,8,6,4,6,2,4,
	6,2,6,6,4,2,4,6,2,6,4,2,4,2,10,2
};
