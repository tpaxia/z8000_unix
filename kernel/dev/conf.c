#include "../h/param.h"
#include "../h/systm.h"
#include "../h/buf.h"
#include "../h/conf.h"

/*
 * Device switch tables for Z8000 bring-up.
 *
 * Block devices:
 *   Major 0: RAM disk (md)
 *
 * Character devices:
 *   Major 0: Console (cons)
 *   Major 1: (unused, null-terminated)
 *   Major 2: TTY (alias to console)
 */

/* RAM disk driver */
extern int mdopen(), mdclose(), mdstrategy();
extern struct buf mdtab;

/* Console driver */
extern int consopen(), consclose(), consread(), conswrite();

/* Stubs */
extern int nodev(), nulldev();

struct bdevsw bdevsw[] = {
	{ mdopen, mdclose, mdstrategy, &mdtab },	/* 0 = md */
	{ 0 }
};

struct cdevsw cdevsw[] = {
	{ consopen, consclose, consread, conswrite, nodev, nulldev, 0 },  /* 0 = console */
	{ consopen, consclose, consread, conswrite, nodev, nulldev, 0 },  /* 1 = spare */
	{ consopen, consclose, consread, conswrite, nodev, nulldev, 0 },  /* 2 = tty */
	{ 0 }
};

struct linesw linesw[] = {
	{ 0 }
};
