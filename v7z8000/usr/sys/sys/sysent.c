#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"

/*
 * System call dispatch table.
 * V7-style sysent[64] array indexed by syscall number.
 *
 * Step 8 entries:
 *   #1  exit   (rexit)
 *   #2  fork   (fork)
 *   #4  write  (write)
 *   #7  wait   (wait)
 *
 * All others -> nosys (returns ENOSYS).
 */

extern int rexit(), fork(), read(), write(), wait1(), exec();

nosys()
{
	u.u_error = ENOSYS;
}

struct sysent sysent[] = {
	{ 0, 0, nosys },	/*  0 = indir (unused) */
	{ 1, 1, rexit },	/*  1 = exit */
	{ 0, 0, fork },		/*  2 = fork */
	{ 3, 3, read },		/*  3 = read */
	{ 3, 3, write },	/*  4 = write */
	{ 0, 0, nosys },	/*  5 = open (not yet) */
	{ 0, 0, nosys },	/*  6 = close (not yet) */
	{ 0, 0, wait1 },	/*  7 = wait */
	{ 0, 0, nosys },	/*  8 = creat */
	{ 0, 0, nosys },	/*  9 = link */
	{ 0, 0, nosys },	/* 10 = unlink */
	{ 2, 2, exec },		/* 11 = exec */
	{ 0, 0, nosys },	/* 12 = chdir */
	{ 0, 0, nosys },	/* 13 = time */
	{ 0, 0, nosys },	/* 14 = mknod */
	{ 0, 0, nosys },	/* 15 = chmod */
	{ 0, 0, nosys },	/* 16 = chown */
	{ 0, 0, nosys },	/* 17 = break */
	{ 0, 0, nosys },	/* 18 = stat */
	{ 0, 0, nosys },	/* 19 = seek */
	{ 0, 0, nosys },	/* 20 = getpid */
	{ 0, 0, nosys },	/* 21 = mount */
	{ 0, 0, nosys },	/* 22 = umount */
	{ 0, 0, nosys },	/* 23 = setuid */
	{ 0, 0, nosys },	/* 24 = getuid */
	{ 0, 0, nosys },	/* 25 = stime */
	{ 0, 0, nosys },	/* 26 = ptrace */
	{ 0, 0, nosys },	/* 27 = alarm */
	{ 0, 0, nosys },	/* 28 = fstat */
	{ 0, 0, nosys },	/* 29 = pause */
	{ 0, 0, nosys },	/* 30 = utime */
	{ 0, 0, nosys },	/* 31 = stty */
	{ 0, 0, nosys },	/* 32 = gtty */
	{ 0, 0, nosys },	/* 33 = access */
	{ 0, 0, nosys },	/* 34 = nice */
	{ 0, 0, nosys },	/* 35 = ftime */
	{ 0, 0, nosys },	/* 36 = sync */
	{ 0, 0, nosys },	/* 37 = kill */
	{ 0, 0, nosys },	/* 38 */
	{ 0, 0, nosys },	/* 39 */
	{ 0, 0, nosys },	/* 40 */
	{ 0, 0, nosys },	/* 41 = dup */
	{ 0, 0, nosys },	/* 42 = pipe */
	{ 0, 0, nosys },	/* 43 = times */
	{ 0, 0, nosys },	/* 44 = prof */
	{ 0, 0, nosys },	/* 45 */
	{ 0, 0, nosys },	/* 46 = setgid */
	{ 0, 0, nosys },	/* 47 = getgid */
	{ 0, 0, nosys },	/* 48 = signal */
	{ 0, 0, nosys },	/* 49 */
	{ 0, 0, nosys },	/* 50 */
	{ 0, 0, nosys },	/* 51 */
	{ 0, 0, nosys },	/* 52 */
	{ 0, 0, nosys },	/* 53 */
	{ 0, 0, nosys },	/* 54 = ioctl */
	{ 0, 0, nosys },	/* 55 */
	{ 0, 0, nosys },	/* 56 */
	{ 0, 0, nosys },	/* 57 */
	{ 0, 0, nosys },	/* 58 */
	{ 0, 0, nosys },	/* 59 */
	{ 0, 0, nosys },	/* 60 */
	{ 0, 0, nosys },	/* 61 */
	{ 0, 0, nosys },	/* 62 */
	{ 0, 0, nosys },	/* 63 */
};
