#
/*
 *
 *	UNIX debugger
 *
 */

#include "defs.h"


MSG		NOFORK;
MSG		ENDPCS;
MSG		BADWAIT;

CHAR		*lp;
INT		sigint;
INT		sigqit;

/* breakpoints */
BKPTR		bkpthead;

REGLIST		reglist[];

CHAR		lastc;
POS		corhdr[];
POS		*endhdr;

INT		fcor;
INT		fsym;
STRING		errflg;
INT		errno;
INT		signo;

L_INT		dot;
STRING		symfil;
INT		wtflag;
INT		pid;
L_INT		expv;
INT		adrflg;
L_INT		loopcnt;





/* service routines for sub process control */

getsig(sig)
{	return(expr(0) ? shorten(expv) : sig);
}

INT		userpc=1;

runpcs(runmode, execsig)
{
	INT		rc;
	REG BKPTR	bkpt;
	IF adrflg
	THEN userpc=shorten(dot);
	FI
	setbp();
	printf("%s: running\n", symfil);

	WHILE (loopcnt--)>0
	DO
#ifdef DEBUG
		printf("\ncontinue %d %d\n",userpc,execsig);
#endif
		stty(0,&usrtty);
		ptrace(runmode,pid,userpc,execsig);
		if(errno)error("cannot continue process");
		bpwait(); chkerr(); readregs();

		/*look for bkpt*/
		IF signo==0 ANDF (bkpt=scanbkpt(endhdr[pc]-2))
		THEN /*stopped at bkpt*/
		     userpc=endhdr[pc]=bkpt->loc;
             /* Without hardware step, consume the breakpoint. Restoring
              * its word and PC lets :c execute it normally, without stepping. */
             ptrace(WIUSER,pid,bkpt->loc,bkpt->ins);
             if(errno)error("cannot restore breakpoint instruction");
             ptrace(WUREGS,pid,REGADDR(pc),bkpt->loc);
             if(errno)error("cannot reset breakpoint PC");
             bkpt->flag=0;rc=1;
             command(bkpt->comm,':');

		ELSE rc=0; execsig=signo; userpc=1;
		FI
	OD
	return(rc);
}

endpcs()
{
	REG BKPTR	bkptr;
	IF pid
	THEN ptrace(EXIT,pid,0,0); pid=0; userpc=1;
	     FOR bkptr=bkpthead; bkptr; bkptr=bkptr->nxtbkpt
	     DO IF bkptr->flag
		THEN bkptr->flag=BKPTSET;
		FI
	     OD
	FI
}

setup()
{
	close(fsym); fsym = -1;
	IF (pid = fork()) == 0
	THEN ptrace(SETTRC,0,0,0);
	     signal(SIGINT,sigint); signal(SIGQUIT,sigqit);
	     doexec(); exit(0);
	ELIF pid == -1
	THEN error(NOFORK);
	ELSE bpwait(); readregs(); lp[0]=EOR; lp[1]=0;
	     fsym=open(symfil,wtflag);
	     IF errflg
	     THEN printf("%s: cannot execute\n",symfil);
		  endpcs(); error(0);
	     FI
	FI
}



doexec()
{
	STRING		argl[MAXARG];
	CHAR		args[LINSIZ];
	STRING		p, *ap, filnam;
	ap=argl; p=args;
	*ap++=symfil;
	REP	IF rdc()==EOR THEN break; FI
		*ap = p;
		WHILE lastc!=EOR ANDF lastc!=SP ANDF lastc!=TB DO *p++=lastc; readchar(); OD
		*p++=0; filnam = *ap+1;
		IF **ap=='<'
		THEN	close(0);
			IF open(filnam,0)<0
			THEN	printf("%s: cannot open\n",filnam); exit(0);
			FI
		ELIF **ap=='>'
		THEN	close(1);
			IF creat(filnam,0666)<0
			THEN	printf("%s: cannot create\n",filnam); exit(0);
			FI
		ELSE	ap++;
		FI
	PER lastc!=EOR DONE
	*ap++=0;
	execv(symfil, argl);
}

BKPTR	scanbkpt(adr)
{
	REG BKPTR	bkptr;
	FOR bkptr=bkpthead; bkptr; bkptr=bkptr->nxtbkpt
	DO IF bkptr->flag ANDF bkptr->loc==adr
	   THEN break;
	   FI
	OD
	return(bkptr);
}

delbp()
{
	REG INT		a;
	REG BKPTR	bkptr;
	FOR bkptr=bkpthead; bkptr; bkptr=bkptr->nxtbkpt
	DO IF bkptr->flag
	   THEN a=bkptr->loc;
		ptrace(WIUSER,pid,a,bkptr->ins);
	   FI
	OD
}

setbp()
{
	REG INT		a;
	REG BKPTR	bkptr;

	FOR bkptr=bkpthead; bkptr; bkptr=bkptr->nxtbkpt
	DO IF bkptr->flag
	   THEN a = bkptr->loc;
		bkptr->ins = ptrace(RIUSER, pid, a, 0);
		ptrace(WIUSER, pid, a, BPT);
		IF errno
		THEN prints("cannot set breakpoint: ");
		     psymoff(leng(bkptr->loc),ISYM,"\n");
		FI
	   FI
	OD
}

bpwait()
{
	REG INT w;
	INT stat;

	signal(SIGINT, 1);
	WHILE (w = wait(&stat))!=pid ANDF w != -1 DONE
	signal(SIGINT,sigint);
	gtty(0,&usrtty);
	stty(0,&adbtty);
	IF w == -1
	THEN pid=0;
	     errflg=BADWAIT;
	ELIF (stat & 0177) != 0177
	THEN IF signo = stat&0177
	     THEN sigprint();
	     FI
	     IF stat&0200
	     THEN prints(" - core dumped");
		  close(fcor);
		  setcor();
	     FI
	     pid=0;
	     errflg=ENDPCS;
	ELSE signo = stat>>8;
	     IF signo!=SIGTRC
	     THEN sigprint();
	     ELSE signo=0;
	     FI
	     flushbuf();
	FI
}

readregs()
{
 int i;
 POS *state;
 for(i=1;i<=NREG;i++) {
  endhdr[i]=ptrace(RUREGS,pid,REGADDR(i),0);
  if(errno)error("cannot read Z8000 registers");
 }
 state=(POS *)((struct user *)corhdr)->u_fpe;
 for(i=0;i<40;i++) {
  state[i]=ptrace(RUREGS,pid,(int)((struct user *)0)->u_fpe+2*i,0);
  if(errno)error("cannot read software EPU registers");
 }
}
