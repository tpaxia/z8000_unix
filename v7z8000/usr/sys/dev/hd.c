#include "../h/param.h"
#include "../h/systm.h"
#include "../h/buf.h"
#include "../h/conf.h"
#include "../h/dir.h"
#include "../h/user.h"

/*
 * IDE hard drive driver for Z8000.
 *
 * Interrupt-driven ATA PIO driver — VI interrupt on command completion.
 * Two drives, whole disk, LBA addressing.
 *
 * hdstrategy() queues the request and starts the controller if idle.
 * The emulator asserts VI when the operation completes.
 * hdintr() (called from the VI handler) performs the data transfer
 * for successful reads, checks status, calls iodone(), and starts the next
 * queued request.
 *
 * ATA register interface (emulated at standard x86 addresses):
 *   0x1F0 (R/W): DATA — 16-bit data transfer (word I/O)
 *   0x1F1 (R):   ERROR
 *   0x1F2 (W):   SC — sector count
 *   0x1F3 (W):   SN — LBA[0:7]
 *   0x1F4 (W):   CL — LBA[8:15]
 *   0x1F5 (W):   CH — LBA[16:23]
 *   0x1F6 (W):   DH — LBA[24:27] + LBA flag + drive
 *   0x1F7 (R/W): STATUS (read) / CMD (write)
 *
 * Status bits: BSY=0x80, DRDY=0x40, DRQ=0x08, ERR=0x01
 * Commands: READ SECTORS=0x20, WRITE SECTORS=0x30
 */

#define HD_DATA		0x01F0
#define HD_ERROR	0x01F1
#define HD_SC		0x01F2
#define HD_SN		0x01F3
#define HD_CL		0x01F4
#define HD_CH		0x01F5
#define HD_DH		0x01F6
#define HD_STATUS	0x01F7
#define HD_CMD		0x01F7

#define ST_BSY		0x80
#define ST_DRDY		0x40
#define ST_DRQ		0x08
#define ST_ERR		0x01

#define CMD_READ	0x20
#define CMD_WRITE	0x30

extern int inb(), outb();
extern int insw(), outsw();

struct buf hdtab;
/* Busy buffers use av_forw as the pending-request queue link. */

hdopen(dev, rw)
dev_t dev;
{
}

hdclose(dev, rw)
dev_t dev;
{
}

hdstrategy(bp)
register struct buf *bp;
{
	int s;

	s = spl6();
	bp->av_forw = 0;
	if (hdtab.b_actf)
		hdtab.b_actl->av_forw = bp;
	else
		hdtab.b_actf = bp;
	hdtab.b_actl = bp;
	if (!hdtab.b_active)
		hdstart();
	splx(s);
}

/* Called with device interrupts masked; only the head owns the controller. */
hdstart()
{
	register struct buf *bp;
	register int blkno;

	bp = hdtab.b_actf;
	if (bp == 0)
		return;
	hdtab.b_active = 1;
	blkno = bp->b_blkno;
	outb(HD_SC, 1);
	outb(HD_SN, blkno & 0xFF);
	outb(HD_CL, (blkno >> 8) & 0xFF);
	outb(HD_CH, 0);
	outb(HD_DH, 0xE0 | ((minor(bp->b_dev)&1)<<4));
	if (bp->b_flags & B_READ)
		outb(HD_CMD, CMD_READ);
	else {
		outb(HD_CMD, CMD_WRITE);
		outsw(HD_DATA, bp->b_un.b_addr, 256);
	}
}

/*
 * hdintr() — called from VI interrupt handler.
 * Completes the I/O: reads data for read commands,
 * checks status, and calls iodone().
 */
hdintr()
{
	register struct buf *bp;
	int status;

	bp = hdtab.b_actf;
	if (bp == 0 || !hdtab.b_active)
		return;
	status = inb(HD_STATUS);
	if (status & ST_BSY)
		return;
	if (status & ST_ERR) {
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
	} else {
		if ((bp->b_flags & B_READ) && !(status & ST_DRQ))
			return;
		if (bp->b_flags & B_READ)
			insw(HD_DATA, bp->b_un.b_addr, 256);
		bp->b_resid = 0;
	}
	/* Unlink before iodone(): asynchronous completion releases av_forw. */
	hdtab.b_actf = bp->av_forw;
	if (hdtab.b_actf == 0)
		hdtab.b_actl = 0;
	hdtab.b_active = 0;
	iodone(bp);
	hdstart();
}
