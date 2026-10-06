/* UNIX V7 source code: see /COPYRIGHT or www.tuhs.org for details. */
/* Changes: Copyright (c) 1999 Robert Nordier. All rights reserved. */

#
/*
 * UNIX shell
 *
 * S. R. Bourne
 * Bell Telephone Laboratories
 *
 */

#include	"defs.h"

STKPTR		stakbot=nullstr;



/* ========	storage allocation	======== */

/* Z8001 SEGT does not retry the failed store. Reserve workspace first,
 * rather than relying on V7's SIGSEGV-driven break extension.
 */
stakroom(end)
STRING end;
{
	IF end < stakbot THEN error(nospace) FI
	IF end > brkend ANDF setbrk(end-brkend) == -1
	THEN error(nospace) FI
}

STKPTR	getstak(asize)
	INT		asize;
{	/* allocate requested stack */
	REG STKPTR	oldstak;
	REG INT		size;

	size=round(asize,BYTESPERWORD);
	stakroom(stakbot+size);
	oldstak=stakbot;
	staktop = stakbot += size;
	return(oldstak);
}

STKPTR	locstak()
{	/* set up stack for local use
	 * should be followed by `endstak'
	 */
	IF brkend-stakbot<BRKINCR
	THEN	IF setbrk(brkincr) == -1 THEN error(nospace) FI
		IF brkincr < BRKMAX
		THEN	brkincr += 256;
		FI
	FI
	return(stakbot);
}

STKPTR	savstak()
{
	assert(staktop==stakbot);
	return(stakbot);
}

STKPTR	endstak(argp)
	REG STRING	argp;
{	/* tidy up after `locstak' */
	REG STKPTR	oldstak;
	stakput(argp,0);
	oldstak=stakbot; stakbot=staktop=(STKPTR)round(argp,BYTESPERWORD);
	return(oldstak);
}

VOID	tdystak(x)
	REG STKPTR 	x;
{
	/* try to bring stack back to x */
	WHILE ADR(stakbsy)>ADR(x)
	DO free(stakbsy);
	   stakbsy = stakbsy->word;
	OD
	staktop=stakbot=max(ADR(x),ADR(stakbas));
	rmtemp(x);
}

stakchk()
{
	IF (brkend-stakbas)>BRKINCR+BRKINCR
	THEN	setbrk(-BRKINCR);
	FI
}

STKPTR	cpystak(x)
	STKPTR		x;
{
	locstak();
	stakroom(stakbot+length(x));
	return(endstak(movstr(x,stakbot)));
}
