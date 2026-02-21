#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"

extern int useg;

/*
 * Syscall dispatcher.
 * Called from krt.s syscall dispatch entry, which is called
 * from trap.s SYSCALL handler.
 *
 * num  = syscall number (extracted from SC tag word)
 * regs = pointer to saved registers on stack
 *        regs[0]=R0, regs[1]=R1, regs[2]=R2, regs[3]=R3, ...
 *
 * V7 conventions:
 *   u.u_dirp = u.u_arg[0] (pathname for namei)
 *   u.u_arg[0..4] = R1..R5 (up to 5 args for syscalls like seek with off_t)
 *   Error: regs[0] = -1, regs[1] = errno
 *   Success: regs[0] = r_val1, regs[1] = r_val2
 */
trap(num, regs)
int num;
unsigned *regs;
{
	int saved_usp;

	saved_usp = get_usp();
	u.u_ar0 = regs;
	u.u_error = 0;
	u.u_intflg = 0;
	u.u_segflg = 0;		/* user-mode caller -> user address space */
	u.u_arg[0] = regs[1];		/* R1 = arg0 */
	u.u_arg[1] = regs[2];		/* R2 = arg1 */
	u.u_arg[2] = regs[3];		/* R3 = arg2 */
	u.u_arg[3] = regs[4];		/* R4 = arg3 */
	u.u_arg[4] = regs[5];		/* R5 = arg4 */
	u.u_dirp = (caddr_t)u.u_arg[0]; /* V7: arg0 is pathname for namei */
	u.u_ap = u.u_arg;
	u.u_r.r_val1 = 0;
	u.u_r.r_val2 = 0;

	if (num < 0 || num >= 64 || sysent[num].sy_call == NULL) {
		u.u_error = ENOSYS;
	} else {
		(*sysent[num].sy_call)();
	}

	/*
	 * exec replaces the user stack entirely via set_usp().
	 * Re-read it so we don't clobber the new SP at trap exit.
	 */
	if (num == 11 && u.u_error == 0)
		saved_usp = get_usp();

	if (u.u_intflg) {
		u.u_error = EINTR;
		u.u_intflg = 0;
	}

	if (u.u_error) {
		regs[0] = -1;		/* error indicator */
		regs[1] = u.u_error;	/* error code in R1 */
	} else {
		regs[0] = u.u_r.r_val1;
		regs[1] = u.u_r.r_val2;
	}

	/*
	 * Ensure IRET returns to the current process's segment.
	 * Critical after fork: child's kernel stack was copied from
	 * parent and has parent's segment encoding in PC_high.
	 */
	regs[15] = useg;

	if (issig())
		psig();

	curpri = setpri(u.u_procp);

	set_usp(saved_usp);
}
