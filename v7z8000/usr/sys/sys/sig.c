#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"

/*
 * V7 signal handling for Z8000.
 *
 * Simplified from V7: no ptrace, no core dumps.
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
 * Return the user SP, possibly lowered to hold a signal frame. The caller
 * keeps it on the kernel stack across process switches, not in NSPOFF.
 */
psig(usp)
unsigned usp;
{
	register n, handler;
	register struct proc *p;
	unsigned sp;

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
		return(usp);
	if ((handler = u.u_signal[n]) != 0 && (handler & 1) == 0) {
		/*
		 * Frame: signo, saved FCW, saved R0, saved PC offset, then
		 * 96 bytes of EPU registers/control (scratch is not persistent).
		 * libc saves R1-R14 before calling the C handler. Returning needs
		 * only user FLAGS and PC; privileged FCW bits are never restored
		 * from user memory. Reserve room for the trampoline's saves/call.
		 */
		if ((usp & 1) || usp < 136 ||
		    usp - 136 < (unsigned)ctob(u.u_dsize)) {
			n = SIGSEG;
			goto die;
		}
		sp = usp - 104;
		if (suword(sp, n) < 0 || suword(sp + 2, u.u_ar0[14]) < 0 ||
		    suword(sp + 4, u.u_ar0[0]) < 0 ||
		    suword(sp + 6, u.u_ar0[16]) < 0 ||
		    copyout(u.u_fpe, sp + 8, 96) < 0) {
			n = SIGSEG;
			goto die;
		}
		u.u_ar0[16] = handler;
		u.u_error = 0;
		if (n != SIGINS && n != SIGTRC)
			u.u_signal[n] = 0;
		return(sp);
	}
	/* Default action: terminate */
	if (n != SIGKIL)
		if (u.u_signal[n] != 0) {
			/* Ignored */
			return(usp);
		}
	/* Kill the process */
die:
	exit(n);
}
