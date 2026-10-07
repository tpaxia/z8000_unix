#include "../h/param.h"
#include "../h/systm.h"
#include "../h/tty.h"
#include "../h/buf.h"
#include "../h/conf.h"

/*
 * Device switch tables for Z8000 bring-up.
 *
 * Block devices:
 *   Major 0: RAM disk (md)
 *   Major 1: IDE hard drive (hd)
 *
 * Character devices:
 *   Major 0: Console (cons)
 *   Major 1: console alias (spare)
 *   Major 2: TTY (alias to console)
 *   Major 3: Raw IDE disk (same minors as block major 1)
 *   Major 4: Memory special file (only minor 2, /dev/null)
 */

/* RAM disk driver */
extern int mdopen(), mdclose(), mdstrategy();
extern struct buf mdtab;

/* IDE hard drive driver */
extern int hdopen(), hdclose(), hdstrategy();
extern int hdrawopen(), hdread(), hdwrite();
extern struct buf hdtab;

/* Console driver */
extern int consopen(), consclose(), consread(), conswrite(), consioctl();
extern struct tty cons_tty[];

/* V7 EOF/rathole device; other memory minors are unavailable. */
extern int mmopen(), mmread(), mmwrite();

/* Stubs */
extern int nodev(), nulldev();

struct bdevsw bdevsw[] = {
	{ mdopen, mdclose, mdstrategy, &mdtab },	/* 0 = md */
	{ hdopen, hdclose, hdstrategy, &hdtab },	/* 1 = hd */
	{ 0 }
};

struct cdevsw cdevsw[] = {
	{ consopen, consclose, consread, conswrite, consioctl, nulldev, &cons_tty[0] },  /* 0 = console */
	{ consopen, consclose, consread, conswrite, consioctl, nulldev, &cons_tty[0] },  /* 1 = spare */
	{ consopen, consclose, consread, conswrite, consioctl, nulldev, &cons_tty[0] },  /* 2 = tty */
	{ hdrawopen, hdclose, hdread, hdwrite, nodev, nulldev, 0 }, /* 3 = raw hd */
	{ mmopen, nulldev, mmread, mmwrite, nodev, nulldev, 0 }, /* 4 = memory */
	{ 0 }
};

/* Discipline zero is ordinary V7 tty processing; no alternate is installed. */
extern int ttyopen(), ttread(), ttyinput(), ttstart();
extern char *ttwrite();
struct linesw linesw[] = {
	{ ttyopen, nulldev, ttread, ttwrite, nodev, ttyinput, ttstart,
	  nulldev, ttstart, nulldev }
};
int nldisp = sizeof(linesw) / sizeof(linesw[0]);

/* Boot devices and interrupt wiring for the emulated machine. */
devinit()
{
	rootdev = makedev(1, 0);
	pipedev = rootdev;
	swapdev = makedev(1, 1);
	nswap = inw(0x00b2);
	swplo = 0;
}

devintr(vector)
{
	hdintr();
	consrint();
}

clkstart()
{
	spl0();
}

/* Used for early kernel output as well as the console TTY driver. */
putchar(c)
{
	outb(0x00F0, c);
}
