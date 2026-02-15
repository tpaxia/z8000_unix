#include "../h/param.h"
#include "../h/systm.h"
#include "../h/buf.h"
#include "../h/conf.h"
#include "../h/dir.h"
#include "../h/user.h"

/*
 * IDE hard drive driver for Z8000.
 *
 * Synchronous polling ATA PIO driver — no interrupts, no partitions.
 * Single drive, whole disk, LBA addressing.
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

hdwait()
{
	while (inb(HD_STATUS) & ST_BSY)
		;
}

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
	register int blkno;
	int status;

	blkno = bp->b_blkno;

	/* Wait for drive ready */
	hdwait();

	/* Set LBA address and sector count */
	outb(HD_SC, 1);
	outb(HD_SN, blkno & 0xFF);
	outb(HD_CL, (blkno >> 8) & 0xFF);
	outb(HD_CH, 0);
	outb(HD_DH, 0xE0);		/* LBA mode, drive 0 */

	if (bp->b_flags & B_READ) {
		outb(HD_CMD, CMD_READ);
		hdwait();
		insw(HD_DATA, bp->b_un.b_addr, 256);
	} else {
		outb(HD_CMD, CMD_WRITE);
		hdwait();
		outsw(HD_DATA, bp->b_un.b_addr, 256);
		hdwait();
	}

	/* Check for errors */
	status = inb(HD_STATUS);
	if (status & ST_ERR) {
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
	}

	iodone(bp);
}
