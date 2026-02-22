#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/file.h"
#include "../h/inode.h"

/*
 * Process management for Z8000 kernel.
 * V7-style with MMU (KDSA6 equivalent via paged MMU).
 * No bcopy of u-areas, no per-process kernel stacks.
 * resume() writes KDSA6 to remap the u-area + kernel stack.
 */

extern int useg;
extern int outw();

/* Identity frame for copy window (seg1 pages 28-29) */
#define WPAGE_IDENTITY	60

#define SQSIZE 0100		/* Must be power of 2 */
#define HASH(x)	(( (int) x >> 5) & (SQSIZE-1))
struct proc *slpque[SQSIZE];

/*
 * Give up the processor till a wakeup occurs
 * on chan, at which time the process enters the
 * scheduling queue at priority pri.
 */
sleep(chan, pri)
caddr_t chan;
{
	register struct proc *rp;
	register s, h;

	rp = u.u_procp;
	s = spl6();
	if (chan==0)
		panic("zero wchan");
	rp->p_stat = SSLEEP;
	rp->p_wchan = chan;
	rp->p_pri = pri;
	h = HASH(chan);
	rp->p_link = slpque[h];
	slpque[h] = rp;
	if(pri > PZERO) {
		if(issig()) {
			rp->p_wchan = 0;
			rp->p_stat = SRUN;
			slpque[h] = rp->p_link;
			spl0();
			goto psig;
		}
		spl0();
		swtch();
		if(issig())
			goto psig;
	} else {
		spl0();
		swtch();
	}
	splx(s);
	return;

psig:
	resume(u.u_procp->p_addr, u.u_qsav);
}

/*
 * Wake up all processes sleeping on chan.
 */
wakeup(chan)
register caddr_t chan;
{
	register struct proc *p, *q;
	register i;
	int s;

	s = spl6();
	i = HASH(chan);
	p = slpque[i];
	q = NULL;
	while(p != NULL) {
		if(p->p_wchan==chan && p->p_stat!=SZOMB) {
			struct proc *sp;

			if (q == NULL)
				sp = slpque[i] = p->p_link;
			else
				sp = q->p_link = p->p_link;
			p->p_wchan = 0;
			setrun(p);
			p = sp;
			continue;
		}
		q = p;
		p = p->p_link;
	}
	splx(s);
}

/*
 * Put process on run queue.
 */
setrq(p)
struct proc *p;
{
	register struct proc *q;
	register s;

	s = spl6();
	for(q=runq; q!=NULL; q=q->p_link)
		if(q == p) {
			printf("proc on q\n");
			goto out;
		}
	p->p_link = runq;
	runq = p;
out:
	splx(s);
}

/*
 * Set the process running;
 * arrange for it to be swapped in if necessary.
 */
setrun(p)
register struct proc *p;
{
	if (p->p_stat==0 || p->p_stat==SZOMB)
		panic("Running a dead proc");
	p->p_stat = SRUN;
	p->p_flag |= SLOAD;	/* always in-core on Z8000 */
	setrq(p);
	if(p->p_pri < curpri)
		runrun++;
}

/*
 * Set user priority.
 */
setpri(pp)
register struct proc *pp;
{
	register p;

	p = (pp->p_cpu & 0377)/16;
	p += PUSER + pp->p_nice - NZERO;
	if(p > 127)
		p = 127;
	if(p < curpri)
		runrun++;
	pp->p_pri = p;
	return(p);
}

/*
 * Switch to the highest-priority runnable process.
 * V7 style: resume() writes KDSA6 to remap u-area + kernel stack.
 * No bcopy of u-areas needed.
 */
swtch()
{
	register n;
	register struct proc *p, *q, *pp, *pq;

	/*
	 * If not the idle process, save state and resume proc[0].
	 * resume() writes KDSA6 to remap u-area before reading u_qsav.
	 */
	if (u.u_procp != &proc[0]) {
		if (save(u.u_rsav)) {
			sureg();
			return;
		}
		resume(proc[0].p_addr, u.u_qsav);
	}
	/*
	 * The first save returns nonzero when proc 0 is resumed
	 * by another process (above); then the second is not done
	 * and the process-search loop is entered.
	 */
	if (save(u.u_qsav)==0 && save(u.u_rsav))
		return;
loop:
	spl6();
	runrun = 0;
	pp = NULL;
	q = NULL;
	n = 128;
	/*
	 * Search for highest-priority runnable process
	 */
	for(p=runq; p!=NULL; p=p->p_link) {
		if((p->p_stat==SRUN) && (p->p_flag&SLOAD)) {
			if(p->p_pri < n) {
				pp = p;
				pq = q;
				n = p->p_pri;
			}
		}
		q = p;
	}
	/*
	 * If no process is runnable, idle.
	 */
	p = pp;
	if(p == NULL) {
		idle();
		goto loop;
	}
	q = pq;
	if(q == NULL)
		runq = p->p_link;
	else
		q->p_link = p->p_link;
	curpri = n;
	spl0();
	n = p->p_flag&SSWAP;
	p->p_flag &= ~SSWAP;
	resume(p->p_addr, n? u.u_ssav: u.u_rsav);
}

/*
 * Create a new process -- the internal version of sys fork.
 * Returns 1 in the new process, 0 in the old.
 *
 * V7 style: allocates u-area frames, copies u-area via MMU copy window.
 * No per-process kernel stacks. No trampoline.
 */
newproc()
{
	struct proc *p, *up;
	register struct proc *rpp, *rip;
	register n;

	p = NULL;
	/*
	 * First, just locate a slot for a process
	 * and copy the useful info from this process into it.
	 */
retry:
	mpid++;
	if(mpid >= 30000) {
		mpid = 0;
		goto retry;
	}
	for(rpp = &proc[0]; rpp < &proc[NPROC]; rpp++) {
		if(rpp->p_stat == NULL && p==NULL)
			p = rpp;
		if (rpp->p_pid==mpid || rpp->p_pgrp==mpid)
			goto retry;
	}
	if ((rpp = p)==NULL)
		panic("no procs");

	/*
	 * make proc entry for new proc
	 */
	rip = u.u_procp;
	up = rip;
	rpp->p_stat = SRUN;
	rpp->p_clktim = 0;
	rpp->p_flag = SLOAD;
	rpp->p_uid = rip->p_uid;
	rpp->p_pgrp = rip->p_pgrp;
	rpp->p_nice = rip->p_nice;
	rpp->p_textp = NULL;		/* no text sharing */
	rpp->p_pid = mpid;
	rpp->p_ppid = rip->p_pid;
	rpp->p_time = 0;
	rpp->p_cpu = 0;

	/*
	 * Allocate u-area frames for child process.
	 */
	n = frame_alloc();
	if (n == 0)
		panic("no frames");
	rpp->p_addr = n;
	rpp->p_size = 1024;		/* 64KB = 1024 clicks of 64 bytes */

	/*
	 * Bump reference counts on open files, cdir, rdir.
	 */
	for(n=0; n<NOFILE; n++)
		if(u.u_ofile[n] != NULL)
			u.u_ofile[n]->f_count++;
	u.u_cdir->i_count++;
	if (u.u_rdir)
		u.u_rdir->i_count++;

	/*
	 * Partially simulate the environment
	 * of the new process so that when it is actually
	 * created (by copying) it will look right.
	 */
	rpp = p;
	u.u_procp = rpp;
	rip = up;

	/*
	 * When resumed, child returns here with save() returning 1.
	 */
	if (save(u.u_ssav)) {
		sureg();
		return(1);
	}

	/*
	 * Copy parent's u-area to child's physical frames via window.
	 * Interrupts must be disabled: BSS extends into the copy window
	 * region (0xE000-0xEFFF), so clock handlers would read garbage.
	 */
	{
		int s;
		s = spl7();
		outw(0x00B4, rpp->p_addr);		/* map window to child's frames */
		bcopy(0xF000, 0xE000, 4096);		/* copy u-area + kernel stack */
		outw(0x00B4, WPAGE_IDENTITY);		/* restore window identity map */
		splx(s);
	}

	/*
	 * Copy parent's user segment to child's segment.
	 * Segment number = proc index + 1 (proc[0]->seg 1, etc.)
	 */
	copyseg((rip - proc) + 1, (rpp - proc) + 1);

	u.u_procp = rip;
	setrq(rpp);
	rpp->p_flag |= SSWAP;
	return(0);
}

/*
 * put the current process on
 * the Q of running processes and
 * call the scheduler.
 */
qswtch()
{

	setrq(u.u_procp);
	swtch();
}
