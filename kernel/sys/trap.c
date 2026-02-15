#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"

/*
 * Syscall dispatcher.
 * Called from krt.s syscall dispatch entry, which is called
 * from trap.s SYSCALL handler.
 *
 * num  = syscall number (extracted from SC tag word)
 * regs = pointer to saved registers on stack
 *        regs[0]=R0, regs[1]=R1, regs[2]=R2, regs[3]=R3, ...
 */
trap(num, regs)
int num;
unsigned *regs;
{
	u.u_ar0 = regs;
	u.u_error = 0;
	u.u_segflg = 0;		/* user-mode caller -> user address space */
	u.u_arg[0] = regs[1];		/* R1 = arg1 */
	u.u_arg[1] = regs[2];		/* R2 = arg2 */
	u.u_arg[2] = regs[3];		/* R3 = arg3 */
	u.u_ap = u.u_arg;
	u.u_r.r_val1 = 0;

	if (num < 0 || num >= 64 || sysent[num].sy_call == NULL) {
		u.u_error = ENOSYS;
	} else {
		(*sysent[num].sy_call)();
	}

	if (u.u_error) {
		regs[0] = u.u_error;	/* error code in R0 */
		/* TODO: set carry flag for error indication */
	} else {
		regs[0] = u.u_r.r_val1;
	}
}
