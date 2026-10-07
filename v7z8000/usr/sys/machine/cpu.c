#include "../h/param.h"
#include "../h/systm.h"
#include "../h/mount.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/inode.h"
#include "../h/buf.h"


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
 * icode[] - Z8002 machine code for process 1's initial program.
 *
 * Executes:
 *   exec("/etc/init", 0)  -- sc #11
 *
 * Layout:
 *   0x00: ld R1, #0x000C    (filename pointer)
 *   0x04: ld R2, #0         (argv = NULL)
 *   0x08: sc #11            (exec syscall)
 *   0x0A: halt              (should not reach)
 *   0x0C: "/etc/init\0"
 */
int icode[] = {
	0x2101, 0x000C,		/* ld R1, #0x000C    */
	0x2102, 0x0000,		/* ld R2, #0         */
	0x7F0B,			/* sc #11            */
	0x7A00,			/* halt              */
	/* "/etc/init\0" at offset 0x0C */
	0x2F65, 0x7463, 0x2F69, 0x6E69, 0x7400
};
int szicode = sizeof(icode);

/* Initial NONSEG stack: strings below fff0, then argc/argv/NULL/envp/NULL.
 * Return the required size in V7 clicks, or zero if data and stack collide.
 * Keep the startup headroom and alignment in the same CPU layer as execstk.
 */
execsize(nc, na, ne, data)
long data;
{
	long bytes;
	unsigned clicks;

	bytes = (long)nc + (na+3)*2L;
	if (data + bytes + 256 > 0xFFF0L)
		return(0);
	clicks = (bytes+17+256+63)/64;
	if (clicks < SSIZE) clicks = SSIZE;
	return(clicks);
}

/* Arguments are already validated and staged by shared exec policy. Commit
 * the process-local SP only after every user copy succeeds. File close/iput
 * can still sleep; trap return installs the hardware USP after that work.
 */
execstk(bno, nc, na, ne)
{
	unsigned sp, ucp, ap;
	int c, limit;
	char *cp;
	struct buf *bp;

	ucp = (0xFFF0-nc)&~1;
	sp = ucp - (na+3)*2;
	ap = sp;
	bp = NULL;
	limit = nc;
	if (suword(ap, na-ne) < 0) goto bad;
	nc = 0;
	/* V7 copy-back loop, with checked user stores and Z8000 stack top. */
	for (;;) {
		ap += NBPW;
		if (na==ne) {
			if (suword(ap, 0) < 0) goto bad;
			ap += NBPW;
		}
		if (--na < 0) break;
		if (suword(ap, ucp) < 0) goto bad;
		do {
			if (nc >= limit) goto bad;
			if ((nc&BMASK) == 0) {
				if (bp) brelse(bp);
				bp = bread(swapdev, swplo+bno+(nc>>BSHIFT));
				if (bp->b_flags&B_ERROR) goto bad;
				cp = bp->b_un.b_addr;
			}
			c = *cp++;
			if (subyte(ucp++, c) < 0) goto bad;
			nc++;
		} while (c&0377);
	}
	if (suword(ap, 0) < 0) goto bad;
	if (bp) brelse(bp);
	u.u_regs[15] = sp;
	return(0);
bad:
	if (bp) brelse(bp);
	return(-1);
}

/* Reset the Z8000 register/EPU image. R13/R14 live outside the common trap
 * frame and are restored from u_regs by the CPU entry wrappers.
 */
execregs()
{
	extern int useg;
	int i;

	for (i = 0; i < 13; i++) u.u_ar0[i] = 0;
	u.u_regs[13] = u.u_regs[14] = 0;
	u.u_ar0[15] = useg;
	u.u_ar0[16] = u.u_exdata.ux_entloc;
	u.u_fpflag = 0;
	bzero(u.u_fpe, sizeof(u.u_fpe));
	u.u_fpe[93] = 1;
}

/* Build the libc signal trampoline's frame: signo, FCW, R0, PC, then 96
 * persistent EPU bytes. Reserve another 32 bytes for register saves/call.
 * Only user FLAGS/PC are restored by libc, never privileged FCW fields.
 * Leave the saved registers and process-local SP untouched on failure.
 */
sendsig(handler, sig, usp)
unsigned handler, *usp;
{
	unsigned sp;

	if ((*usp & 1) || *usp < 136 ||
	    *usp-136 < (unsigned)ctob(u.u_dsize) || !grow(*usp-136))
		return(-1);
	sp = *usp-104;
	if (suword(sp, sig) < 0 || suword(sp+2, u.u_ar0[14]) < 0 ||
	    suword(sp+4, u.u_ar0[0]) < 0 || suword(sp+6, u.u_ar0[16]) < 0 ||
	    copyout(u.u_fpe, sp+8, 96) < 0)
		return(-1);
	u.u_ar0[16] = handler;
	*usp = sp;
	return(0);
}

/* Clock frame decoding and processor predicates stay below shared policy. */
clktick(regs)
unsigned *regs;
{
	clock(regs[16], regs[14]);
}
usermode(ps) { return((ps&0x4000) == 0); }
basepri(ps) { return((ps&0x1800) != 0x1800); }
idlepc(pc)
unsigned pc;
{
	extern char waitloc[];
	return(pc == (unsigned)waitloc);
}

/* V7 addupc scaling: halve unsigned delta/scale, multiply, shift 14,
 * round up to an even byte offset. Sample only a complete user-data word.
 * A bad mapping disables profiling; it must never fault or sleep in clock.
 */
addupc(pc, p, ticks)
unsigned pc;
register struct prof {
	unsigned base, size, off, scale;
} *p;
{
	unsigned index, addr, value;
	long product;
	char error;

	if (p->scale <= 1) return;
	product = (long)((unsigned)(pc-p->off)>>1) * (p->scale>>1);
	index = ((unsigned)(product>>14)+1)&~1;
	if (p->size < 2 || index > p->size-2) return;
	addr = p->base+index;
	error = u.u_error;
	if ((addr&1) || addr < p->base || addr == 65535 ||
	    copyin(addr, &value, 2) < 0) goto bad;
	value += ticks;
	if (copyout(&value, addr, 2) < 0) goto bad;
	u.u_error = error;
	return;
bad:
	p->scale = 0;
	u.u_error = error;
}
