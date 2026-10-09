#include "../h/param.h"
#include "../h/systm.h"
#include "../h/buf.h"
#include "../h/conf.h"
#include "../h/inode.h"
#include "../h/ino.h"
#include "../h/filsys.h"
#include "../h/mount.h"

/* Panic cannot sleep, allocate buffers, or enable interrupts. Drivers provide
 * bounded, non-sleeping completion polling through panicpoll(). A private
 * buffer permits metadata writes without waiting for a cache buffer owner.
 * Locked/busy objects are skipped: their state may be only partly updated.
 */
static struct buf pbuf;
static char pdata[BSIZE];
static int pwritten, pskipped, perrors;

pwait(bp)
register struct buf *bp;
{
	unsigned n;
	for (n = 0; n < 30000; n++) {
		if (bp->b_flags&B_DONE)
			return(1);
		panicpoll();
	}
	return(0);
}

pio(dev, block, read)
dev_t dev;
daddr_t block;
{
	pbuf.b_flags = B_BUSY | read;
	pbuf.b_dev = dev;
	pbuf.b_blkno = block;
	pbuf.b_bcount = BSIZE;
	pbuf.b_un.b_addr = pdata;
	pbuf.b_error = 0;
	(*bdevsw[major(dev)].d_strategy)(&pbuf);
	if (!pwait(&pbuf))
		return(-1);
	if (pbuf.b_flags&B_ERROR) {
		perrors++;
		return(0);
	}
	if (!read) pwritten++;
	return(1);
}

/* Copy a coherent cached block or read it by polling. Never wait for its
 * owner, nor read an older disk copy behind an unfinished cache operation. */
pread(dev, block)
dev_t dev;
daddr_t block;
{
	register struct buf *bp;
	for (bp = buf; bp < &buf[NBUF]; bp++)
		if (bp->b_dev == dev && bp->b_blkno == block) {
			if (bp->b_flags&(B_BUSY|B_ERROR)) {
				pskipped++;
				return(0);
			}
			if (bp->b_flags&(B_DONE|B_DELWRI)) {
				bcopy(bp->b_un.b_addr, pdata, BSIZE);
				return(1);
			}
		}
	return(pio(dev, block, B_READ));
}

panicflush()
{
	register struct buf *bp;
	register struct inode *ip;
	register struct mount *mp;
	struct filsys *fp;
	struct dinode *dp;
	char *src, *dst;
	int i, r;

	/* Finish queued requests first. Completion may release asynchronous
	 * buffers, but no process runs and synchronous owners stay busy. */
	for (i = 0; bdevsw[i].d_strategy; i++)
		if (bdevsw[i].d_tab->b_active) {
			unsigned n;
			for (n = 0; n < 30000 && bdevsw[i].d_tab->b_active; n++)
				panicpoll();
			if (bdevsw[i].d_tab->b_active) goto timeout;
		}
	for (bp = buf; bp < &buf[NBUF]; bp++) {
		if (!(bp->b_flags&B_DELWRI)) continue;
		if (bp->b_flags&B_BUSY) { pskipped++; continue; }
		notavail(bp);
		bp->b_flags &= ~(B_READ|B_DONE|B_ERROR|B_DELWRI|B_ASYNC);
		bp->b_bcount = BSIZE;
		(*bdevsw[major(bp->b_dev)].d_strategy)(bp);
		if (!pwait(bp)) goto timeout;
		if (bp->b_flags&B_ERROR) perrors++;
		else pwritten++;
		brelse(bp);
	}
	/* V7's inode serialization, with the port's big-endian disk addresses.
	 * Do not take inode locks or clear dirty flags until a write succeeds. */
	for (ip = inode; ip < &inode[NINODE]; ip++) {
		if (!ip->i_count || !(ip->i_flag&(IUPD|IACC|ICHG))) continue;
		if (ip->i_flag&ILOCK) { pskipped++; continue; }
		for (mp = mount; mp < &mount[NMOUNT]; mp++)
			if (mp->m_bufp && mp->m_dev == ip->i_dev) break;
		if (mp == &mount[NMOUNT] || mp->m_bufp->b_un.b_filsys->s_ronly)
			continue;
		r = pread(ip->i_dev, itod(ip->i_number));
		if (r < 0) goto timeout;
		if (!r) continue;
		dp = (struct dinode *)pdata + itoo(ip->i_number);
		dp->di_mode = ip->i_mode;
		dp->di_nlink = ip->i_nlink;
		dp->di_uid = ip->i_uid;
		dp->di_gid = ip->i_gid;
		dp->di_size = ip->i_size;
		src = (char *)ip->i_un.i_addr;
		dst = dp->di_addr;
		for (i = 0; i < NADDR; i++) {
			src++;
			*dst++ = *src++; *dst++ = *src++; *dst++ = *src++;
		}
		if (ip->i_flag&IACC) dp->di_atime = time;
		if (ip->i_flag&IUPD) dp->di_mtime = time;
		if (ip->i_flag&ICHG) dp->di_ctime = time;
		r = pio(ip->i_dev, itod(ip->i_number), B_WRITE);
		if (r < 0) goto timeout;
		if (r) {
			ip->i_flag &= ~(IUPD|IACC|ICHG);
			/* Later inodes in the same block must see this update. */
			for (bp = buf; bp < &buf[NBUF]; bp++)
				if (bp->b_dev == ip->i_dev &&
				    bp->b_blkno == itod(ip->i_number) &&
				    !(bp->b_flags&B_BUSY))
					bcopy(pdata, bp->b_un.b_addr, BSIZE);
		}
	}
	for (mp = mount; mp < &mount[NMOUNT]; mp++) {
		if (!mp->m_bufp) continue;
		fp = mp->m_bufp->b_un.b_filsys;
		if (!fp->s_fmod || fp->s_ronly) continue;
		if (fp->s_ilock || fp->s_flock) { pskipped++; continue; }
		bcopy((caddr_t)fp, pdata, BSIZE);
		((struct filsys *)pdata)->s_time = time;
		((struct filsys *)pdata)->s_fmod = 0;
		r = pio(mp->m_dev, (daddr_t)SUPERB, B_WRITE);
		if (r < 0) goto timeout;
	}
	printf("panic flush: %d writes, %d skipped, %d errors\n",
	    pwritten, pskipped, perrors);
	return;
timeout:
	printf("panic flush: timeout; %d writes, %d skipped, %d errors\n",
	    pwritten, pskipped, perrors);
}
