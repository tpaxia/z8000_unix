#
/*
 *
 *	UNIX debugger
 *
 */

#include "defs.h"
#include "object.h"
#include <sys/crash.h>
struct object adbobj;


MSG		BADNAM;
MSG		BADMAG;

MAP		txtmap;
MAP		datmap;
SYMSLAVE	*symvec;
INT		wtflag;
INT		fcor;
INT		fsym;
L_INT		maxfile;
L_INT		maxstor;
L_INT		txtsiz;
L_INT		datsiz;
L_INT		datbas;
L_INT		stksiz;
STRING		errflg;
INT		magic;
L_INT		symbas;
L_INT		symnum;
L_INT		entrypt;

INT		argcount;
INT kernelcore;
INT kernelregs;
INT adbseg;
INT		signo;
POS		corhdr[CORESIZE];
POS		*endhdr;

STRING		symfil	"a.out";
STRING		corfil	"core";

setsym()
{
 FILE *file;
 long length;
 int i;
 SYMSLAVE *ptr;
 SYMPTR symp;
 fsym=getfile(symfil,1);txtmap.ufd=fsym;
 if(fsym<0)return;
 file=fopen(symfil,"r");if(!file)return;
 fseek(file,0L,2);length=ftell(file);
 if(objread(&adbobj,file,0L,length)!=1 || adbobj.segmented ||
    !(adbobj.flags&SO_STRIP)) {
  printf("invalid NONSEG s.out executable\n");exit(1);
 }
 magic=adbobj.magic;txtsiz=adbobj.text;datsiz=adbobj.data;
 symnum=adbobj.symbols;symbas=adbobj.symoff;
 txtmap.b1=0;txtmap.e1=magic==SO_NMAG?txtsiz+datsiz:txtsiz;
 txtmap.f1=adbobj.imageoff;
 txtmap.b2=datbas=magic==SO_NMAG?txtsiz:0;
 txtmap.e2=txtmap.b2+datsiz;txtmap.f2=adbobj.imageoff+txtsiz;
 entrypt=0;
 {char h[4];if(objbytes(&adbobj,14L,h,4))entrypt=so_get32(h);}
 if(symnum>8190L) {printf("too many symbols\n");exit(1);}
 ptr=symvec=sbrk((int)(symnum+1)*sizeof(SYMSLAVE));
 if(ptr==(SYMSLAVE *)-1) {printf("%s\n",BADNAM);exit(1);}
 symset();
 while(symp=symget()) {
  ptr->valslave=symp->symv;ptr->typslave=SYMTYPE(symp->symf);ptr++;
 }
 ptr->typslave=ESYM;
}

setcor()
{
 struct user *up;
 struct kcontext ctx;
 int i;
 SYMPTR symp;
 long length, need;
 fcor=getfile(corfil,2);datmap.ufd=fcor;
 endhdr=((struct user *)corhdr)->u_regs-1;
 if(fcor<0) {if(kernelcore)exit(1);return;}
 if(kernelcore) {
  datmap.b1=0;datmap.e1=0xe000L;datmap.f1=65536L;
  datmap.b2=datmap.e2=0;datmap.f2=0;
  length=lseek(fcor,0L,2);
  symp=lookupsym("physmem");
  if(!symp || length<196608L || length>8388608L ||
     length!=(long)get(leng(symp->symv),DSP)*2048L || errflg) {
   printf("incomplete kernel RAM dump or missing kernel namelist\n");exit(1);
  }
  symp=lookupsym("kcrash");
  if(symp) {
   lseek(fcor,65536L+leng(symp->symv),0);
   if(read(fcor,&ctx,sizeof(ctx))!=sizeof(ctx)) {
    printf("incomplete kernel register context\n");exit(1);
   }
   if(ctx.kind) {
    if(ctx.magic!=KC_MAGIC || ctx.version!=KC_VERSION ||
       (ctx.kind!=KC_PANIC && ctx.kind!=KC_FAULT) ||
       !(ctx.regs[16]&0x4000) || !ctx.upage ||
       ((long)ctx.upage+2)*2048L>length) {
     printf("invalid kernel register context\n");exit(1);
    }
    for(i=0;i<19;i++)endhdr[i+1]=ctx.regs[i];
    kernelregs=1;adbseg=(ctx.regs[16]&0x8000)!=0;
    datmap.b2=0xf000L;datmap.e2=65536L;
    datmap.f2=(long)ctx.upage*2048L;
   }
  }
  return;
 }
 length=lseek(fcor,0L,2);lseek(fcor,0L,0);
 if(read(fcor,corhdr,ctob(USIZE))!=ctob(USIZE)) {
  printf("incomplete process core\n");exit(1);
 }
 up=(struct user *)corhdr;
 txtsiz=ctob((long)up->u_tsize);datsiz=ctob((long)up->u_dsize);
 stksiz=ctob((long)up->u_ssize);
 need=ctob(USIZE)+datsiz+stksiz;
 if(length!=need || datsiz<0 || datsiz>65536L || stksiz>65536L ||
    datsiz+stksiz>65536L ||
    (magic && up->u_exdata.ux_mag!=((unsigned)magic==SO_NID?0411:0407))) {
  printf("%s\n",BADMAG);exit(1);
 }
 datmap.b1=datbas=0;datmap.e1=datsiz;datmap.f1=ctob(USIZE);
 datmap.b2=maxstor-stksiz;datmap.e2=maxstor;datmap.f2=ctob(USIZE)+datsiz;
}

create(f)
STRING	f;
{	int fd;
	IF (fd=creat(f,0644))>=0
	THEN close(fd); return(open(f,wtflag));
	ELSE return(-1);
	FI
}

getfile(filnam,cnt)
STRING	filnam;
{
	REG INT		fsym;

	IF !eqstr("-",filnam)
	THEN	fsym=open(filnam,wtflag);
		IF fsym<0 ANDF argcount>cnt
		THEN	IF wtflag
			THEN	fsym=create(filnam);
			FI
			IF fsym<0
			THEN printf("cannot open `%s'\n", filnam);
			FI
		FI
	ELSE	fsym = -1;
	FI
	return(fsym);
}
