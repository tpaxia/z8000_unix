#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"

/*
 * Signal stubs for Step 8.
 * No signal handling yet -- just enough for slp.c to compile.
 */

/*
 * issig() - check if signal pending.
 * Always returns 0 (no signals).
 */
issig()
{
	return(0);
}

/*
 * psignal(p, sig) - send signal to process.
 * No-op.
 */
psignal(p, sig)
struct proc *p;
{
}

/*
 * signal(pgrp, sig) - send signal to process group.
 * Called by ttyinput() for SIGINT/SIGQUIT.
 * psignal is a no-op stub, so this effectively does nothing.
 */
signal(pgrp, sig)
{
	register struct proc *p;

	for (p = &proc[0]; p < &proc[NPROC]; p++)
		if (p->p_pgrp == pgrp)
			psignal(p, sig);
}

/*
 * ssignal(sig, func) - set signal disposition syscall.
 * No-op, returns 0.
 */
ssignal(sig, func)
{
	return(0);
}
