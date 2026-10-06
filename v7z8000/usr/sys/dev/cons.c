#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"
#include "../h/tty.h"
#include "../h/conf.h"

/*
 * Console character device driver.
 *
 * Uses V7 TTY subsystem for line discipline, echo, and buffering.
 * Follows the V7 kl.c pattern: tty struct, t_oproc callback,
 * interrupt-driven receive via consrint().
 *
 * Console I/O ports:
 *   0xF0 = TX/RX data
 *   0xF2 = status (bit 0 = TX ready, bit 1 = RX ready)
 */

extern int putchar();
extern int inb();
extern int consstart();
extern int ttrstrt();

struct tty cons_tty[1];

consopen(dev, flag)
dev_t dev;
{
	register struct tty *tp;

	tp = &cons_tty[0];
	tp->t_oproc = consstart;
	if ((tp->t_state & ISOPEN) == 0) {
		tp->t_state = ISOPEN|CARR_ON;
		tp->t_flags = ECHO|CRMOD;
		ttychars(tp);
	}
	ttyopen(dev, tp);
}

consclose(dev, flag)
dev_t dev;
{
	ttyclose(&cons_tty[0]);
}

consread(dev)
dev_t dev;
{
	ttread(&cons_tty[0]);
}

conswrite(dev)
dev_t dev;
{
	ttwrite(&cons_tty[0]);
}

consioctl(dev, cmd, addr, flag)
dev_t dev;
caddr_t addr;
{
	ttioccomm(cmd, &cons_tty[0], addr, dev);
}

/*
 * Console receive interrupt handler.
 * Called from vi_dispatch on VI(0).
 * Checks RX-ready status and feeds characters to ttyinput.
 */
consrint()
{
	register struct tty *tp;
	register int c;

	tp = &cons_tty[0];
	if (inb(0xF2) & 0x02) {
		c = inb(0xF0);
		ttyinput(c, tp);
	}
}

/*
 * Console start output routine (t_oproc callback).
 * Drains t_outq via putchar. The emulator console is always ready,
 * so we drain the entire queue. Delay characters (>0200) are
 * handled via timeout for ttrstrt.
 */
consstart(tp)
register struct tty *tp;
{
	register int c;

	while ((c = getc(&tp->t_outq)) >= 0) {
		if ((c & 0200) && (tp->t_flags & RAW) == 0) {
			/* delay character — schedule restart */
			tp->t_state |= TIMEOUT;
			timeout(ttrstrt, (caddr_t)tp, c & 0177);
			break;
		}
		putchar(c);
	}
	if (tp->t_outq.c_cc == 0 || tp->t_outq.c_cc <= TTLOWAT) {
		if (tp->t_state & ASLEEP) {
			tp->t_state &= ~ASLEEP;
			wakeup((caddr_t)&tp->t_outq);
		}
	}
}
