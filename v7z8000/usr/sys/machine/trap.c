#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "mmu.h"

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
trap(num, regs, extra)
int num;
unsigned *regs;
unsigned *extra;
{
	int saved_usp;

	saved_usp = get_usp();
	u.u_regs[13] = extra[0]; u.u_regs[14] = extra[1];
	u.u_ar0 = (int *)regs;
	/* SC 255 is the Z8000 breakpoint instruction, outside the syscall table.
	 * Preserve registers; saved PC is the following word, as specified for SC.
	 */
	if (num == 255) {
		psignal(u.u_procp, SIGTRC);
		userret(regs, saved_usp);
		extra[0] = u.u_regs[13]; extra[1] = u.u_regs[14];
		return;
	}
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
	} else if (save(u.u_qsav)) {
		/* sleep() unwinds here when a signal interrupts a syscall. */
		if (u.u_error == 0)
			u.u_error = EINTR;
	} else {
		(*sysent[num].sy_call)();
	}

	/*
	 * exec stages its new SP in the process-local register image. Hardware
	 * NSPOFF may belong to another process after sleeping in closef/iput.
	 */
	if ((num == 11 || num == 59) && u.u_error == 0)
		saved_usp = u.u_regs[15];

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

	userret(regs, saved_usp);
	extra[0] = u.u_regs[13]; extra[1] = u.u_regs[14];
}

/*
 * Interrupt return. The assembly wrapper supplies the same saved-register
 * layout as a syscall. Kernel interrupts never switch processes here.
 */
intrret(regs, extra)
unsigned *regs;
unsigned *extra;
{
	if ((regs[14] & 0x4000) == 0) {
		u.u_regs[13] = extra[0]; u.u_regs[14] = extra[1];
		userret(regs, get_usp());
		extra[0] = u.u_regs[13]; extra[1] = u.u_regs[14];
	}
}

/*
 * Common return to user mode: deliver signals and honor clock/wakeup
 * rescheduling requests. Keep the user SP on this process's kernel stack:
 * NSPOFF is a CPU register and another process can change it in qswtch().
 * Recheck after switching, since signals can arrive while we are off CPU.
 * Mask the final check through IRET so no request slips past this boundary.
 */
userret(regs, usp)
unsigned *regs;
int usp;
{
	u.u_ar0 = (int *)regs;
	for (;;) {
		spl0();
		if (issig())
			usp = psig(usp);
		curpri = setpri(u.u_procp);
		spl7();
		if (runrun) {
			spl0();
			qswtch();
		} else if (!issig())
			break;
	}
	set_usp(usp);
}

/*
 * Only the listed cross-segment user accesses may recover a kernel fault.
 * Z8001 SEGT is delivered after the offending instruction; the table holds
 * that saved PC and a landing pad restoring the helper's original FCW/stack.
 */
segtrap(regs, extra)
unsigned *regs;
unsigned *extra;
{
	extern unsigned ufixups[];
	register unsigned *p;
	unsigned fault[6];
	int i;

	for (i = 0; i < 6; i++)
		fault[i] = inw(MM_FAULT+2*i);
	outw(MM_ACK, 0);
	if (regs[14] & 0x4000) {
		if ((regs[14] & 0x8000) && regs[15] == 0x8100)
			for (p = ufixups; p[0]; p += 2)
				if (regs[16] == p[0]) {
					regs[16] = p[1];
					return;
				}
		panic("kernel access fault");
		return;
	}
	u.u_regs[13] = extra[0]; u.u_regs[14] = extra[1];
	if (!stackfault(regs, fault))
		psignal(u.u_procp, SIGSEG);
	userret(regs, get_usp());
	extra[0] = u.u_regs[13]; extra[1] = u.u_regs[14];
}

/* Z8001 completes the faulting instruction. Only replay stores with no
 * register/flag changes, or back out an implicit R15 decrement. Encodings:
 * Z8000 CPU Technical Manual, CALL/CALR, LD, LDM and PUSH tables.
 * CLR/CLRB also only write memory and leave all flags/registers unchanged.
 * MMU first-word/status latches are bus-visible state, not CPU rollback.
 */
stackfault(regs, f)
unsigned *regs, *f;
{
	unsigned sp, op, hi, back, base;
	int b;
	if ((regs[14] & 0x8000) || !(f[0] & MF_VALID) ||
	    (f[0] & (MF_FETCH|MF_PROT|MF_MIXED|MF_READ)) ||
	    !(f[0] & MF_WRITE) || f[1] != u.u_procp-proc+1 ||
	    f[4] != f[1])
		return(0);
	sp = get_usp();
	base = (unsigned)(-(((u.u_ssize+31)/32)*2048));
	if (!(f[0] & MF_UNMAP)) {
		if (!(f[0] & MF_WARN)) return(0);
		/* A warning reports a successful store. A valid mapped access
		 * below SP must not become SIGSEGV, nor does failed pre-growth
		 * invalidate that store. A later real fault can still fail. */
		if (sp && base >= 256 && sp <= base+256)
			grow(base-256);
		return(1);
	}
	if (!sp || f[2] < sp || f[3] < f[2] || f[2] >= base)
		return(0);
	b = fuibyte(f[5]);
	if (b < 0 || f[5] == 65535)
		return(0);
	op = b << 8;
	b = fuibyte(f[5]+1);
	if (b < 0)
		return(0);
	op |= b;
	hi = op >> 8;
	back = 0;
	if ((op & 0xf000) == 0xd000 ||
	    ((op & 0xbf0f) == 0x1f00))
		back = 2;
	else if ((op & 0x00f0) == 0x00f0 &&
	    (hi == 0x93 || hi == 0x91 || op == 0x0df9)) {
		/* Do not replay a push whose source includes the modified SP. */
		if ((hi == 0x93 && (op & 15) == 15) ||
		    (hi == 0x91 && (op & 15) >= 14))
			return(0);
		back = hi == 0x91 ? 4 : 2;
	} else if (!(hi == 0x2e || hi == 0x2f || hi == 0x6e ||
	    hi == 0x6f || hi == 0x32 || hi == 0x33 || hi == 0x72 ||
	    hi == 0x73 || hi == 0x1d || hi == 0x5d || hi == 0x37 ||
	    hi == 0x77 || (op & 0xbe0f) == 0x0c08 ||
	    (op & 0xbf0f) == 0x0c05 ||
	    (op & 0xbf0f) == 0x0d05 || (op & 0xbf0f) == 0x1c09))
		return(0);
	if (back && sp > 65535-back)
		return(0);
	if (!grow(sp))
		return(0);
	set_usp(sp+back);
	regs[15] = useg;
	regs[16] = f[5];
	return(1);
}

/* R13/R14 are passed by the entry wrappers before C reuses them. Other
 * registers come from the unchanged trap frame; USP is process-local here.
 */
coreregs(usp)
unsigned usp;
{
	int i;
	for(i = 0; i < 13; i++) u.u_regs[i] = u.u_ar0[i];
	u.u_regs[15] = usp;
	for(i = 0; i < 3; i++) u.u_regs[16+i] = u.u_ar0[14+i];
}

/* V7 u-area reads, with writes confined to saved user registers and EPU state.
 * FCW writes affect arithmetic flags only, never mode, EPU or IRQ enables.
 */
traceuser(req, off, value)
unsigned off;
int *value;
{
	int *p, i, v;
	if (off&1 || off >= ctob(USIZE)) return(-1);
	p = (int *)((char *)&u+off);
	if (req == 3) { *value = *p; return(0); }
	for(i = 0; i < 19; i++) if (p == &u.u_regs[i]) goto reg;
	for(i = 0; i < 13; i++) if (p == &u.u_ar0[i]) goto reg;
	if (p == &u.u_ar0[14]) { i = 16; goto reg; }
	if (p == &u.u_ar0[16]) { i = 18; goto reg; }
	if (p >= (int *)u.u_fpe && p < (int *)(u.u_fpe+96)) {
		*p = *value; u.u_fpflag = 1; return(0);
	}
	return(-1);
reg:
	v = *value;
	if (i == 17 || ((i == 15 || i == 18) && (v&1))) return(-1);
	if (i == 16) v = (u.u_regs[16]&~0xfc) | (v&0xfc);
	u.u_regs[i] = v;
	if (i < 13) u.u_ar0[i] = v;
	if (i == 16) u.u_ar0[14] = v;
	if (i == 18) u.u_ar0[16] = v;
	return(0);
}

/* Hardware stepping is optional. This Z8001 configuration has no trace
 * facility; reject request 9 without changing the stopped register image.
 */
tracego(addr, step)
unsigned addr;
{
	if (step || (addr != 1 && (addr&1))) return(-1);
	if (addr != 1) u.u_regs[18] = u.u_ar0[16] = addr;
	return(0);
}
