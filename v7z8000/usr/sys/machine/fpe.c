#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"

extern int useg, iseg;

/* r[0..15] = all CPU registers, then tag, FCW, PC segment, PC offset.
 * userret uses the existing R0..R12/tag/FCW/PC layout, so adapt a copy.
 * The engine cannot sleep; interrupts may run but never schedule in SYS.
 */
fptrap(r)
unsigned *r;
{
	unsigned v[17], op, w, n, a, kind, fr;
	int i, error;

	if (r[17] & 0xc000)
		panic("kernel epu");
	op = r[16];
	a = r[19];
	if (a & 1) {
		error = SIGINS;
		goto done;
	}
	copyiin(a, &w, 2);
	/* Reject invalid register/count fields before the historical decoder
	 * can turn them into pointers beyond its saved frame/workspace.
	 * We expose the documented register operations, plus bounded memory
	 * transfers. Reserved EPU numbers/opcodes are illegal instructions.
	 */
	n = w & 15;
	kind = w & 0xc0f0;
	fr = (op & 0xff00) == 0x8f00 ? ((op >> 4) & 15) : ((w >> 8) & 15);
	error = SIGINS;
	if ((op & 0xff00) == 0x8e00) {
		if ((op & 15) != 0 && (op & 15) != 4)
			goto done;
	} else if ((op & 0xff00) == 0x8f00) {
		if ((op & 15) != 0 && (op & 15) != 8)
			goto done;
	} else if ((op & 15) != 4 && (op & 15) != 12)
		goto done;
	/* Expose tested arithmetic/transfers, not reserved/no-op decoder
	 * entries or the upstream broken double-precision FINT operation.
	 * Control transfers are limited to flags and user mode (32 bits).
	 */
	if ((op & 0xff00) == 0x8e00) {
		if (n || (kind != 0 && kind != 0x0010 && kind != 0x0020 &&
		    kind != 0x4010 && kind != 0x4020 && kind != 0x4080 &&
		    kind != 0x4090 && kind != 0x40a0 && kind != 0x40b0 &&
		    kind != 0x8080 && kind != 0x8090 && kind != 0x80c0))
			goto done;
	} else {
		if (kind == 0xc000) {
			if ((fr != 4 && fr != 5) || n != 1)
				goto done;
		} else if (kind == 0x8000) {
			if (n != 1 && n != 3 && n != 4)
				goto done;
		} else if (kind == 0x8020 || kind == 0x8040) {
			if (n != 1 && n != 3)
				goto done;
		} else goto done;
	}
	if ((op & 3) ||
	    (((op & 0xff00) == 0x8e00 || (op & 0xff00) == 0x8f00)
	     && ((op >> 4) & 15) > 7) ||
	    ((op & 0xff00) != 0x8f00 && ((w >> 8) & 15) > 7)) {
		error = SIGINS;
		goto done;
	}
	if ((op & 0xff00) == 0x8f00) {
		if (((w >> 8) & 15) + n >= 16) {
			error = SIGINS;
			goto done;
		}
	} else if ((op & 0xff00) != 0x8e00) {
		/* Memory operands use a 20-byte decoder buffer. */
		if (n >= 10 || ((op & 0xff00) != 0x0f00 &&
		    (op & 0xff00) != 0x4f00)) {
			error = SIGINS;
			goto done;
		}
	}
	if (!u.u_fpflag) {
		bzero(u.u_fpe, sizeof(u.u_fpe));
		u.u_fpe[93] = 1; /* affine infinity; nearest/even */
		u.u_fpflag = 1;
	}
	error = fprun(r, u.u_fpe, useg, iseg);
	error = (error & 255) ? SIGSEG : (error ? SIGFPT : 0);
done:
	u.u_regs[13] = r[13]; u.u_regs[14] = r[14];
	for (i = 0; i < 13; i++)
		v[i] = r[i];
	for (i = 13; i < 17; i++)
		v[i] = r[i+3];
	if (error)
		psignal(u.u_procp, error);
	userret(v, r[15]);
	for (i = 0; i < 13; i++)
		r[i] = v[i];
	for (i = 13; i < 17; i++)
		r[i+3] = v[i];
	r[13] = u.u_regs[13]; r[14] = u.u_regs[14];
	r[15] = get_usp();
}

/* Signal trampoline restores only EPU registers/control, never CPU state. */
fprestore()
{
	unsigned a;
	char state[96];
	a = u.u_arg[0];
	if (a > 65440) {
		u.u_error = EFAULT;
		return;
	}
	if (copyin(a, state, 96) < 0) {
		u.u_error = EFAULT;
		return;
	}
	bcopy(state, u.u_fpe, 96);
	u.u_fpflag = 1;
}
