/* Z8002 access through a privileged 2KB kernel-data aperture.
 * Validate the selected descriptor before touching the window. Keep IRQs
 * masked only across each bounded copy and restore the previous selector.
 */
#include "../../../../h/param.h"
#include "../mmu.h"
extern int useg, iseg;
static copy2(map, from, to, count, writing)
unsigned map, from, count;
char *to;
{
 unsigned n, select, frame, old;
 int s, v;
 char *window;
 if (from && count > -from) return(-1);
 while(count) {
  n=2048-(from&2047);
  if(n>count)n=count;
  if(n>16)n=16;
  select=((map>>8)&127)*32+(from>>11);
  s=spl7();
  outw(MM_PAGESEL,select);
  frame=inw(MM_PAGEFRAME);
  if(frame==65535 || (frame&MM_SYS) || (writing && (frame&MM_RO))) {splx(s);return(-1);}
  old=inw(MM_USERWIN);
  outw(MM_USERWIN,select);
  window=(char *)(0xe000+(from&2047));
  while(n--) {
   if(writing)v=winwrit(window,*to);else v=winread(window);
   if(v<0) {outw(MM_USERWIN,old);splx(s);return(-1);}
   if(!writing)*to=v;
   window++;from++;to++;count--;
  }
  outw(MM_USERWIN,old);
  splx(s);
 }
 return(0);
}
copyin(from,to,n) unsigned from,n; char *to; {return(copy2(useg,from,to,n,0));}
copyout(from,to,n) char *from; unsigned to,n; {return(copy2(useg,to,from,n,1));}
copyiin(from,to,n) unsigned from,n; char *to; {return(copy2(iseg,from,to,n,0));}
copyiout(from,to,n) char *from; unsigned to,n; {return(copy2(iseg,to,from,n,1));}
fubyte(a) unsigned a; {unsigned char c;if(copyin(a,&c,1)<0)return(-1);return(c);}
fuibyte(a) unsigned a; {unsigned char c;if(copyiin(a,&c,1)<0)return(-1);return(c);}
subyte(a,v) unsigned a; {char c;c=v;return(copyout(&c,a,1));}
suibyte(a,v) unsigned a; {char c;c=v;return(copyiout(&c,a,1));}
fuword(a) unsigned a; {int v;if((a&1)||copyin(a,&v,2)<0)return(-1);return(v);}
suword(a,v) unsigned a; int v; {if(a&1)return(-1);return(copyout(&v,a,2));}
