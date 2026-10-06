#include "../h/param.h"
#include "../h/systm.h"
#include "../h/map.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/text.h"
#include "../h/inode.h"
#include "../h/file.h"

/* V7 inode-backed text ownership, with machine-dependent physical transfers.
 * No idle text cache: the last reference releases the inode and both stores.
 * Text is immutable; a swap copy can be reused until the last reference exits.
 */
struct text text[NTEXT];
#define XPAGES(xp) (((xp)->x_size+31)/32)

struct text *
textget(ip, bytes)
struct inode *ip;
unsigned bytes;
{
	register struct text *xp, *freep;
	register struct file *fp;
	unsigned off, n;
	char buf[512];
	freep = NULL;
	for (xp = text; xp < &text[NTEXT]; xp++) {
		if (xp->x_iptr == ip) {
			if (textin(xp) < 0) return(NULL);
			xp->x_count++; xp->x_ccount++;
			return(xp);
		}
		if (!xp->x_iptr && !freep) freep = xp;
	}
	/* V7 forbids establishing pure text while an existing writer has it. */
	for (fp = file; fp < &file[NFILE]; fp++)
		if (fp->f_count && fp->f_inode == ip && (fp->f_flag&FWRITE)) {
			u.u_error = ETXTBSY; return(NULL);
		}
	if (!(xp = freep)) { u.u_error = ENOMEM; return(NULL); }
	xp->x_size = ((long)bytes+63)/64;
	xp->x_caddr = corealloc(XPAGES(xp));
	if (!xp->x_caddr) { u.u_error = ENOMEM; return(NULL); }
	xp->x_iptr = ip;
	xp->x_flag = XLOCK;
	xp->x_count = xp->x_ccount = 1;
	xp->x_daddr = 0;
	ip->i_count++;
	ip->i_flag |= ITEXT;
	copyframes(0, xp->x_caddr, XPAGES(xp));
	for (off = 0; off < bytes; off += n) {
		n = bytes-off; if (n > sizeof(buf)) n = sizeof(buf);
		u.u_base = buf; u.u_count = n;
		u.u_offset = (long)off+sizeof(u.u_exdata);
		u.u_segflg = 1;
		readi(ip);
		if (u.u_error || u.u_count) {
			if (!u.u_error) u.u_error = EIO;
			xp->x_flag = 0; textput(xp); return(NULL);
		}
		physcopy(xp->x_caddr, off, buf, n, 1);
	}
	xp->x_flag = 0;
	return(xp);
}

/* The caller holds a process lock or the prototype inode lock. Swap I/O
 * enables interrupts but never schedules, so these transitions cannot race.
 */
textin(xp)
register struct text *xp;
{
	unsigned frame;
	if (!xp || xp->x_caddr) return(0);
	xp->x_flag |= XLOCK;
	frame = corealloc(XPAGES(xp));
	if (!frame) goto fail;
	if (swapio(xp->x_daddr, frame, XPAGES(xp), 1) < 0) {
		mfree(coremap, XPAGES(xp), frame); goto fail;
	}
	xp->x_caddr = frame;
	xp->x_flag &= ~XLOCK;
	return(0);
fail:
	xp->x_flag &= ~XLOCK;
	u.u_error = ENOMEM;
	return(-1);
}

textout(xp)
register struct text *xp;
{
	unsigned block;
	if (!xp) return(0);
	if (xp->x_flag & XLOCK) return(-1);
	if (xp->x_ccount > 1) { xp->x_ccount--; return(0); }
	if (!xp->x_daddr) {
		block = malloc(swapmap, XPAGES(xp)*4);
		if (!block) return(-1);
		if (swapio(block, xp->x_caddr, XPAGES(xp), 0) < 0) {
			mfree(swapmap, XPAGES(xp)*4, block); return(-1);
		}
		xp->x_daddr = block;
	}
	mfree(coremap, XPAGES(xp), xp->x_caddr);
	xp->x_caddr = 0;
	xp->x_ccount = 0;
	return(0);
}

textput(xp)
register struct text *xp;
{
	register struct inode *ip;
	if (!xp) return;
	xp->x_ccount--;
	if (--xp->x_count) {
		if (!xp->x_ccount) textout(xp);
		return;
	}
	if (xp->x_caddr) mfree(coremap, XPAGES(xp), xp->x_caddr);
	if (xp->x_daddr) mfree(swapmap, XPAGES(xp)*4, xp->x_daddr);
	ip = xp->x_iptr;
	xp->x_iptr = NULL;
	xp->x_caddr = xp->x_daddr = 0;
	ip->i_flag &= ~ITEXT;
	if (ip->i_flag & ILOCK) ip->i_count--;
	else { plock(ip); iput(ip); }
}

xfree()
{
	struct text *xp;
	xp = u.u_procp->p_textp;
	u.u_procp->p_textp = NULL;
	textput(xp);
}

/* No unreferenced cached texts: active references retain ITEXT and the inode. */
xrele(ip)
struct inode *ip;
{}
xumount(dev)
{}
