#include "../h/param.h"
#include "../h/systm.h"
#include "../h/buf.h"
#include "../h/conf.h"
#include "../h/dir.h"
#include "../h/user.h"

/*
 * RAM disk driver for Z8000.
 *
 * Uses I/O port-based DMA to transfer blocks between
 * the disk image (held by the emulator) and kernel buffers.
 *
 * DMA controller ports:
 *   0xE0 (W): block number high byte
 *   0xE1 (W): block number low byte
 *   0xE2 (W): DMA address high byte (kernel buffer offset)
 *   0xE3 (W): DMA address low byte
 *   0xE4 (W): command (1=read block->mem, 2=write mem->block)
 *   0xE5 (R): status (0=ok, 0xFF=error)
 */

extern int outb();
extern int inb();

struct buf mdtab;

mdopen(dev, rw)
dev_t dev;
{
}

mdclose(dev, rw)
dev_t dev;
{
}

mdstrategy(bp)
register struct buf *bp;
{
	register int blkno;
	register int addr;
	int cmd;
	int status;

	blkno = bp->b_blkno;
	addr = (int)bp->b_un.b_addr;

	/* Set block number */
	outb(0xE0, (blkno >> 8) & 0xFF);
	outb(0xE1, blkno & 0xFF);

	/* Set DMA address (kernel buffer) */
	outb(0xE2, (addr >> 8) & 0xFF);
	outb(0xE3, addr & 0xFF);

	/* Issue command: 1=read, 2=write */
	if (bp->b_flags & B_READ)
		cmd = 1;
	else
		cmd = 2;
	outb(0xE4, cmd);

	/* Check status */
	status = inb(0xE5);
	if (status != 0) {
		bp->b_flags |= B_ERROR;
		bp->b_error = EIO;
	}

	iodone(bp);
}
