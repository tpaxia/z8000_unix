#include "../h/param.h"
#include "../h/systm.h"
#include "../h/map.h"
#include "../h/buf.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/inode.h"
#include "../h/file.h"
#include "../h/seg.h"
#include "../h/text.h"
#include "../h/acct.h"

/*
 * System calls: fork, exit, wait, exec.
 * read/write are now in sys2.c (via rdwr()).
 */

/*
 * exit system call:
 * pass back caller's arg
 */
rexit()
{
	u.u_arg[0] = (u.u_arg[0] & 0377) << 8;
	exit(u.u_arg[0]);
}

/*
 * Release resources.
 * Save u. area for parent to look at.
 * Enter zombie state.
 * Wake up parent and init processes,
 * and dispose of children.
 */
exit(rv)
{
	register int i;
	register struct proc *p, *q;
	register struct file *f;

	p = u.u_procp;
	p->p_flag &= ~(STRC|SULOCK);
	p->p_clktim = 0;
	for(i=0; i<NSIG; i++)
		u.u_signal[i] = 1;
	for(i=0; i<NOFILE; i++) {
		f = u.u_ofile[i];
		u.u_ofile[i] = NULL;
		closef(f);
	}
	plock(u.u_cdir);
	iput(u.u_cdir);
	if (u.u_rdir) {
		plock(u.u_rdir);
		iput(u.u_rdir);
	}
	xfree();
	acct();
	/*
	 * Free the user banks and u-area frames.
	 */
	freemem(p);
	p->p_stat = SZOMB;
	((struct xproc *)p)->xp_xstat = rv;
	((struct xproc *)p)->xp_utime = u.u_cutime + u.u_utime;
	((struct xproc *)p)->xp_stime = u.u_cstime + u.u_stime;
	for(q = &proc[0]; q < &proc[NPROC]; q++)
		if(q->p_ppid == p->p_pid) {
			wakeup((caddr_t)&proc[1]);
			q->p_ppid = 1;
			if (q->p_stat==SSTOP)
				setrun(q);
		}
	for(q = &proc[0]; q < &proc[NPROC]; q++)
		if(p->p_ppid == q->p_pid) {
			wakeup((caddr_t)q);
			swtch();
			/* no return */
		}
	swtch();
}

/*
 * Wait system call.
 * Search for a terminated (zombie) child,
 * finally lay it to rest, and collect its status.
 */
wait1()
{
	register f;
	register struct proc *p;

	f = 0;

loop:
	for(p = &proc[0]; p < &proc[NPROC]; p++)
	if(p->p_ppid == u.u_procp->p_pid) {
		f++;
		if(p->p_stat == SZOMB) {
			u.u_r.r_val1 = p->p_pid;
			u.u_r.r_val2 = ((struct xproc *)p)->xp_xstat;
			u.u_cutime += ((struct xproc *)p)->xp_utime;
			u.u_cstime += ((struct xproc *)p)->xp_stime;
			p->p_pid = 0;
			p->p_ppid = 0;
			p->p_pgrp = 0;
			p->p_sig = 0;
			p->p_flag = 0;
			p->p_wchan = 0;
			p->p_stat = NULL;
			return;
		}
		if(p->p_stat == SSTOP) {
			if((p->p_flag&SWTED) == 0) {
				p->p_flag |= SWTED;
				u.u_r.r_val1 = p->p_pid;
				u.u_r.r_val2 = (fsig(p)<<8) | 0177;
				return;
			}
			continue;
		}
	}
	if(f) {
		sleep((caddr_t)u.u_procp, PWAIT);
		goto loop;
	}
	u.u_error = ECHILD;
}

/*
 * fork system call.
 *
 * r_val1 = child pid (in parent) or parent pid (in child)
 * r_val2 = 0 (in parent) or 1 (in child)
 * Libc fork wrapper uses r_val2 to distinguish parent from child.
 */
fork()
{
	register struct proc *p1, *p2;
	int n, a;

	a = 0;
	p2 = NULL;
	for(p1 = &proc[0]; p1 < &proc[NPROC]; p1++) {
		if (p1->p_stat==NULL && p2==NULL)
			p2 = p1;
		else {
			if (p1->p_uid==u.u_uid && p1->p_stat!=NULL)
				a++;
		}
	}
	/* V7 per-user admission and the final slot reserved for root. */
	if (p2==NULL || (u.u_uid!=0 && (p2==&proc[NPROC-1] || a>MAXUPRC))) {
		u.u_error = EAGAIN;
		goto out;
	}
	p1 = u.u_procp;
	n = newproc();
	if(n < 0) {
		u.u_error = EAGAIN;
		goto out;
	}
	if(n) {
		/* child */
		u.u_r.r_val1 = p1->p_pid;
		u.u_r.r_val2 = 1;	/* child indicator */
		u.u_start = time;
		u.u_cstime = 0;
		u.u_stime = 0;
		u.u_cutime = 0;
		u.u_utime = 0;
		u.u_acflag = AFORK;
		return;
	}
	/* parent */
	u.u_r.r_val1 = p2->p_pid;
	u.u_r.r_val2 = 0;		/* parent indicator */

out:
	;
}

/*
 * exec system call.
 * Load and execute a program from the filesystem.
 * Handles argv and envp: collects argument and environment strings
 * from user space into a kernel buffer, loads the new binary,
 * then builds the user stack with argc/argv[]/envp[]/strings.
 *
 * Simplified from V7: no swap for argument collection,
 * shared 0411 text. V7 set-ID policy is retained.
 *
 * u_arg[0] = pathname (user pointer)
 * u_arg[1] = argv (user pointer to array of user pointers)
 * u_arg[2] = envp (user pointer to array of user pointers, or 0)
 */

struct execa {
	char *fname;
	char **argp;
	char **envp;
};

exec()
{
	u.u_arg[2] = 0;
	exece();
}

exece()
{
	register struct inode *ip;
	register unsigned i;
	extern int uchar();
	char *cp;
	int nc;		/* total argument and environment bytes */
	int na;		/* total argument and environment strings */
	int ne;		/* number of env strings */
	int ap;		/* user pointer to argv[] array */
	int c, bno;
	struct buf *bp;
	struct execa *uap;
	long datasize, filesize;
	int sep;
	unsigned stacksize;
	struct text *xp, *oldtext;
	extern struct text *textget();

	/*
	 * Look up executable.
	 */
	u.u_dirp = (caddr_t)u.u_arg[0];
	ip = namei(uchar, 0);
	if (ip == NULL)
		return;
	bno = 0;
	bp = 0;
	if (access(ip, IEXEC))
		goto bad;
	if ((ip->i_mode & IFMT) != IFREG ||
	    (ip->i_mode & (IEXEC|(IEXEC>>3)|(IEXEC>>6))) == 0) {
		u.u_error = EACCES;
		goto bad;
	}

	/*
	 * Collect arguments on "file" in swap space.
	 */
	na = 0;
	ne = 0;
	nc = 0;
	uap = (struct execa *)u.u_arg;
	if ((bno = malloc(swapmap,(NCARGS+BSIZE-1)/BSIZE)) == 0)
		panic("Out of swap");
	if (uap->argp) for (;;) {
		ap = NULL;
		if (uap->argp) {
			ap = fuword((caddr_t)uap->argp);
			uap->argp++;
		}
		if (ap==NULL && uap->envp) {
			uap->argp = NULL;
			if ((ap = fuword((caddr_t)uap->envp)) == NULL)
				break;
			uap->envp++;
			ne++;
		}
		if (ap==NULL)
			break;
		na++;
		if(ap == -1)
			u.u_error = EFAULT;
		do {
			if (nc >= NCARGS-1)
				u.u_error = E2BIG;
			if ((c = fubyte((caddr_t)ap++)) < 0)
				u.u_error = EFAULT;
			if (u.u_error)
				goto bad;
			if ((nc&BMASK) == 0) {
				if (bp)
					bawrite(bp);
				bp = getblk(swapdev, swplo+bno+(nc>>BSHIFT));
				cp = bp->b_un.b_addr;
			}
			nc++;
			*cp++ = c;
		} while (c>0);
	}
	if (bp)
		bawrite(bp);
	bp = 0;
	nc = (nc + NBPW-1) & ~(NBPW-1);
	/*
	 * Read header into u.u_exdata.
	 */
	u.u_base = (caddr_t)&u.u_exdata;
	u.u_count = sizeof(u.u_exdata);
	u.u_offset = 0;
	u.u_segflg = 1;		/* kernel space */
	readi(ip);
	if (u.u_error)
		goto bad;

	/* Validate the entire layout before replacing the old image. */
	sep = u.u_exdata.ux_mag == 0411;
	datasize = (long)u.u_exdata.ux_dsize + u.u_exdata.ux_bsize;
	filesize = (long)sizeof(u.u_exdata) + u.u_exdata.ux_tsize + u.u_exdata.ux_dsize;
	if (!sep)
		datasize += u.u_exdata.ux_tsize;
	stacksize = execsize(nc, na, ne, datasize);
	if (u.u_count || (!sep && u.u_exdata.ux_mag != 0407) ||
	    !u.u_exdata.ux_tsize || (u.u_exdata.ux_tsize & 1) ||
	    u.u_exdata.ux_entloc >= u.u_exdata.ux_tsize ||
	    (u.u_exdata.ux_entloc & 1) || u.u_exdata.ux_trsize || u.u_exdata.ux_drsize ||
	    filesize > ip->i_size || stacksize == 0) {
		u.u_error = ENOEXEC;
		goto bad;
	}

	xp = NULL;
	u.u_procp->p_flag |= SLOCK;
	if (sep && !(xp = textget(ip, u.u_exdata.ux_tsize))) goto bad;
	oldtext = u.u_procp->p_textp;
	u.u_procp->p_textp = xp;
	if (estabur((unsigned)(sep ? ((long)u.u_exdata.ux_tsize+63)>>6 : 0),
	    (unsigned)((datasize+63)>>6),
	    stacksize, sep, 0) < 0) {
		u.u_procp->p_textp = oldtext;
		textput(xp);
		u.u_error = ENOMEM;
		goto bad;
	}
	textput(oldtext);
	u.u_offset = sizeof(u.u_exdata);
	if (sep) {
		u.u_offset += u.u_exdata.ux_tsize;
	} else {
		u.u_exdata.ux_dsize += u.u_exdata.ux_tsize;
		u.u_exdata.ux_tsize = 0;
	}
	u.u_base = 0;
	u.u_count = u.u_exdata.ux_dsize;
	u.u_segflg = 0;
	readi(ip);
	if (u.u_error || u.u_count)
		goto badimage;

	/*
	 * Zero BSS in user segment.
	 */
	for (i = 0; i < u.u_exdata.ux_bsize; i++)
		if (subyte(u.u_exdata.ux_dsize + i, 0) < 0)
			goto badimage;

	if (execstk(bno, nc, na, ne) < 0)
		goto badimage;

	/*
	 * set SUID/SGID protections, if no tracing
	 */
	if ((u.u_procp->p_flag&STRC)==0) {
		if(ip->i_mode&ISUID)
			if(u.u_uid != 0) {
				u.u_uid = ip->i_uid;
				u.u_procp->p_uid = ip->i_uid;
			}
		if(ip->i_mode&ISGID)
			u.u_gid = ip->i_gid;
	} else
		psignal(u.u_procp, SIGTRC);

	setregs();

	goto bad;

badimage:
	/* The old image has been overwritten; never return into it. */
	psignal(u.u_procp, SIGKIL);
	u.u_error = EIO;
bad:
	if (bp)
		brelse(bp);
	if (bno)
		mfree(swapmap, (NCARGS+BSIZE-1)/BSIZE, bno);
	u.u_procp->p_flag &= ~SLOCK;
	iput(ip);
}

/*
 * setregs - Reset signals and close EXCLOSE files after exec.
 */
setregs()
{
	register int i;
	execregs();
	u.u_prof.pr_scale = 0;

	for (i = 0; i < NSIG; i++)
		if ((u.u_signal[i] & 1) == 0)
			u.u_signal[i] = 0;

	for (i = 0; i < NOFILE; i++)
		if (u.u_pofile[i] & EXCLOSE) {
			closef(u.u_ofile[i]);
			u.u_ofile[i] = NULL;
			u.u_pofile[i] &= ~EXCLOSE;
		}

	u.u_acflag &= ~AFORK;
	bcopy((caddr_t)u.u_dbuf, (caddr_t)u.u_comm, DIRSIZ);
}
