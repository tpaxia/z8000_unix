#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/inode.h"
#include "../h/file.h"
#include "../h/seg.h"

/*
 * System calls: fork, exit, wait, exec.
 * Simplified from V7 sys1.c -- no swapping, no SUID/SGID.
 */

/*
 * xproc overlay for zombie status collection.
 * In V7, the first part of struct proc is overlaid with
 * exit status and times when the process becomes a zombie.
 */
struct xproc {
	char	xp_stat;
	char	xp_flag;
	char	xp_pri;
	char	xp_time;
	char	xp_cpu;
	char	xp_nice;
	short	xp_sig;
	short	xp_uid;
	short	xp_pgrp;
	short	xp_pid;
	short	xp_ppid;
	short	xp_xstat;	/* overlays p_addr: exit status */
	short	xp_size;
	long	xp_utime;	/* overlays p_wchan + p_textp */
	long	xp_stime;	/* overlays p_link + p_clktim */
};

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
	/* xfree() and acct() are no-ops for Step 8 */
	xfree();
	acct();
	/*
	 * Free the u-area frames.
	 */
	frame_free(p->p_addr);
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
 * Simplified for Step 8: no swap space check, no MAXUPRC check.
 */
fork()
{
	register struct proc *p1, *p2;

	p2 = NULL;
	for(p1 = &proc[0]; p1 < &proc[NPROC]; p1++) {
		if (p1->p_stat==NULL && p2==NULL)
			p2 = p1;
	}
	if (p2==NULL) {
		u.u_error = EAGAIN;
		goto out;
	}
	p1 = u.u_procp;
	if(newproc()) {
		u.u_r.r_val1 = p1->p_pid;
		u.u_start = time;
		u.u_cstime = 0;
		u.u_stime = 0;
		u.u_cutime = 0;
		u.u_utime = 0;
		u.u_acflag = AFORK;
		return;
	}
	u.u_r.r_val1 = p2->p_pid;

out:
	;
}

/*
 * exec system call.
 * Load and execute a program from the filesystem.
 * Simplified from V7: no swap for argument collection,
 * no text sharing, no SUID/SGID.
 */
exec()
{
	register struct inode *ip;
	register unsigned i;
	extern int uchar();
	extern int useg;

	/*
	 * Look up executable.
	 */
	u.u_dirp = (caddr_t)u.u_arg[0];	/* filename from R1 */
	ip = namei(uchar, 0);
	if (ip == NULL)
		return;
	if (access(ip, IEXEC) ||
	    (ip->i_mode & IFMT) != IFREG ||
	    (ip->i_mode & (IEXEC|(IEXEC>>3)|(IEXEC>>6))) == 0) {
		u.u_error = EACCES;
		goto bad;
	}

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

	/*
	 * Validate magic number.
	 */
	if (u.u_exdata.ux_mag != 0407) {
		u.u_error = ENOEXEC;
		goto bad;
	}

	/*
	 * For 0407: merge text into data.
	 */
	u.u_exdata.ux_dsize += u.u_exdata.ux_tsize;
	u.u_exdata.ux_tsize = 0;

	/*
	 * Load program into user segment.
	 */
	u.u_base = 0;
	u.u_offset = sizeof(u.u_exdata);	/* skip header */
	u.u_count = u.u_exdata.ux_dsize;
	u.u_segflg = 0;		/* user space (copyout) */
	readi(ip);
	if (u.u_error)
		goto bad;

	/*
	 * Zero BSS in user segment.
	 */
	for (i = 0; i < u.u_exdata.ux_bsize; i++)
		subyte(u.u_exdata.ux_dsize + i, 0);

	/*
	 * Close EXCLOSE files, reset signals.
	 */
	setregs();

	/*
	 * Set return PC to entry point.
	 * regs[15] = PC high (segment encoding)
	 * regs[16] = PC low (offset)
	 */
	u.u_ar0[15] = useg;
	u.u_ar0[16] = u.u_exdata.ux_entloc;

	/*
	 * Clear user registers.
	 */
	for (i = 0; i < 13; i++)
		u.u_ar0[i] = 0;

	/*
	 * Set user stack pointer to top of segment.
	 */
	set_usp(0xFFF0);

	iput(ip);
	return;

bad:
	iput(ip);
}

/*
 * setregs - Reset signals and close EXCLOSE files after exec.
 */
setregs()
{
	register int i;

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

/*
 * read system call.
 * Sets up u-area fields and calls readi.
 */
read()
{
	register struct file *fp;
	register struct inode *ip;

	fp = getf(u.u_arg[0]);
	if (fp == NULL)
		return;
	if ((fp->f_flag & FREAD) == 0) {
		u.u_error = EBADF;
		return;
	}
	u.u_base = (caddr_t)u.u_arg[1];
	u.u_count = u.u_arg[2];
	u.u_offset = fp->f_un.f_offset;
	u.u_segflg = 0;
	ip = fp->f_inode;
	plock(ip);
	readi(ip);
	prele(ip);
	fp->f_un.f_offset += u.u_arg[2] - u.u_count;
	u.u_r.r_val1 = u.u_arg[2] - u.u_count;
}

/*
 * write system call.
 * Sets up u-area fields and calls writei.
 */
write()
{
	register struct file *fp;
	register struct inode *ip;

	fp = getf(u.u_arg[0]);		/* fd from R1 */
	if (fp == NULL)
		return;
	if ((fp->f_flag & FWRITE) == 0) {
		u.u_error = EBADF;
		return;
	}
	u.u_base = (caddr_t)u.u_arg[1];	/* buffer from R2 */
	u.u_count = u.u_arg[2];		/* count from R3 */
	u.u_offset = fp->f_un.f_offset;
	u.u_segflg = 0;			/* user space */
	ip = fp->f_inode;
	plock(ip);
	writei(ip);
	prele(ip);
	fp->f_un.f_offset += u.u_arg[2] - u.u_count;
	u.u_r.r_val1 = u.u_arg[2] - u.u_count;
}
