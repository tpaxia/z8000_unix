#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/buf.h"

/* V7 raw-I/O buffer ownership and completion policy. The selected MMU owns
 * validation, pinning and the buffer's machine-dependent address descriptor.
 * Drivers consume that descriptor through physio_copy(), including at interrupt
 * time while a different process is current. No buffer-cache entry is involved.
 */
physio(strat, bp, dev, rw)
int (*strat)();
register struct buf *bp;
dev_t dev;
{
	int s;
	unsigned done;
	if (!u.u_count) return;
	if (((unsigned)u.u_base & 1) || (u.u_count & 1)) {
		u.u_error = EFAULT;
		return;
	}
	s = spl6();
	while (bp->b_flags & B_BUSY) {
		bp->b_flags |= B_WANTED;
		sleep((caddr_t)bp, PRIBIO+1);
	}
	bp->b_flags = B_BUSY | B_PHYS | rw;
	bp->b_dev = dev;
	bp->b_blkno = u.u_offset >> BSHIFT;
	bp->b_bcount = bp->b_resid = u.u_count;
	bp->b_error = 0;
	if (physmap(bp, rw) < 0) goto release;
	(*strat)(bp);
	spl6();
	while (!(bp->b_flags & B_DONE)) sleep((caddr_t)bp, PRIBIO);
	physunmap(bp);
	/* A driver must leave a byte residual within the original request. */
	if (bp->b_resid > bp->b_bcount) {
		bp->b_resid = bp->b_bcount;
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
	}
	done = bp->b_bcount-bp->b_resid;
	u.u_count = bp->b_resid;
	u.u_base += done;
	u.u_offset += done;
	geterror(bp);
release:
	if (bp->b_flags & B_WANTED) wakeup((caddr_t)bp);
	bp->b_flags &= ~(B_BUSY|B_WANTED|B_PHYS);
	splx(s);
}
