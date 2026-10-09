#
/*
 *
 *	UNIX debugger
 *
 */

#include "defs.h"


MSG		NOCFN;

INT		callpc;
BOOL		localok;
SYMTAB		symbol;

STRING		errflg;


/* NONSEG C frame: saved R13 at fp, return PC at fp+2. The object
 * format contains global symbols, not argument counts or local debug records. */
findroutine(cframe)
L_INT cframe;
{
 unsigned where;
 where=callpc;callpc=get(cframe+2,DSP);localok=FALSE;
 if(findsym(where,ISYM)==(unsigned)-1) {
  symbol.symc[0]='?';symbol.symc[1]=0;symbol.symv=0;
 }
 return(0);
}
