/* Z8000 standalone adapter. Filesystem operations are original V7 SYS.c. */
#include <sys/param.h>
#include <sys/inode.h>
#include "saio.h"

struct devsw devsw[] = { { "hd", 0, 0, 0 }, { 0, 0, 0, 0 } };
static char buf[512];
#ifdef Z8002_MMU
#define FPUBASE 0x8000
#define FPULIMIT 0xc000
#define DATALIMIT 0x8000
#else
#define FPUBASE 0
#define FPULIMIT 0xe000
#define DATALIMIT 0xe000
#endif

putchar(c) { if (c == '\n') outb(0xf0, '\r'); outb(0xf0, c); }
getchar() { while (!(inb(0xf2)&2)); return(inb(0xf0)&127); }
devopen(io) struct iob *io; {
 if (io->i_unit || io->i_boff) _stop("Use hd(0,0)");
}
devclose() {}
devwrite() { _stop("Read-only bootstrap"); }
devread(io) struct iob *io; {
 unsigned n, s, *p;
 long bn;
 bn = io->i_bn;
 if (bn < 0 || bn > 0xfffffffL || io->i_cc != 512) _stop("Bad disk request");
 outb(0x1f3,(int)bn); outb(0x1f4,(int)(bn>>8)); outb(0x1f5,(int)(bn>>16));
 outb(0x1f6,0xe0|(int)(bn>>24)); outb(0x1f2,1); outb(0x1f7,0x20);
 n=65535;
 do {
  s=inb(0x1f7);
  if (s&1) _stop("Disk error");
  if (!(s&128) && (s&8)) break;
 } while (--n);
 if (!n) _stop("Disk timeout");
 p=(unsigned *)io->i_ma;
 for(n=0;n<256;n++) *p++=inw(0x1f0);
 return(512);
}

static load(fd,seg,off,count)
unsigned seg,off,count;
{
 unsigned n;
 while(count) {
  n=count>512?512:count;
  if(read(fd,buf,n)!=n) _stop("Short executable");
  copyseg(seg,off,buf,n); off+=n; count-=n;
 }
}

main()
{
 int fd, n;
 unsigned h[20], off;
 char kernel[80], fpu[20];
#ifdef Z8002_MMU
 printf("V7 Z8002 boot\n");
#else
 printf("V7 Z8001 boot\n");
#endif
 for (;;) {
  printf(": ");
  n=0;
  for (;;) {
   int c;
   c=getchar();
   if(c=='\r'||c=='\n') break;
   if(c==8||c==127) {
    if(n) { n--; putchar(8); putchar(' '); putchar(8); }
    continue;
   }
   if(n<79) { kernel[n++]=c; putchar(c); }
  }
  putchar('\n'); kernel[n]=0;
  if(!n) strcpy(kernel,"hd(0,0)/unix");
  fd=open(kernel,0);
  if(fd<0) continue;
  if(read(fd,(char *)h,40)!=40 || h[0]!=0xe711 || h[5]!=16 ||
     h[9]!=1 || h[7] || h[8]!=0x1f0 || h[14]<512 ||
     h[15]>DATALIMIT || h[16]>DATALIMIT-h[15] || h[12] || h[13] ||
     h[17]!=7 || h[18] || h[19] || h[3] || h[4]!=h[16] ||
     ((long)h[1]<<16)+h[2]!=(long)h[14]+h[15]) {
   printf("Bad kernel header\n"); close(fd); continue;
  }
  load(fd,0x8200,0,h[14]); load(fd,0x8100,0,h[15]);
  lseek(fd,40L,0);
  if(read(fd,buf,512)!=512) _stop("Missing vectors");
#ifdef Z8002_MMU
  copyseg(0x8100,0xd000,buf,512);
#else
  copyseg(0x8000,0x1000,buf,512);
#endif
  close(fd);
  for(n=0;n<512;n++) buf[n]=0;
  off=h[15];
  while(h[16]) {
   n=h[16]>512?512:h[16]; copyseg(0x8100,off,buf,n); off+=n; h[16]-=n;
  }
  strcpy(fpu,"hd(0,0)/fpe"); fd=open(fpu,0);
  if(fd<0) _stop("Missing /fpe");
  off=FPUBASE;
  while((n=read(fd,buf,512))>0) {
   if(off>FPULIMIT-n) _stop("FPE too large");
   copyseg(0xff00,off,buf,n); off+=n;
  }
  if(off==FPUBASE) _stop("Empty /fpe");
  close(fd);
  printf("Starting Unix\n"); enter();
 }
}
