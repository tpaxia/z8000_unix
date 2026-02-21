#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"

/*
 * V7 signal handling for Z8000.
 *
 * Simplified from V7: no ptrace, no core dumps.
 * Signal delivery causes process exit (for now);
 * the shell uses signal() to set handlers but the kernel
 * doesn't actually call user signal handlers yet.
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
 * Returns true if the current process has a signal to process.
 * This is asked at least once each time a process enters the
 * system.
 */
issig()
{
	register struct proc *p;
	register n;
	register unsigned cursig;

	p = u.u_procp;
	while (p->p_sig) {
		n = p->p_sig;
		/* Find lowest set bit */
		for (cursig = 1; cursig < NSIG; cursig++) {
			if (n & 1)
				break;
			n >>= 1;
		}
		if (cursig >= NSIG)
			break;
		if ((u.u_signal[cursig] & 1) == 0 || u.u_signal[cursig] == 0)
			return(cursig);
		/* Signal is being ignored (disposition == 1) */
		p->p_sig &= ~(1 << (cursig - 1));
	}
	return(0);
}

/*
 * Perform the action specified by the current signal.
 * For now, all caught signals just terminate the process
 * (no user-mode signal handler delivery yet).
 */
psig()
{
	register n;
	register struct proc *p;

	p = u.u_procp;
	n = p->p_sig;
	/* Find lowest set bit */
	{
		register cursig;
		for (cursig = 1; cursig < NSIG; cursig++) {
			if (n & (1 << (cursig - 1))) {
				p->p_sig &= ~(1 << (cursig - 1));
				n = cursig;
				break;
			}
		}
	}
	if (n <= 0 || n >= NSIG)
		return;
	if (u.u_signal[n] != 0 && (u.u_signal[n] & 1) == 0) {
		/*
		 * User has a handler set. For now, just reset to default
		 * and ignore (shell sets handlers but we can't call them
		 * in user mode yet without a proper signal trampoline).
		 */
		u.u_signal[n] = 0;
		return;
	}
	/* Default action: terminate */
	if (n != SIGKIL)
		if (u.u_signal[n] != 0) {
			/* Ignored */
			return;
		}
	/* Kill the process */
	u.u_arg[0] = (n << 8);
	exit(u.u_arg[0]);
}
