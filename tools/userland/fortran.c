#include "/usr/src/libI77/fio.h"

double d_sqrt(), pow_di();
main()
{
	double x;
	long exponent, number, value;
	char text[8], copy[8];
	icilist ctl;
	x = 81.0;
	if (d_sqrt(&x) != 9.0) return 1;
	x = 3.0; exponent = 4;
	if (pow_di(&x, &exponent) != 81.0) return 2;
	s_copy(copy, "abc", 8L, 3L);
	if (strncmp(copy, "abc     ", 8)) return 3;
	ctl.icierr = ctl.iciend = 0;
	/* The V7 Fortran frontend emits lowercase format descriptors. */
	ctl.iciunit = text; ctl.icifmt = "(i5)";
	ctl.icirlen = 8; ctl.icirnum = 1;
	number = 1; value = 123;
	if (s_wsfi(&ctl) || do_fio(&number, (char *)&value, 4L) || e_wsfi()) return 4;
	if (strncmp(text, "  123   ", 8)) return 5;
	return 0;
}
