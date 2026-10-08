/* Z8001 standalone adapter. Filesystem operations are original V7 SYS.c. */
#include <sys/param.h>
#include <sys/inode.h>
#include "saio.h"

struct devsw devsw[] = { { "hd", 0, 0, 0 }, { 0, 0, 0, 0 } };
static char buf[512];

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
 if (bn < 0 || bn > 65535L || io->i_cc != 512) _stop("Bad disk request");
 outb(0x1f3,(int)bn); outb(0x1f4,(int)(bn>>8)); outb(0x1f5,0);
 outb(0x1f6,0xe0); outb(0x1f2,1); outb(0x1f7,0x20);
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
 unsigned h[8], off;
 char kernel[80], fpu[20];
 printf("V7 Z8001 boot\n");
 for (;;) {
  printf(": ");
  n=0;
  for (;;) {
   int c;
   c=getchar();
   if(c=='\r'||c=='\n') break;
   if(c==8||c==127) { if(n) n--; continue; }
   if(n<79) { kernel[n++]=c; putchar(c); }
  }
  putchar('\n'); kernel[n]=0;
  if(!n) strcpy(kernel,"hd(0,0)/unix");
  fd=open(kernel,0);
  if(fd<0) continue;
  if(read(fd,(char *)h,16)!=16 || h[0]!=0411 || h[1]<512 ||
     h[2]>0xe000 || h[3]>0xe000-h[2] || h[5]!=0x1f0) {
   printf("Bad kernel header\n"); close(fd); continue;
  }
  load(fd,0x8200,0,h[1]); load(fd,0x8100,0,h[2]);
  lseek(fd,16L,0);
  if(read(fd,buf,512)!=512) _stop("Missing vectors");
  copyseg(0x8000,0x1000,buf,512);
  close(fd);
  for(n=0;n<512;n++) buf[n]=0;
  off=h[2];
  while(h[3]) {
   n=h[3]>512?512:h[3]; copyseg(0x8100,off,buf,n); off+=n; h[3]-=n;
  }
  strcpy(fpu,"hd(0,0)/fpe"); fd=open(fpu,0);
  if(fd<0) _stop("Missing /fpe");
  off=0;
  while((n=read(fd,buf,512))>0) {
   if(off>0xe000-n) _stop("FPE too large");
   copyseg(0xff00,off,buf,n); off+=n;
  }
  if(!off) _stop("Empty /fpe");
  close(fd);
  printf("Starting Unix\n"); enter();
 }
}
