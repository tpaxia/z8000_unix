#include <stdio.h>
#include <sgtty.h>
#include <errno.h>

extern int errno;
int failed;

check(name, ok)
char *name;
{
	if (!ok) {
		printf("tty: FAIL %s\n", name);
		failed++;
	}
}

same(a, b, n)
char *a, *b;
{
	while (n--)
		if (*a++ != *b++)
			return(0);
	return(1);
}

main(argc, argv)
int argc;
char **argv;
{
	struct sgttyb saved, mode, got;
	struct tchars chars, changed, gotchars;
	char buf[32];
	int n, fd, discipline;

	if (gtty(0, &saved) < 0)
		return(1);
	check("ioctl GETP", ioctl(0, TIOCGETP, &got) == 0 &&
	    same(&saved, &got, sizeof(saved)));
	check("GETC", ioctl(0, TIOCGETC, &chars) == 0);
	changed = chars;
	changed.t_intrc = 7;
	check("SETC", ioctl(0, TIOCSETC, &changed) == 0);
	check("GETC round trip", ioctl(0, TIOCGETC, &gotchars) == 0 &&
	    same(&changed, &gotchars, sizeof(changed)));
	check("restore characters", ioctl(0, TIOCSETC, &chars) == 0);
	check("bad fd", ioctl(-1, TIOCGETP, &got) == -1 && errno == EBADF);
	fd = open("/etc/init", 0);
	check("not a tty", ioctl(fd, TIOCGETP, &got) == -1 && errno == ENOTTY);
	check("close on exec", ioctl(fd, FIOCLEX, 0) == 0);
	check("clear close on exec", ioctl(fd, FIONCLEX, 0) == 0);
	close(fd);
	check("unknown command", ioctl(0, 0, &got) == -1 && errno == ENOTTY);
	discipline = -1;
	check("get discipline", ioctl(0, TIOCGETD, &discipline) == 0 && discipline == 0);
	check("set ordinary discipline", ioctl(0, TIOCSETD, &discipline) == 0);
	discipline = 1;
	check("unconfigured discipline", ioctl(0, TIOCSETD, &discipline) == -1 && errno == ENXIO);
	check("discipline unchanged", ioctl(0, TIOCGETD, &discipline) == 0 && discipline == 0);
	check("discipline ioctl", ioctl(0, DIOCGETP, &got) == -1 && errno == ENODEV);
	check("bad discipline address", ioctl(0, TIOCGETD, (char *)0xffff) == -1 && errno == EFAULT);
	check("bad parameters", ioctl(0, TIOCSETP, (char *)0xffff) == -1 && errno == EFAULT);
	check("parameters preserved", gtty(0, &got) == 0 && same(&saved, &got, sizeof(saved)));
	mode = saved;
	mode.sg_flags = CRMOD;
	mode.sg_ispeed = B9600;
	mode.sg_ospeed = B9600;
	mode.sg_erase = '#';
	mode.sg_kill = '@';
	check("SETN", ioctl(0, TIOCSETN, &mode) == 0);
	check("SETN round trip", gtty(0, &got) == 0 &&
	    same(&mode, &got, sizeof(mode)));
	check("flush", ioctl(0, TIOCFLUSH, 0) == 0);
	if (argc != 2)
		return(1);
	if (argv[1][0] == 'r')
		mode.sg_flags = RAW;
	if (argv[1][0] == 'b')
		mode.sg_flags = CBREAK;
	if (argv[1][0] == 'e')
		mode.sg_flags |= ECHO;
	check("stty", stty(0, &mode) == 0);
	check("stty round trip", gtty(0, &got) == 0 &&
	    same(&mode, &got, sizeof(mode)));
	if (argv[1][0] == 'r')
		write(1, "rawbytes:\200\377:end\n", 16);
	printf("tty: ready\n");
	fflush(stdout);
	n = read(0, buf, sizeof(buf));
	if (argv[1][0] == 'r')
		check("raw single byte", n == 1 && buf[0] == 'Q');
	else if (argv[1][0] == 'b')
		check("cbreak bypasses erase", n == 1 && buf[0] == '#');
	else
		check("cooked erase and kill", n == 3 && same(buf, "ac\n", 3));
	check("restore settings", stty(0, &saved) == 0);
	check("restored settings", gtty(0, &got) == 0 &&
	    same(&saved, &got, sizeof(saved)));
	if (!failed)
		printf("tty: all checks passed\n");
	return(failed != 0);
}
