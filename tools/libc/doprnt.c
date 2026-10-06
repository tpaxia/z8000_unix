/*
 * _doprnt - formatted output conversion for printf, fprintf and sprintf.
 *
 * In Seventh Edition this routine is PDP-11 assembly (libc/stdio/doprnt.s).
 * This is the same interface written in C for the Z8000:
 *
 *	_doprnt(fmt, argp, iop)
 *
 * argp points at the first argument on the caller's stack. An int or a
 * pointer takes one word there, a long two (high word first) and a double
 * four.
 *
 * Conversions: d o x u c s e f g and %, the l modifier, D O X U for long,
 * a leading - for left adjustment, a leading 0 for zero padding, a field
 * width and a precision, either of which may be *.
 */
#include <stdio.h>

char	*ecvt();
char	*fcvt();
char	*gcvt();

#define	NDIG	80

static char *
conv(val, base, end)
unsigned long val;
register char *end;
{
	*--end = '\0';
	do {
		*--end = "0123456789abcdef"[(int)(val % base)];
		val /= base;
	} while (val != 0);
	return (end);
}

/*
 * Format d as %e or %f into buf; returns buf. The sign is reported
 * through *negp so that zero padding can go between it and the digits.
 */
static char *
fmtflt(d, fc, prec, buf, negp)
double d;
char *buf;
int *negp;
{
	register char *p, *q;
	int decpt, sign, i;

	q = buf;
	if (fc == 'e') {
		p = ecvt(d, prec + 1, &decpt, &sign);
		*q++ = *p++;
		if (prec > 0)
			*q++ = '.';
		while (*p)
			*q++ = *p++;
		*q++ = 'e';
		decpt = d == 0.0 ? 0 : decpt - 1;
		if (decpt < 0) {
			*q++ = '-';
			decpt = -decpt;
		} else
			*q++ = '+';
		*q++ = decpt / 10 + '0';
		*q++ = decpt % 10 + '0';
	} else {
		p = fcvt(d, prec, &decpt, &sign);
		if (decpt <= 0)
			*q++ = '0';
		else
			for (i = 0; i < decpt; i++)
				*q++ = *p ? *p++ : '0';
		if (prec > 0) {
			*q++ = '.';
			for (i = 0; i < prec; i++) {
				if (decpt < 0) {
					*q++ = '0';
					decpt++;
				} else
					*q++ = *p ? *p++ : '0';
			}
		}
	}
	*q = '\0';
	*negp = sign;
	return (buf);
}

_doprnt(fmt, argp, iop)
register char *fmt;
int *argp;
register FILE *iop;
{
	register int c;
	char *s;
	int width, prec, hasprec, left, zero, lng, neg, len, pad, base;
	unsigned long uval;
	long lval;
	double d;
	char buf[NDIG];

	for (;;) {
		while ((c = *fmt++) != '%') {
			if (c == '\0')
				return;
			putc(c, iop);
		}
		width = prec = hasprec = left = zero = lng = neg = 0;
		c = *fmt++;
		if (c == '-') {
			left = 1;
			c = *fmt++;
		}
		if (c == '0') {
			zero = 1;
			c = *fmt++;
		}
		if (c == '*') {
			width = *argp++;
			if (width < 0) {
				left = 1;
				width = -width;
			}
			c = *fmt++;
		} else
			while (c >= '0' && c <= '9') {
				width = width * 10 + c - '0';
				c = *fmt++;
			}
		if (c == '.') {
			hasprec = 1;
			c = *fmt++;
			if (c == '*') {
				prec = *argp++;
				c = *fmt++;
			} else
				while (c >= '0' && c <= '9') {
					prec = prec * 10 + c - '0';
					c = *fmt++;
				}
		}
		if (c == 'l') {
			lng = 1;
			c = *fmt++;
		}
		base = 0;
		switch (c) {

		case 'D':
			lng = 1;
		case 'd':
			if (lng) {
				lval = *(long *)argp;
				argp += 2;
			} else
				lval = *argp++;
			if (lval < 0) {
				neg = 1;
				uval = -lval;
			} else
				uval = lval;
			s = conv(uval, 10, buf + NDIG);
			break;

		case 'O':
			lng = 1;
		case 'o':
			base = 8;
			break;

		case 'X':
			lng = 1;
		case 'x':
			base = 16;
			break;

		case 'U':
			lng = 1;
		case 'u':
			base = 10;
			break;

		case 'c':
			buf[0] = *argp++;
			buf[1] = '\0';
			s = buf;
			break;

		case 's':
			s = (char *)*argp++;
			if (s == NULL)
				s = "(null)";
			break;

		case 'e':
		case 'f':
			d = *(double *)argp;
			argp += 4;
			s = fmtflt(d, c, hasprec ? prec : 6, buf, &neg);
			break;

		case 'g':
			d = *(double *)argp;
			argp += 4;
			s = gcvt(d, hasprec ? prec : 6, buf);
			break;

		case '\0':
			return;

		default:
			putc(c, iop);
			continue;
		}
		if (base) {
			if (lng) {
				uval = *(unsigned long *)argp;
				argp += 2;
			} else
				uval = (unsigned)*argp++;
			s = conv(uval, base, buf + NDIG);
		}
		for (len = 0; s[len] != '\0'; len++)
			;
		if (c == 's' && hasprec && len > prec)
			len = prec;
		if (c == 'c')
			len = 1;
		pad = width - len - neg;
		if (neg && (zero || left || pad <= 0)) {
			putc('-', iop);
			neg = 0;
		}
		if (!left)
			while (pad-- > 0)
				putc(zero ? '0' : ' ', iop);
		if (neg)
			putc('-', iop);
		while (len-- > 0)
			putc(*s++, iop);
		if (left)
			while (pad-- > 0)
				putc(' ', iop);
	}
}
