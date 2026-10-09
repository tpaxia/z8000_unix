#
/*
 *
 *	UNIX debugger - common definitions
 *
 */



/* Executables and symbols use s.out; process cores use the public u-area. */

#include <sys/param.h>
#include <sys/dir.h>
#include <sys/reg.h>
#include <sys/user.h>
#include <sgtty.h>
#include "mac.h"
#include "mode.h"


#define VARB	11
#define VARD	13
#define VARE	14
#define VARM	22
#define VARS	28
#define VART	29

#define COREMAGIC 0140000

#define RD	0
#define WT	1
#define NSP	0
#define	ISP	1
#define	DSP	2
#define STAR	4
#define STARCOM 0200
#define DSYM	7
#define ISYM	2
#define ASYM	1
#define NSYM	0
#define ESYM	(-1)
#define BKPTSET	1
#define BKPTEXEC 2
#define	SYMSIZ	100
#define MAXSIG	20

#define USERREG ((int)(((struct user *)0)->u_regs))
#define BPT 0x7fff
#define SETTRC 0
#define RDUSER 2
#define RIUSER 1
#define WDUSER 5
#define WIUSER 4
#define RUREGS 3
#define WUREGS 6
#define CONTIN 7
#define SINGLE 9
#define EXIT 8
#define NREG 19
#define CORESIZE (USIZE*32)
/* Index zero is the parser's 'no register' sentinel. */
#define ps (UREG_FCW+1)
#define pc (UREG_PC+1)
#define sp (UREG_SP+1)
#define r5 14 /* Z8000 C frame pointer is R13 */
#define r4 5
#define REGADDR(r) (USERREG+2*((r)-1))

#define MAXOFF	255
#define MAXPOS	80
#define MAXLIN	128
#define EOF	0
#define EOR	'\n'
#define TB	'\t'
#define QUOTE	0200
#define STRIP	0177
#define LOBYTE	0377
#define EVEN	-2


/* long to ints and back (puns) */
union {
	INT	I[2];
	L_INT	L;
} itolws;

#define leng(a)		((long)((unsigned)(a)))
#define shorten(a)	((int)(a))
#define itol(a,b)	(itolws.I[0]=(a), itolws.I[1]=(b), itolws.L)



/* result type declarations */
L_INT		inkdot();
long lseek();
SYMPTR		lookupsym();
SYMPTR		symget();
POS		get();
POS		chkget();
STRING		exform();
L_INT		round();
BKPTR		scanbkpt();
VOID		fault();

typedef struct sgttyb TTY;
TTY	adbtty, usrtty;
#include <setjmp.h>
jmp_buf erradb;
