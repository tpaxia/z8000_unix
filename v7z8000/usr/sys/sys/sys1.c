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
 * read/write are now in sys2.c (via rdwr()).
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
 * r_val1 = child pid (in parent) or parent pid (in child)
 * r_val2 = 0 (in parent) or 1 (in child)
 * Libc fork wrapper uses r_val2 to distinguish parent from child.
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
 * no text sharing, no SUID/SGID.
 *
 * u_arg[0] = pathname (user pointer)
 * u_arg[1] = argv (user pointer to array of user pointers)
 * u_arg[2] = envp (user pointer to array of user pointers, or 0)
 */

static char argbuf[NCARGS];	/* kernel buffer for exec arguments */

exec()
{
	register struct inode *ip;
	register unsigned i;
	extern int uchar();
	extern int useg;
	char *cp;
	int nc;		/* total chars in argbuf */
	int na;		/* number of arg strings */
	int ne;		/* number of env strings */
	int ap;		/* user pointer to argv[] array */
	int c;
	unsigned usp;
	unsigned strbase;
	long datasize, filesize;
	int sep;

	/*
	 * Look up executable.
	 */
	u.u_dirp = (caddr_t)u.u_arg[0];
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
	 * Collect arguments from user space into argbuf[].
	 * Format in argbuf: NUL-separated strings.
	 */
	nc = 0;
	na = 0;
	ne = 0;
	cp = argbuf;

	/* Collect argv strings */
	ap = u.u_arg[1];	/* user pointer to argv[] */
	if (ap) {
		for (;;) {
			int sp;
			sp = fuword(ap);
			ap += 2;
			if (sp == 0 || sp == -1)
				break;
			na++;
			/* Copy string from user space */
			for (;;) {
				if (nc >= NCARGS) {
					u.u_error = E2BIG;
					goto bad;
				}
				c = fubyte(sp++);
				if (c == -1) {
					u.u_error = EFAULT;
					goto bad;
				}
				*cp++ = c;
				nc++;
				if (c == 0)
					break;
			}
		}
	}

	/* Collect envp strings */
	ap = u.u_arg[2];	/* user pointer to envp[] */
	if (ap) {
		for (;;) {
			int sp;
			sp = fuword(ap);
			ap += 2;
			if (sp == 0 || sp == -1)
				break;
			ne++;
			for (;;) {
				if (nc >= NCARGS) {
					u.u_error = E2BIG;
					goto bad;
				}
				c = fubyte(sp++);
				if (c == -1) {
					u.u_error = EFAULT;
					goto bad;
				}
				*cp++ = c;
				nc++;
				if (c == 0)
					break;
			}
		}
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

	/* Validate the entire layout before replacing the old image. */
	sep = u.u_exdata.ux_mag == 0411;
	datasize = (long)u.u_exdata.ux_dsize + u.u_exdata.ux_bsize;
	filesize = (long)sizeof(u.u_exdata) + u.u_exdata.ux_tsize + u.u_exdata.ux_dsize;
	if (!sep)
		datasize += u.u_exdata.ux_tsize;
	if (u.u_count || (!sep && u.u_exdata.ux_mag != 0407) ||
	    !u.u_exdata.ux_tsize || (u.u_exdata.ux_tsize & 1) ||
	    u.u_exdata.ux_entloc >= u.u_exdata.ux_tsize ||
	    (u.u_exdata.ux_entloc & 1) || u.u_exdata.ux_trsize || u.u_exdata.ux_drsize ||
	    filesize > ip->i_size || datasize + nc + (na+ne+3)*2L + 256 > 0xFFF0L) {
		u.u_error = ENOEXEC;
		goto bad;
	}

	u.u_sep = sep;
	sureg();
	u.u_offset = sizeof(u.u_exdata);
	if (sep) {
		/* I-space helpers address the backing bank through the data bus. */
		u.u_base = 0;
		u.u_count = u.u_exdata.ux_tsize;
		u.u_segflg = 2;
		readi(ip);
		if (u.u_error || u.u_count)
			goto badimage;
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
		subyte(u.u_exdata.ux_dsize + i, 0);

	/*
	 * Set up user stack with arguments.
	 *
	 * Stack layout (growing downward from top of segment):
	 *
	 *   string data (NUL-terminated arg and env strings)
	 *   [padding to word boundary]
	 *   0         (envp terminator)
	 *   envp[ne-1]
	 *   ...
	 *   envp[0]
	 *   0         (argv terminator)
	 *   argv[na-1]
	 *   ...
	 *   argv[0]
	 *   argc      <-- sp points here
	 */

	/* Start strings at top of segment, working down */
	usp = 0xFFF0;

	/* Copy strings to user stack, recording their user addresses */
	usp -= nc;
	/* Word-align */
	usp &= ~1;
	strbase = usp;
	for (i = 0; i < nc; i++)
		subyte(strbase + i, argbuf[i]);

	/* Now lay out pointers below the strings */
	/* Space needed: argc(2) + na ptrs(2*na) + NULL(2) + ne ptrs(2*ne) + NULL(2) */
	usp -= 2 + (na + 1) * 2 + (ne + 1) * 2;
	usp &= ~1;

	/* Write argc */
	suword(usp, na);

	/* Write argv[] pointers */
	cp = argbuf;
	for (i = 0; i < na; i++) {
		suword(usp + 2 + i * 2, strbase);
		/* Advance past this string */
		while (*cp++)
			strbase++;
		strbase++;	/* skip NUL */
	}
	/* argv terminator */
	suword(usp + 2 + na * 2, 0);

	/* Write envp[] pointers */
	for (i = 0; i < ne; i++) {
		suword(usp + 2 + (na + 1) * 2 + i * 2, strbase);
		while (*cp++)
			strbase++;
		strbase++;
	}
	/* envp terminator */
	suword(usp + 2 + (na + 1) * 2 + ne * 2, 0);

	/*
	 * Close EXCLOSE files, reset signals.
	 */
	setregs();

	/*
	 * Set up u-area for new program.
	 */
	u.u_tsize = (u.u_exdata.ux_tsize + 63L) >> 6;
	u.u_dsize = btoc(u.u_exdata.ux_dsize + u.u_exdata.ux_bsize);
	u.u_ssize = 1;		/* minimal stack */

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
	 * Set user stack pointer.
	 */
	set_usp(usp);

	iput(ip);
	return;

badimage:
	/* The old image has been overwritten; never return into it. */
	psignal(u.u_procp, SIGKIL);
	u.u_error = EIO;
bad:
	iput(ip);
}

/*
 * setregs - Reset signals and close EXCLOSE files after exec.
 */
setregs()
{
	register int i;
	u.u_fpflag = 0;
	bzero(u.u_fpe, sizeof(u.u_fpe));
	u.u_fpe[93] = 1;

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
