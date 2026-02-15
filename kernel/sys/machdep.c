#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/inode.h"
#include "../h/conf.h"

/*
 * Machine-dependent stubs for Z8000 single-process bring-up.
 */

extern int idle();

/*
 * sleep: should never be called during single-process bring-up
 * with synchronous RAM disk. If it is, something is wrong.
 */
sleep(chan, pri)
caddr_t chan;
{
	printf("sleep(%x, %d)\n", chan, pri);
	panic("sleep");
}

/*
 * wakeup: no-op (no other processes to wake)
 */
wakeup(chan)
caddr_t chan;
{
}

/*
 * SPL functions: no-ops (no interrupts)
 */
spl0() { return(0); }
spl1() { return(0); }
spl4() { return(0); }
spl5() { return(0); }
spl6() { return(0); }
spl7() { return(0); }
splx(s) { return(0); }

/*
 * plock/prele: inode locking (flag-based, no sleep)
 */
plock(ip)
struct inode *ip;
{
	ip->i_flag |= 01;	/* ILOCK */
}

prele(ip)
struct inode *ip;
{
	ip->i_flag &= ~01;	/* ~ILOCK */
}

/*
 * cinit: count character device switch entries.
 * Sets nchrdev to the number of entries in cdevsw[].
 */
cinit()
{
	register struct cdevsw *cdp;

	for (cdp = cdevsw; cdp->d_open; cdp++)
		nchrdev++;
}

/*
 * Stubs for functions not needed yet.
 */
startup() {}
clkstart() {}
xrele(ip) struct inode *ip; {}

/*
 * bzero: zero count bytes starting at addr.
 */
bzero(addr, count)
char *addr;
int count;
{
	register char *p;
	register int n;

	p = addr;
	n = count;
	while (n--)
		*p++ = 0;
}

/*
 * fubyte/subyte/copyin/copyout: should never be called.
 * Everything runs in kernel space.
 */
fubyte(addr) char *addr; { panic("fubyte"); return(-1); }
subyte(addr, val) char *addr; { panic("subyte"); return(-1); }
fuibyte(addr) char *addr; { panic("fuibyte"); return(-1); }
suibyte(addr, val) char *addr; { panic("suibyte"); return(-1); }
copyin(from, to, n) { panic("copyin"); return(-1); }
copyout(from, to, n) { panic("copyout"); return(-1); }
copyiin(from, to, n) { panic("copyiin"); return(-1); }
copyiout(from, to, n) { panic("copyiout"); return(-1); }
