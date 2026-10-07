#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/inode.h"

/*
 * Priority for tracing
 */
#define	IPCPRI	PZERO

/*
 * Tracing variables.
 * Used to pass trace command from
 * parent to child being traced.
 * This data base cannot be
 * shared and is locked
 * per user.
 */
struct
{
	int	ip_lock;
	int	ip_req;
	int	*ip_addr;
	int	ip_data;
} ipc;

/*
 * V7 signal handling for Z8000.
 *
 * V7 signal selection, stop/ptrace coordination and core-file policy.
 * Caught signals enter a libc trampoline on the user stack. The trampoline
 * preserves registers, calls the handler and restores flags/PC in user mode.
 */

/*
 * Send the specified signal to the specified process.
 */
psignal(p, sig)
register struct proc *p;
register sig;
{
	if (sig <= 0 || sig >= NSIG)
		return;
	p->p_sig |= 1 << (sig - 1);
	if (p->p_stat == SSLEEP && p->p_pri > PZERO) {
		setrun(p);
	}
}

/*
 * Send signal to process group.
 * Called by ttyinput() for SIGINT/SIGQUIT.
 */
signal(pgrp, sig)
{
	register struct proc *p;

	for (p = &proc[0]; p < &proc[NPROC]; p++)
		if (p->p_pgrp == pgrp)
			psignal(p, sig);
}

/*
 * Returns true if the current
 * process has a signal to process.
 * This is asked at least once
 * each time a process enters the
 * system.
 * A signal does not do anything
 * directly to a process; it sets
 * a flag that asks the process to
 * do something to itself.
 */
issig()
{
	register n;
	register struct proc *p;

	p = u.u_procp;
	while(p->p_sig) {
		n = fsig(p);
		if((u.u_signal[n]&1) == 0 || (p->p_flag&STRC))
			return(n);
		p->p_sig &= ~(1<<(n-1));
	}
	return(0);
}

/*
 * Enter the tracing STOP state.
 * In this state, the parent is
 * informed and the process is able to
 * receive commands from the parent.
 */
stop()
{
	register struct proc *pp, *cp;

loop:
	cp = u.u_procp;
	if(cp->p_ppid != 1)
	for (pp = &proc[0]; pp < &proc[NPROC]; pp++)
		if (pp->p_pid == cp->p_ppid) {
			wakeup((caddr_t)pp);
			cp->p_stat = SSTOP;
			swtch();
			if ((cp->p_flag&STRC)==0 || procxmt())
				return;
			goto loop;
		}
	exit(fsig(u.u_procp));
}

/*
 * Perform the action specified by the current signal.
 * Return the user SP, possibly lowered to hold a signal frame. The caller
 * keeps it on the kernel stack across process switches, not in NSPOFF.
 */
psig(usp)
unsigned usp;
{
	register n, handler;
	register struct proc *p;

	p = u.u_procp;
	if (p->p_flag&STRC) {
		coreregs(usp);
		stop();
		usp = u.u_regs[15];
	}
	n = fsig(p);
	if (n <= 0 || n >= NSIG)
		return(usp);
	p->p_sig &= ~(1<<(n-1));
	if ((handler = u.u_signal[n]) != 0 && (handler & 1) == 0) {
		if (sendsig(handler, n, &usp) < 0) {
			n = SIGSEG;
			goto die;
		}
		u.u_error = 0;
		if (n != SIGINS && n != SIGTRC)
			u.u_signal[n] = 0;
		return(usp);
	}
	/* Default action: terminate */
	if (n != SIGKIL)
		if (u.u_signal[n] != 0) {
			/* Ignored */
			return(usp);
		}
	/* Kill the process */
die:
	switch(n) {

	case SIGQUIT:
	case SIGINS:
	case SIGTRC:
	case SIGIOT:
	case SIGEMT:
	case SIGFPT:
	case SIGBUS:
	case SIGSEG:
	case SIGSYS:
		coreregs(usp);
		if(core())
			n += 0200;
	}
	exit(n);
}

/*
 * V7 core-file policy. The machine layer writes the u-area, data and stack
 * without the PDP-11's temporary contiguous mapping. Reject elevated group
 * credentials as well as user credentials before creating or truncating a file.
 */
core()
{
	register struct inode *ip;
	extern schar();

	u.u_error = 0;
	if(u.u_uid != u.u_ruid || u.u_gid != u.u_rgid)
		return(0);
	u.u_dirp = "core";
	ip = namei(schar, 1);
	if(ip == NULL) {
		if(u.u_error)
			return(0);
		ip = maknode(0666);
		if (ip==NULL)
			return(0);
	}
	if(!access(ip, IWRITE) && (ip->i_mode&IFMT) == IFREG) {
		itrunc(ip);
		coredump(ip);
	} else if(u.u_error == 0)
		u.u_error = EACCES;
	iput(ip);
	return(u.u_error==0);
}

/*
 * find the signal in bit-position
 * representation in p_sig.
 */
fsig(p)
struct proc *p;
{
	register n, i;

	n = p->p_sig;
	for(i=1; i<NSIG; i++) {
		if(n & 1)
			return(i);
		n >>= 1;
	}
	return(0);
}


/*
 * sys-trace system call.
 */
ptrace()
{
	register struct proc *p;
	register struct a {
		int	data;
		int	pid;
		int	*addr;
		int	req;
	} *uap;

	uap = (struct a *)u.u_ap;
	if (uap->req <= 0) {
		u.u_procp->p_flag |= STRC;
		return;
	}
	for (p=proc; p < &proc[NPROC]; p++)
		if (p->p_stat==SSTOP && (p->p_flag&STRC)
		 && p->p_pid==uap->pid
		 && p->p_ppid==u.u_procp->p_pid)
			goto found;
	u.u_error = ESRCH;
	return;

    found:
	while (ipc.ip_lock)
		sleep((caddr_t)&ipc, IPCPRI);
	ipc.ip_lock = p->p_pid;
	ipc.ip_data = uap->data;
	ipc.ip_addr = uap->addr;
	ipc.ip_req = uap->req;
	p->p_flag &= ~SWTED;
	setrun(p);
	while (ipc.ip_req > 0)
		sleep((caddr_t)&ipc, IPCPRI);
	u.u_r.r_val1 = ipc.ip_data;
	if (ipc.ip_req < 0)
		u.u_error = EIO;
	ipc.ip_lock = 0;
	wakeup((caddr_t)&ipc);
}

/* V7 child-side request dispatch. CPU/MMU hooks replace PDP-11 register
 * offsets and temporary writable text mappings. Helpers do not sleep.
 */
procxmt()
{
	register int i;

	if (ipc.ip_lock != u.u_procp->p_pid)
		return(0);
	i = ipc.ip_req;
	ipc.ip_req = 0;
	wakeup((caddr_t)&ipc);
	switch(i) {
	case 1:
	case 2:
	case 4:
	case 5:
		if (traceword(i, ipc.ip_addr, &ipc.ip_data) < 0) goto error;
		break;
	case 3:
	case 6:
		if (traceuser(i, ipc.ip_addr, &ipc.ip_data) < 0) goto error;
		break;
	case 9:
	case 7:
		if ((unsigned)ipc.ip_data >= NSIG ||
		    tracego(ipc.ip_addr, i == 9) < 0) goto error;
		u.u_procp->p_sig = 0;
		if (ipc.ip_data) psignal(u.u_procp, ipc.ip_data);
		return(1);
	case 8:
		exit(fsig(u.u_procp));
	default:
	error:
		ipc.ip_req = -1;
	}
	return(0);
}
