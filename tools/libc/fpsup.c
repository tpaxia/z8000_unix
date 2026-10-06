/*
 * modf, ldexp and frexp for IEEE double precision on the Z8000.
 *
 * Seventh Edition has these in PDP-11 assembly (modf11.s, ldexp11.s,
 * frexp11.s) for the PDP-11 floating format. Here a double is IEEE
 * binary64 held in four 16-bit words, most significant first: one sign
 * bit, eleven exponent bits biased by 1023, fifty-two fraction bits.
 */
#include <errno.h>

extern int errno;

union dw {
	double	d;
	unsigned w[4];
};

#define	EXPMASK	077760		/* exponent field in the first word */
#define	EXPONE	1023

/* split d into integer part (*ip) and fraction (returned), same sign */
double
modf(d, ip)
double d, *ip;
{
	union dw u;
	register int e, i, bits;

	u.d = d;
	e = ((u.w[0] >> 4) & 03777) - EXPONE;
	if (e < 0) {
		*ip = 0.0;
		return (d);
	}
	if (e >= 52) {
		*ip = d;
		return (0.0);
	}
	bits = 52 - e;		/* fraction bits, counted from the right */
	for (i = 3; bits > 0; i--) {
		if (bits >= 16) {
			u.w[i] = 0;
			bits -= 16;
		} else {
			u.w[i] &= ~((1 << bits) - 1);
			bits = 0;
		}
	}
	*ip = u.d;
	return (d - u.d);
}

/* d times two to the power n */
double
ldexp(d, n)
double d;
{
	union dw u;
	register int e;

	u.d = d;
	e = (u.w[0] >> 4) & 03777;
	if (e == 0) {
		/* zero, or too small to hold normalized: scale it up first */
		if (d == 0.0)
			return (d);
		u.d = d * 18014398509481984.0;		/* 2**54 */
		e = (u.w[0] >> 4) & 03777;
		n -= 54;
	}
	e += n;
	if (e >= 2047) {
		errno = ERANGE;
		u.w[0] = (u.w[0] & 0100000) | EXPMASK;	/* signed infinity */
		u.w[1] = u.w[2] = u.w[3] = 0;
		return (u.d);
	}
	if (e <= 0)
		return (0.0);
	u.w[0] = (u.w[0] & ~EXPMASK) | (e << 4);
	return (u.d);
}

/* d as a fraction in [1/2, 1) times two to the power *ep */
double
frexp(d, ep)
double d;
int *ep;
{
	union dw u;
	register int e;

	*ep = 0;
	if (d == 0.0)
		return (d);
	u.d = d;
	e = (u.w[0] >> 4) & 03777;
	if (e == 0) {
		u.d = d * 18014398509481984.0;		/* 2**54 */
		e = ((u.w[0] >> 4) & 03777) - 54;
	}
	*ep = e - (EXPONE - 1);
	u.w[0] = (u.w[0] & ~EXPMASK) | ((EXPONE - 1) << 4);
	return (u.d);
}
