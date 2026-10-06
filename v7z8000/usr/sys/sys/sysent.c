#include "../h/param.h"
#include "../h/systm.h"
#include "../h/dir.h"
#include "../h/user.h"

/*
 * System call dispatch table.
 * V7-style sysent[64] array indexed by syscall number.
 */

extern int rexit(), fork(), read(), write(), open(), close();
extern int wait1(), creat(), link(), exec(), unlink(), chdir();
extern int gtime(), mknod(), chmod(), chown(), sbreak(), stat();
extern int seek(), getpid(), smount(), sumount(), setuid();
extern int fstat(), getuid(), stime(), alarm(), pause(), utime();
extern int stty(), gtty(), saccess(), nice(), ftime(), sync();
extern int kill(), dup(), pipe(), times(), profil(), setgid();
extern int getgid(), ssig(), ioctl(), umask(), chroot();
extern int fprestore();

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
	{ 2, 2, open },		/*  5 = open */
	{ 1, 1, close },	/*  6 = close */
	{ 0, 0, wait1 },	/*  7 = wait */
	{ 2, 2, creat },	/*  8 = creat */
	{ 2, 2, link },		/*  9 = link */
	{ 1, 1, unlink },	/* 10 = unlink */
	{ 2, 2, exec },		/* 11 = exec */
	{ 1, 1, chdir },	/* 12 = chdir */
	{ 0, 0, gtime },	/* 13 = time */
	{ 3, 3, mknod },	/* 14 = mknod */
	{ 2, 2, chmod },	/* 15 = chmod */
	{ 3, 3, chown },	/* 16 = chown */
	{ 1, 1, sbreak },	/* 17 = break */
	{ 2, 2, stat },		/* 18 = stat */
	{ 4, 4, seek },		/* 19 = seek */
	{ 0, 0, getpid },	/* 20 = getpid */
	{ 3, 3, smount },	/* 21 = mount */
	{ 1, 1, sumount },	/* 22 = umount */
	{ 1, 1, setuid },	/* 23 = setuid */
	{ 0, 0, getuid },	/* 24 = getuid */
	{ 2, 2, stime },	/* 25 = stime */
	{ 0, 0, nosys },	/* 26 = ptrace */
	{ 1, 1, alarm },	/* 27 = alarm */
	{ 2, 2, fstat },	/* 28 = fstat */
	{ 0, 0, pause },	/* 29 = pause */
	{ 2, 2, utime },	/* 30 = utime */
	{ 1, 1, stty },		/* 31 = stty */
	{ 1, 1, gtty },		/* 32 = gtty */
	{ 2, 2, saccess },	/* 33 = access */
	{ 1, 1, nice },		/* 34 = nice */
	{ 1, 1, ftime },	/* 35 = ftime */
	{ 0, 0, sync },		/* 36 = sync */
	{ 2, 2, kill },		/* 37 = kill */
	{ 0, 0, nosys },	/* 38 */
	{ 0, 0, nosys },	/* 39 */
	{ 0, 0, nosys },	/* 40 */
	{ 2, 2, dup },		/* 41 = dup */
	{ 0, 0, pipe },		/* 42 = pipe */
	{ 1, 1, times },	/* 43 = times */
	{ 4, 4, profil },	/* 44 = prof */
	{ 0, 0, nosys },	/* 45 */
	{ 1, 1, setgid },	/* 46 = setgid */
	{ 0, 0, getgid },	/* 47 = getgid */
	{ 2, 2, ssig },		/* 48 = signal */
	{ 0, 0, nosys },	/* 49 */
	{ 0, 0, nosys },	/* 50 */
	{ 0, 0, nosys },	/* 51 */
	{ 1, 1, fprestore },	/* 52 = restore signal EPU state */
	{ 0, 0, nosys },	/* 53 */
	{ 3, 3, ioctl },	/* 54 = ioctl */
	{ 0, 0, nosys },	/* 55 */
	{ 0, 0, nosys },	/* 56 */
	{ 0, 0, nosys },	/* 57 */
	{ 0, 0, nosys },	/* 58 */
	{ 0, 0, nosys },	/* 59 */
	{ 0, 0, nosys },	/* 60 */
	{ 1, 1, umask },	/* 61 = umask */
	{ 1, 1, chroot },	/* 62 = chroot */
	{ 0, 0, nosys },	/* 63 */
};
