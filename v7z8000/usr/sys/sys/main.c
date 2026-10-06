#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/filsys.h"
#include "../h/mount.h"
#include "../h/proc.h"
#include "../h/inode.h"
#include "../h/seg.h"
#include "../h/conf.h"
#include "../h/buf.h"
#include "../h/file.h"

/*
 * V7 kernel main() for Z8000.
 *
 * V7 style with paged MMU: process 0 mounts root, opens /dev/console,
 * prints "Z8000 Unix", then forks process 1 which copies icode to
 * user space and enters user mode.
 */

extern int schar();
extern int open1();

struct proc proc[NPROC];

main()
{
	register struct inode *ip;
	register struct file *fp;
	register int i;

	/*
	 * set up system process
	 */
	proc[0].p_stat = SRUN;
	proc[0].p_flag = SLOAD|SSYS;
	proc[0].p_nice = NZERO;
	mmuinit();		/* establish process 0 machine mapping */
	u.u_procp = &proc[0];
	u.u_cmask = CMASK;
	u.u_uid = 0;
	u.u_gid = 0;
	u.u_ruid = 0;
	u.u_rgid = 0;
	u.u_error = 0;
	u.u_rdir = NULL;

	/* Root, pipe and swap devices belong to the machine configuration. */
	devinit();

	printf("boot\n");
	clkstart();
	cinit();
	binit();
	iinit();
	rootdir = iget(rootdev, (ino_t)ROOTINO);
	rootdir->i_flag &= ~ILOCK;
	u.u_cdir = iget(rootdev, (ino_t)ROOTINO);
	u.u_cdir->i_flag &= ~ILOCK;

	/*
	 * Open /dev/console as fd 0.
	 * namei walks root -> dev -> console via bread/bmap/iget.
	 * open1 calls falloc -> openi -> cdevsw[0].d_open.
	 */
	u.u_dirp = "/dev/console";
	ip = namei(schar, 0);
	if (ip == NULL)
		panic("console");
	open1(ip, FREAD|FWRITE, 0);
	if (u.u_error)
		panic("console open");

	/*
	 * Dup fd 0 to fd 1 and fd 2 (stdout, stderr).
	 */
	fp = u.u_ofile[0];
	u.u_ofile[1] = fp;
	fp->f_count++;
	u.u_ofile[2] = fp;
	fp->f_count++;

	printf("Z8000 Unix\n");

	/*
	 * Fork process 1.
	 * V7 style: newproc() returns 1 in child, 0 in parent.
	 * Child copies icode to user segment and enters user mode.
	 */
	if (newproc()) {
		/* Child (process 1) */
		estabur(0, btoc(szicode), 0, 0, 0);	/* no-op */
		copyout((caddr_t)icode, (caddr_t)0, szicode);
		return;		/* returns to krt.s boot_entry -> retu() -> user mode */
	}

	/*
	 * Parent (process 0): become idle/scheduler.
	 * swtch() will find proc[1] on the run queue and switch to it.
	 */
	swtch();
}

/* open1 is now provided by sys2.c */

/*
 * iinit is called once (from main)
 * very early in initialization.
 * It reads the root's super block
 * and initializes the current date
 * from the last modified date.
 */
iinit()
{
	register struct buf *cp, *bp;
	register struct filsys *fp;

	(*bdevsw[major(rootdev)].d_open)(rootdev, 1);
	bp = bread(rootdev, SUPERB);
	cp = geteblk();
	if(u.u_error)
		panic("iinit");
	bcopy(bp->b_un.b_addr, cp->b_un.b_addr, sizeof(struct filsys));
	brelse(bp);
	mount[0].m_bufp = cp;
	mount[0].m_dev = rootdev;
	fp = cp->b_un.b_filsys;
	fp->s_flock = 0;
	fp->s_ilock = 0;
	fp->s_ronly = 0;
	time = fp->s_time;
}

/*
 * This is the set of buffers proper, whose heads
 * were declared in buf.h.
 */
char	buffers[NBUF][BSIZE+BSLOP];

/*
 * Initialize the buffer I/O system by freeing
 * all buffers and setting all device buffer lists to empty.
 */
binit()
{
	register struct buf *bp;
	register struct buf *dp;
	register int i;
	struct bdevsw *bdp;

	bfreelist.b_forw = bfreelist.b_back =
	    bfreelist.av_forw = bfreelist.av_back = &bfreelist;
	for (i=0; i<NBUF; i++) {
		bp = &buf[i];
		bp->b_dev = NODEV;
		bp->b_un.b_addr = buffers[i];
		bp->b_back = &bfreelist;
		bp->b_forw = bfreelist.b_forw;
		bfreelist.b_forw->b_back = bp;
		bfreelist.b_forw = bp;
		bp->b_flags = B_BUSY;
		brelse(bp);
	}
	for (bdp = bdevsw; bdp->d_open; bdp++) {
		dp = bdp->d_tab;
		if(dp) {
			dp->b_forw = dp;
			dp->b_back = dp;
		}
		nblkdev++;
	}
}

struct buf buf[NBUF];
struct buf bfreelist;
struct inode inode[NINODE];
struct file file[NFILE];
