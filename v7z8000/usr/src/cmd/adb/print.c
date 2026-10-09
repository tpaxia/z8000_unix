#
/*
 *
 *	UNIX debugger
 *
 */

#include "defs.h"


MSG		LONGFIL;
MSG		NOTOPEN;
MSG		A68BAD;
MSG		A68LNK;
MSG		BADMOD;

MAP		txtmap;
MAP		datmap;

SYMTAB		symbol;
INT		lastframe;
INT		callpc;

INT		infile;
INT		outfile;
CHAR		*lp;
INT		maxoff;
INT		maxpos;
INT		octal;

/* symbol management */
L_INT		localval;

/* breakpoints */
BKPTR		bkpthead;

REGLIST reglist[] {
 "r0", 1,
 "r1", 2,
 "r2", 3,
 "r3", 4,
 "r4", 5,
 "r5", 6,
 "r6", 7,
 "r7", 8,
 "r8", 9,
 "r9", 10,
 "r10", 11,
 "r11", 12,
 "r12", 13,
 "r13", 14,
 "r14", 15,
 "r15", 16,
 "fcw", ps, "seg", UREG_SEG+1, "pc", pc,
 "sp", sp, "fp", r5, "ps", ps,
};

char		lastc;
POS		corhdr[];
POS		*endhdr;

INT		fcor;
STRING		errflg;
INT		signo;


L_INT		dot;
L_INT		var[];
STRING		symfil;
STRING		corfil;
INT		pid;
INT kernelcore;
INT kernelregs;
INT adbseg;
L_INT		adrval;
INT		adrflg;
L_INT		cntval;
INT		cntflg;

STRING		signals[] {
		"",
		"hangup",
		"interrupt",
		"quit",
		"illegal instruction",
		"trace/BPT",
		"IOT",
		"EMT",
		"floating exception",
		"killed",
		"bus error",
		"memory fault",
		"bad system call",
		"broken pipe",
		"alarm call",
		"terminated",
};




/* general printing routines ($) */

printtrace(modif)
{
	INT		narg, i, stat, name, limit;
	POS		dynam;
	REG BKPTR	bkptr;
	CHAR		hi, lo;
	INT		word;
	STRING		comptr;
	L_INT		argp, frame, link;
	SYMPTR		symp;

	IF cntflg==0 THEN cntval = -1; FI

	switch (modif) {

	    case '<':
	    case '>':
		{CHAR		file[64];
		INT		index;

		index=0;
		IF modif=='<'
		THEN	iclose();
		ELSE	oclose();
		FI
		IF rdc()!=EOR
		THEN	REP file[index++]=lastc;
			    IF index>=63 THEN error(LONGFIL); FI
			PER readchar()!=EOR DONE
			file[index]=0;
			IF modif=='<'
			THEN	infile=open(file,0);
				IF infile<0
				THEN	infile=0; error(NOTOPEN);
				FI
			ELSE	outfile=open(file,1);
				IF outfile<0
				THEN	outfile=creat(file,0644);
				ELSE	lseek(outfile,0L,2);
				FI
			FI

		FI
		lp--;
		}
		break;

	    case 'o':
		octal = TRUE; break;

	    case 'd':
		octal = FALSE; break;

	    case 'q': case 'Q': case '%':
		done();

	    case 'w': case 'W':
		maxpos=(adrflg?adrval:MAXPOS);
		break;

	    case 's': case 'S':
		maxoff=(adrflg?adrval:MAXOFF);
		break;

	    case 'v': case 'V':
		prints("variables\n");
		FOR i=0;i<=35;i++
		DO IF var[i]
		   THEN printc((i<=9 ? '0' : 'a'-10) + i);
			printf(" = %Q\n",var[i]);
		   FI
		OD
		break;

	    case 'z': case 'Z':
		if(adrflg) {
		 if(adrval!=0 && adrval!=1)error("use 0$z for NONSEG or 1$z for SEG");
		 adbseg=adrval;
		}
		printf("instruction mode: %s\n",adbseg?"SEG":"NONSEG");
		break;

	    case 'm': case 'M':
		printmap("? map",&txtmap);
		printmap("/ map",&datmap);
		break;

	    case 0: case '?':
		IF pid
		THEN printf("pcs id = %d\n",pid);
		ELSE prints("no process\n");
		FI
		sigprint(); flushbuf();

	    case 'r': case 'R':
		if(kernelcore && !kernelregs)error("kernel dump has no saved kernel register context");
		printregs();
		return;

	    case 'f': case 'F':
		if(kernelcore)error("kernel dump has no saved floating register context");
		printfregs(modif=='F');
		return;

	    case 'c': case 'C':
		if(kernelcore && !kernelregs && !adrflg)error("specify the kernel frame address explicitly");
		frame=(adrflg?adrval:endhdr[r5])&EVEN; lastframe=0;
		callpc=(adrflg?get(frame+2,DSP):endhdr[pc]);
		WHILE cntval--
		DO	chkerr();
			narg = findroutine(frame);
			printf("%.8s(", symbol.symc);
			argp = frame+4;
			IF --narg >= 0
			THEN	printf("%o", get(argp, DSP));
			FI
			WHILE --narg >= 0
			DO	argp += 2;
				printf(",%o", get(argp, DSP));
			OD
			prints(")\n");

			IF modif=='C'
			THEN WHILE localsym(frame)
			     DO word=get(localval,DSP);
				printf("%8t%.8s:%10t", symbol.symc);
				IF errflg THEN prints("?\n"); errflg=0; ELSE printf("%o\n",word); FI
			     OD
			FI

			if(kernelcore) {
			 if(eqstr(symbol.symc,"_scwrap"))break;
			 if(eqstr(symbol.symc,"_nviwrap") || eqstr(symbol.symc,"_viwrap")) {
			  if(!(get(frame+32,DSP)&0x4000) ||
			     (unsigned)get(frame+34,DSP)!=0x8100)break;
			  callpc=get(frame+36,DSP);
			 }
			}
			lastframe=frame;
			frame=get(frame, DSP)&EVEN;
			IF frame==0 THEN break; FI
			IF frame<=leng(lastframe) THEN error("invalid frame chain"); FI
		OD
		break;

	    /*print externals*/
	    case 'e': case 'E':
		symset();
		WHILE (symp=symget())
		DO chkerr();
		   IF (symp->symf)==043 ORF (symp->symf)==044
		   THEN printf("%.8s:%12t%o\n", symp->symc, get(leng(symp->symv),DSP));
		   FI
		OD
		break;

	    case 'a': case 'A':
		error("Algol frames are not supported on Z8000");
		break;

	    /*set default c frame*/
	    /*print breakpoints*/
	    case 'b': case 'B':
		printf("breakpoints\ncount%8tbkpt%24tcommand\n");
		FOR bkptr=bkpthead; bkptr; bkptr=bkptr->nxtbkpt
		DO IF bkptr->flag
		   THEN printf("%-8.8d",bkptr->count);
			psymoff(leng(bkptr->loc),ISYM,"%24t");
			comptr=bkptr->comm;
			WHILE *comptr DO printc(*comptr++); OD
		   FI
		OD
		break;

	    default: error(BADMOD);
	}

}

printmap(s,amap)
STRING	s; MAP *amap;
{
	int file;
	file=amap->ufd;
	printf("%s%12t`%s'\n",s,(file<0 ? "-" : (file==fcor ? corfil : symfil)));
	printf("b1 = %-16Q",amap->b1);
	printf("e1 = %-16Q",amap->e1);
	printf("f1 = %-16Q",amap->f1);
	printf("\nb2 = %-16Q",amap->b2);
	printf("e2 = %-16Q",amap->e2);
	printf("f2 = %-16Q",amap->f2);
	printc(EOR);
}

printfregs(longpr)
{
 int i,j;
 unsigned char *state;
 state=(unsigned char *)((struct user *)corhdr)->u_fpe;
 prints("software EPU state (raw words)\n");
 for(i=0;i<8;i++) {
  printf("fr%d%8t",i);
  for(j=0;j<10;j+=2)printf("%x ",(state[i*10+j]<<8)|state[i*10+j+1]);
  printc(EOR);
 }
}

printregs()
{
	REG REGPTR	p;
	INT		v;

	FOR p=reglist; p < &reglist[NREG]; p++
	DO	printf("%s%8t%o%8t", p->rname, v=endhdr[p->roffs]);
		valpr(v,(p->roffs==pc?ISYM:DSYM));
		printc(EOR);
	OD
	printpc();
}

getreg(regnam)
{
 char name[8], *saved;
 int n,i;
 saved=lp;n=0;name[n++]=regnam;
 while(n<7 && (letter(*lp) || digit(*lp)))name[n++]=readchar();
 name[n]=0;
 for(i=0;i<NREG+3;i++)if(eqstr(name,reglist[i].rname))return(reglist[i].roffs);
 lp=saved;return(0);
}

printpc()
{
	if(kernelcore && kernelregs && (unsigned)endhdr[18]!=0x8100) {
	 printf("pc segment %x offset %x\n",endhdr[18],endhdr[pc]);return;
	}
	adbseg=((unsigned)endhdr[ps]&0x8000)!=0;
	dot=endhdr[pc];
	psymoff(dot,ISYM,":%16t"); printins(0,ISP,chkget(dot,ISP));
	printc(EOR);
}

sigprint()
{
	prints(signals[signo]);
}

