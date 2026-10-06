#include "../h/param.h"
#include "../h/systm.h"
#include "../h/mount.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/proc.h"
#include "../h/inode.h"
#include "../h/buf.h"

xrele(ip) struct inode *ip; {}
xfree() {}
xumount(dev) {}
acct() {}

/*
 * bzero: zero count bytes starting at addr.
 */
bzero(addr, count)
char *addr;
int count;
{
	register char *p;
	register int n;

	p = addr;
	n = count;
	while (n--)
		*p++ = 0;
}

/*
 * icode[] - Z8002 machine code for process 1's initial program.
 *
 * Executes:
 *   exec("/etc/init", 0)  -- sc #11
 *
 * Layout:
 *   0x00: ld R1, #0x000C    (filename pointer)
 *   0x04: ld R2, #0         (argv = NULL)
 *   0x08: sc #11            (exec syscall)
 *   0x0A: halt              (should not reach)
 *   0x0C: "/etc/init\0"
 */
int icode[] = {
	0x2101, 0x000C,		/* ld R1, #0x000C    */
	0x2102, 0x0000,		/* ld R2, #0         */
	0x7F0B,			/* sc #11            */
	0x7A00,			/* halt              */
	/* "/etc/init\0" at offset 0x0C */
	0x2F65, 0x7463, 0x2F69, 0x6E69, 0x7400
};
int szicode = sizeof(icode);
