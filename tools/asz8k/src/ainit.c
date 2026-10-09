#include "stdio.h"
/*
 * Changed to use the Standard I/O library  9/13/82  FZ
 */
#include "acom.h"

/*
 * Version Z.1, 4/5/83.  Added external declarations of segflg and oflag.
 */

/*
 * Version 3.2, 5/27/82.  Added uext (-u) switch.
 */
static	char	ident[] = "@(#)a.init.c	3.2";

extern	char	segflg;
extern	char	oflag;
extern int aflag;
extern int zflag;

/*
 * badpre - Issues a fatal error for a bad PREDEF file.
 */
badpre() {

	fprintf(ERROR,"PREDEF file error at line %u\n",infp->in_seq);
	exit(1);
}

/*
 * getdat - Gets the date and time and puts them into datstr.
 *	    Disabled in CP/M version.
 */
getdat() {
}
#define BinFile	1
#define AscFile	0

extern	char	putfile[];
extern	char	optfile[];
/*
 * init - Performs assembler initialization.
 */
init(argc,argv) int argc; char *argv[]; {

char	**av;
char	*ap;
char	*ep;
char	*sp;
char	fname[15];
int	fd, mach;
FILE	*newfile();
static char errbuf[BUFSIZ];

	/* Keep stdio off the break managed by palloc. */
	setbuf(ERROR, errbuf);
#ifndef ASZ_HOST
	phytop = phylim = sbrk(0);  /* needed only for monitoring */
#else
	pinit();
#endif
	getdat();
	prname = "asz8k";
	while((ap = *++argv) != 0) {  /* read command line arguments */
		if(*ap == '-') {  /* switches */
			while(*++ap) switch(*ap) {

			case 'a':
#ifdef ASZ_LEGACY
				aflag = 1; zflag = 0; break;
#else
				fprintf(ERROR,"Only s.out output is supported\n"); exit(1);
#endif
			case 'z': zflag = 1; break;
			case 'c': pccflg = 1; break;
			case 'g': machineflg = pccflg = 1; break;

			case 'l':
				lflag = 1;
				break;

			case 'o':
				oflag = 1;
				if (argv[1] == NULL || strlen(argv[1]) >= 128)
					usage();
				strcpy(optfile,*++argv);
				break;

			case 'p':
				pflag = 1;
				break;

			case 's':
				segflg = 1;
				break;

			case 'u':
				uext = 1;
				break;

			case 'x':
				xflag = lflag = 1;
				break;

			default:
				printf("error case\n");
				usage();

			}
		} else {  /* file name */
			if(srcfile) {printf("srcfile != 0\n"); usage();}
			srcfile = ap;
		}
	}
	if (aflag && zflag) usage();
	if (pccflg && (!zflag || (segflg && !machineflg))) usage();
	if (pccflg && !machineflg) uext = 1;
	objectseg = segflg;
	if (aflag && segflg) {
		fprintf(ERROR,"a.out: segmented output is not implemented\n");
		exit(1);
	}
	if(!srcfile) {printf("no srcfile\n"); usage();}
	if((sp = rindex(srcfile,'/')) == 0) sp = srcfile; else sp++;
	if(strlen(sp) > 14) usage();
	strncpy(titl1,srcfile,TITSIZ-1); titl1[TITSIZ-1] = 0;
	strcpy(fname,sp);
	if((ep = rindex(fname,'.')) == 0)
		usage();
	else
		ep++;
	if( segflg ) {		/* Prog name must end with "8ks". */
	    if( strcmp (ep,"8ks") != 0 && !(machineflg && !strcmp(ep,"s"))) {
		printf("Segmented source file must end with '.8ks'\n");
		usage();
	    }
	}
	else 			/* PCC accepts its native .az8 assembly suffix. */
	    if( strcmp (ep,"8kn") != 0 && !(pccflg && !strcmp(ep,"az8")) && !(machineflg && !strcmp(ep,"s"))) {
		printf("Nonsegmented source file must end with '.8kn'\n");
		usage();
	    }
	if((fd = open(srcfile,0)) == -1) {
		fprintf(ERROR,"Cannot open %s\n",srcfile);
		exit(1);
	}
	strcpy(ep,zflag ? "so" : aflag ? "b" : "obj");  OBJECT = newfile(oflag ? optfile : fname,BinFile);
	strcpy(putfile,fname);
	if(lflag) {
		strcpy(ep,"lst");  LIST = newfile(fname,AscFile);
	}
	vinit();
	mach = machineflg; machineflg = 0;
	predef();
	machineflg = mach;
}

/*
 * newfile - Creates a new file of the specified name,
 * and returns the file descriptor.
 */
FILE *
newfile(s,binary) char *s; char binary; {

FILE	*fd;
static char objbuf[BUFSIZ], lstbuf[BUFSIZ];

	if((fd = fopen(s,(aflag || zflag) && binary ? "w+" : "w")) == NULL) {
		fprintf(ERROR,"Cannot create %s\n",s);
		exit(1);
	}
	setbuf(fd, binary ? objbuf : lstbuf);
	return(fd);
}

/*
 * preget - Gets a token, issues a fatal error if it is not the specified
 * type, and returns the token's value.
 */
preget(typ) int typ; {

	if(token() != typ) badpre();
	return(tokval);
}

/*
 * usage - Issues a fatal error for an illegal command line.
 */
usage() {

	fprintf(ERROR,"Usage: asz8k [-o outfile] [-azcgluxs] file.8k{n|s} (or -zc file.az8, -zg file.s)\n");
	exit(1);
}
