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
 * Sticky text retains its inode and swap image after the last reference.
 * If backing storage is unavailable, release the unused image instead.
 * Text is immutable except for exclusive ptrace writes, which invalidate its
 * swap copy and prohibit fresh exec sharing until the last reference exits.
 */
struct text text[NTEXT];
#define XPAGES(xp) (((xp)->x_size+31)/32)

struct text *
textget(ip, bytes, imageoff)
struct inode *ip;
unsigned bytes;
long imageoff;
{
	register struct text *xp, *freep;
	register struct file *fp;
	unsigned off, n;
	char buf[512];
	freep = NULL;
	for (xp = text; xp < &text[NTEXT]; xp++) {
		if (xp->x_iptr == ip) {
			if (xp->x_flag&XTRC) { u.u_error = ETXTBSY; return(NULL); }
			xlock(xp);
			if (textin(xp) < 0) { xunlock(xp); return(NULL); }
			xp->x_count++; xp->x_ccount++;
			xunlock(xp);
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
	xp->x_iptr = ip;
	xp->x_flag = XLOAD|XLOCK;
	xp->x_count = xp->x_ccount = 1;
	xp->x_daddr = 0;
	ip->i_count++;
	ip->i_flag |= ITEXT;
	/* Publish ownership before corealloc can sleep in a concurrent exec. */
	xp->x_caddr = corealloc(XPAGES(xp));
	if (!xp->x_caddr) {
		u.u_error = ENOMEM;
		xunlock(xp); textput(xp); return(NULL);
	}
	copyframes(0, xp->x_caddr, XPAGES(xp));
	for (off = 0; off < bytes; off += n) {
		n = bytes-off; if (n > sizeof(buf)) n = sizeof(buf);
		u.u_base = buf; u.u_count = n;
		u.u_offset = (long)off+imageoff;
		u.u_segflg = 1;
		readi(ip);
		if (u.u_error || u.u_count) {
			if (!u.u_error) u.u_error = EIO;
			xunlock(xp); textput(xp); return(NULL);
		}
		physcopy(xp->x_caddr, off, buf, n, 1);
	}
	xp->x_flag &= ~XLOAD;
	xunlock(xp);
	return(xp);
}

/* Caller holds XLOCK across allocation, I/O and resident-count updates. */
textin(xp)
register struct text *xp;
{
	unsigned frame;
	if (!xp || xp->x_caddr) return(0);
	frame = corealloc(XPAGES(xp));
	if (!frame) goto fail;
	if (swapio(xp->x_daddr, frame, XPAGES(xp), 1) < 0) {
		mfree(coremap, XPAGES(xp), frame); goto fail;
	}
	xp->x_caddr = frame;
	return(0);
fail:
	u.u_error = ENOMEM;
	return(-1);
}

textout(xp)
register struct text *xp;
{
	unsigned block;
	if (!xp) return(0);
	/* Caller holds XLOCK; the departing resident cannot run during I/O. */
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
	if (!xp) return;
	xlock(xp);
	xp->x_ccount--;
	if (--xp->x_count) {
		if (!xp->x_ccount) textout(xp);
		xunlock(xp);
		return;
	}
	/* V7 retains unused sticky text on swap, not in physical memory. */
	if ((xp->x_iptr->i_mode&ISVTX) && !(xp->x_flag&(XLOAD|XTRC)) &&
	    textout(xp) == 0) {
		xunlock(xp);
		return;
	}
	xunlock(xp);
	xuntext(xp);
}

xfree()
{
	struct text *xp;
	xp = u.u_procp->p_textp;
	u.u_procp->p_textp = NULL;
	textput(xp);
}

/* V7 text locking and cache lookup policy. */
xlock(xp)
register struct text *xp;
{

	while(xp->x_flag&XLOCK) {
		xp->x_flag |= XWANT;
		sleep((caddr_t)xp, PSWP);
	}
	xp->x_flag |= XLOCK;
}

xunlock(xp)
register struct text *xp;
{

	if (xp->x_flag&XWANT)
		wakeup((caddr_t)xp);
	xp->x_flag &= ~(XLOCK|XWANT);
}

xumount(dev)
register dev;
{
	register struct text *xp;

	for (xp = &text[0]; xp < &text[NTEXT]; xp++)
		if (xp->x_iptr!=NULL && dev==xp->x_iptr->i_dev)
			xuntext(xp);
}

xrele(ip)
register struct inode *ip;
{
	register struct text *xp;

	if ((ip->i_flag&ITEXT)==0)
		return;
	for (xp = &text[0]; xp < &text[NTEXT]; xp++)
		if (ip==xp->x_iptr)
			xuntext(xp);
}

/* V7 release policy; physical/swap units remain those of the selected MMU. */
xuntext(xp)
register struct text *xp;
{
	register struct inode *ip;

	xlock(xp);
	if (xp->x_count || xp->x_iptr == NULL) {
		xunlock(xp);
		return;
	}
	ip = xp->x_iptr;
	xunlock(xp);
	xp->x_iptr = NULL;
	if (xp->x_caddr) mfree(coremap, XPAGES(xp), xp->x_caddr);
	if (xp->x_daddr) mfree(swapmap, XPAGES(xp)*4, xp->x_daddr);
	xp->x_caddr = xp->x_daddr = 0;
	ip->i_flag &= ~ITEXT;
	if (ip->i_flag&ILOCK)
		ip->i_count--;
	else {
		plock(ip);
		iput(ip);
	}
}
