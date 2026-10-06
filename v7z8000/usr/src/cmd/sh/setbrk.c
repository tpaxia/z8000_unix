/* UNIX V7 source code: see /COPYRIGHT or www.tuhs.org for details. */
/* Changes: Copyright (c) 1999 Robert Nordier. All rights reserved. */

#
/*
 *	UNIX shell
 *
 *	S. R. Bourne
 *	Bell Telephone Laboratories
 *
 */

#include	"defs.h"

BYTPTR sbrk();

setbrk(incr)
{
	REG BYTPTR	a=sbrk(incr);
	IF a != (BYTPTR)-1 THEN brkend=a+incr FI
	return((INT)a);
}
