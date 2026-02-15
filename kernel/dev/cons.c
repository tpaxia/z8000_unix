#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/inode.h"
#include "../h/conf.h"

/*
 * Console character device driver.
 *
 * Writes characters to the emulator console port via putchar().
 * Read is a no-op for now (no input support yet).
 *
 * Everything runs in kernel space, so u.u_base points to
 * kernel memory and we can dereference it directly.
 */

extern int putchar();

consopen(dev, rw)
dev_t dev;
{
}

consclose(dev, rw)
dev_t dev;
{
}

consread(dev)
dev_t dev;
{
	/* No input support yet */
}

conswrite(dev)
dev_t dev;
{
	register int c;

	while (u.u_count > 0) {
		c = *u.u_base & 0377;
		putchar(c);
		u.u_base++;
		u.u_count--;
		u.u_offset++;
	}
}
