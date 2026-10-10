#include "../../../h/param.h"
#include "../../../h/systm.h"
#include "../../../h/buf.h"
#include "../../../h/filsys.h"
#include "../../../h/mount.h"
#include "../../mmu/paged/mmu.h"
#include "../../../h/crash.h"

struct kcontext kcrash;
unsigned ksave[2];       /* assembly panic veneer scratch, never the record */

/* Called by panic's C body after the assembly veneer masks interrupts and
 * saves the caller registers. Preserve the first record across recursive panic.
 */
panicctx()
{
 if(!kcrash.upage)kcrash.upage=(unsigned)inw(MM_UPAGE);
}

crashtrap(regs,extra,fault)
unsigned *regs,*extra,*fault;
{
 int i;
 if(kcrash.kind)return;
 for(i=0;i<13;i++)kcrash.regs[i]=regs[i];
 kcrash.regs[13]=extra[0];
 kcrash.regs[14]=(regs[14]&0x8000)?extra[2]:extra[1];
 kcrash.regs[15]=(unsigned)(regs+17);
 for(i=0;i<3;i++)kcrash.regs[16+i]=regs[14+i];
 kcrash.tag=regs[13];kcrash.trap_pc=regs[16];
 kcrash.stackseg=extra[2];kcrash.upage=(unsigned)inw(MM_UPAGE);
 for(i=0;i<6;i++)kcrash.fault[i]=fault[i];
 if((fault[0]&MF_VALID) && fault[4]==((regs[15]>>8)&127))
  kcrash.regs[18]=fault[5];
 kcrash.magic=KC_MAGIC;kcrash.version=KC_VERSION;
 kcrash.kind=KC_FAULT;
}

/* Reserved tail of root ATA unit: commit sector, raw RAM, raw swap.
 * Never touch filesystem blocks or overwrite swap to save the dump.
 * Clear the old commit first, and write the new commit last.
 */
static daddr_t dumplo;
static unsigned dumpram, dumpswap;
static char dumpbuf[512];
extern int physmem;

dumpinit()
{
 long capacity, need;
 if(rootdev!=makedev(1,0) || !mount[0].m_bufp)return;
 capacity=(unsigned)inw(MM_DISKSIZE);
 dumplo=mount[0].m_bufp->b_un.b_filsys->s_fsize;
 dumpram=physmem*4;dumpswap=(unsigned)inw(MM_SWAPSIZE);
 need=1L+dumpram+dumpswap;
 if(dumplo<2 || dumplo+need>capacity) {dumplo=0;return;}
 printf("Crash dump: hd0 block %D, %D sectors\n",dumplo,need);
}

/* Disk metadata has explicit byte order, independent of host structures. */
dput(p,n)
char *p;
long n;
{p[0]=n>>24;p[1]=n>>16;p[2]=n>>8;p[3]=n;}

panicdump()
{
 long i;
 int r;
 if(!dumplo)return;
 bzero(dumpbuf,512);
 r=hddump(rootdev,dumplo,dumpbuf,1);if(r!=1)goto bad;
 for(i=0;i<dumpram;i++) {
  if(dumpcopy(i*512L,dumpbuf)<0) {r=0;goto bad;}
  r=hddump(rootdev,dumplo+1+i,dumpbuf,1);if(r!=1)goto bad;
 }
 for(i=0;i<dumpswap;i++) {
  r=hddump(swapdev,i,dumpbuf,0);if(r!=1)goto bad;
  r=hddump(rootdev,dumplo+1+dumpram+i,dumpbuf,1);if(r!=1)goto bad;
 }
 bzero(dumpbuf,512);
 dput(dumpbuf,0x5a384b44L);dput(dumpbuf+4,1L);
 dput(dumpbuf+8,(long)dumpram*512L);
 dput(dumpbuf+12,(long)dumpswap*512L);dput(dumpbuf+16,time);
 r=hddump(rootdev,dumplo,dumpbuf,1);if(r!=1)goto bad;
 printf("panic dump: %u RAM, %u swap sectors saved\n",dumpram,dumpswap);
 return;
bad:
 printf("panic dump: %s; incomplete\n",r<0?"timeout":"I/O error");
}
