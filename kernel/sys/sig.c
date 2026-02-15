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
 * signal(sig, func) - set signal disposition.
 * No-op, returns 0.
 */
ssignal(sig, func)
{
	return(0);
}
