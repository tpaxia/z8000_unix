#
/*
 *
 *	UNIX debugger
 *
 */

#include "defs.h"
#include "object.h"
extern struct object adbobj;


MSG		BADFIL;

SYMTAB		symbol;
BOOL		localok;
INT		lastframe;
SYMSLAVE		*symvec;
POS		maxoff;
L_INT		maxstor;

/* symbol management */
L_INT		symbas;
L_INT		symcnt;
L_INT		symnum;
L_INT		localval;
char		symrqd -1;
SYMTAB		symbuf[SYMSIZ];
SYMPTR		symnxt;
SYMPTR		symend;


INT		fsym;
STRING		errflg;
POS		findsym();


/* symbol table and file handling service routines */

longseek(f, a)
L_INT a;
{
	return(lseek(f,a,0) != -1);
}

valpr(v,idsp)
{
	POS		d;
	d = findsym(v,idsp);
	IF d < maxoff
	THEN	printf("%.8s", symbol.symc);
		IF d
		THEN	printf(OFFMODE, d);
		FI
	FI
}

localsym(cframe)
L_INT cframe;
{
	INT symflg;
	WHILE nextsym() ANDF localok
		ANDF symbol.symc[0]!='~'
		ANDF (symflg=symbol.symf)!=037
	DO IF symflg>=2 ANDF symflg<=4
	   THEN localval=symbol.symv;
		return(TRUE);
	   ELIF symflg==1
	   THEN localval=leng(shorten(cframe)+symbol.symv);
		return(TRUE);
	   ELIF symflg==20 ANDF lastframe
	   THEN localval=leng(lastframe+2*symbol.symv-10);
		return(TRUE);
	   FI
	OD
	return(FALSE);
}
psymoff(v,type,s)
L_INT v; int type; char *s;
{
	POS		w;
	w = findsym(shorten(v),type);
	IF w >= maxoff
	THEN printf(LPRMODE,v);
	ELSE printf("%.8s", symbol.symc);
	     IF w THEN printf(OFFMODE,w); FI
	FI
	printf(s);
}

POS	findsym(svalue,type)
POS	svalue;
INT	type;
{
	L_INT		diff, value, symval, offset;
	INT		symtyp;
	REG SYMSLAVE	*symptr;
	SYMSLAVE	*symsav;
	value=svalue; diff = 0377777L; symsav=0;
	IF type!=NSYM ANDF (symptr=symvec)
	THEN	WHILE diff ANDF (symtyp=symptr->typslave)!=ESYM
		DO  IF symtyp==type
		    THEN symval=leng(symptr->valslave);
			 IF value-symval<diff
			    ANDF value>=symval
			 THEN diff = value-symval;
			      symsav=symptr;
			 FI
		    FI
		    symptr++;
		OD
		IF symsav
		THEN	offset=leng(symsav-symvec);
			symcnt=symnum-offset;
			getsout(offset);
		FI
	FI
	return(shorten(diff));
}

getsout(index)
long index;
{
 struct osymbol entry;
 int i;
 if(!objsym(&adbobj,(int)index,&entry)) {errflg=BADFIL;return(FALSE);}
 for(i=0;i<8;i++)symbol.symc[i]=entry.name[i];
 symbol.symf=entry.type;symbol.symv=entry.value;
 return(TRUE);
}
nextsym()
{
 if(--symcnt<0)return(FALSE);
 return(getsout(symnum-symcnt));
}
symset()
{symcnt=symnum;}
SYMPTR symget()
{
 if(symcnt<=0)return(0);
 if(!getsout(symnum-symcnt)) {symcnt=0;return(0);}
 symcnt--;return(&symbol);
}
