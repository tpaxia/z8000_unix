/*
 * libctest - exercise the Seventh Edition C library under the kernel.
 *
 * Each check has a number. A failed check prints "FAIL n"; the last line
 * is "libc: P passed, F failed". The verdict line goes through printf and
 * stdio buffering, so seeing it at all means those work and that exit
 * flushed the buffer.
 */
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

char	*malloc(), *calloc(), *index(), *rindex(), *strcpy(), *strcat();
long	atol();
double	atof();
char	*getenv();
extern char **environ;
extern int errno;

int	passed, failed;

check(n, ok)
{
	if (ok)
		passed++;
	else {
		failed++;
		printf("FAIL %d\n", n);
	}
}

same(a, b)
char *a, *b;
{
	return (strcmp(a, b) == 0);
}

intcmp(a, b)
int *a, *b;
{
	return (*a - *b);
}

/* Re-enter through exec with a known argument and environment layout. */
startup(empty)
int empty;
{
	int pid, status;
	char *args[3], *env[3];

	args[0] = "libctest";
	args[1] = empty ? "empty" : "environment";
	args[2] = 0;
	env[0] = "LIBC_STARTUP=present";
	env[1] = "EMPTY=";
	env[2] = 0;
	pid = fork();
	if (pid < 0)
		return(0);
	if (pid == 0) {
		execve("/bin/libctest", args, empty ? (char **)0 : env);
		_exit(1);
	}
	return(wait(&status) == pid && status == 0);
}

main(argc, argv, envp)
int argc;
char **argv, **envp;
{
	char buf[80], word[20], *p, *q;
	int v[8], i, n;
	unsigned u;
	long l;
	double d;
	FILE *f;

	if (argc == 2) {
		if (argv[2] != 0 || !same(argv[0], "libctest") ||
		    envp != environ || environ != argv + argc + 1 || errno != 0)
			return(1);
		if (same(argv[1], "empty"))
			return(environ[0] != 0 || getenv("LIBC_STARTUP") != NULL);
		p = getenv("LIBC_STARTUP");
		q = getenv("EMPTY");
		return(!same(argv[1], "environment") || p == NULL || q == NULL ||
		    !same(p, "present") || !same(q, "") || environ[2] != 0);
	}

	/* strings */
	strcpy(buf, "hello");
	strcat(buf, ", world");
	check(1, same(buf, "hello, world") && strlen(buf) == 12);
	check(2, strcmp("abc", "abd") < 0 && strncmp("abcx", "abcy", 3) == 0);
	check(3, index(buf, 'w') == buf + 7 && rindex(buf, 'o') == buf + 8);
	check(4, index(buf, 'z') == NULL);

	/* ctype and number conversion */
	check(5, isalpha('q') && isdigit('7') && isspace(' ') && !isupper('q') && toupper('q') == 'Q');
	check(6, atoi("  -1234") == -1234 && atol("123456789") == 123456789L);
	check(7, abs(-9) == 9);

	/* malloc */
	p = malloc(100);
	q = calloc(10, 4);
	check(8, p != NULL && q != NULL && p != q);
	for (i = 0, n = 0; i < 40; i++)
		n += q[i];
	check(9, n == 0);
	strcpy(p, "kept");
	free(q);
	q = malloc(30);
	check(10, q != NULL && same(p, "kept"));
	free(p);
	free(q);

	/* qsort */
	v[0] = 5; v[1] = -3; v[2] = 9; v[3] = 0; v[4] = 7; v[5] = -8; v[6] = 2; v[7] = 1;
	qsort((char *)v, 8, sizeof(int), intcmp);
	for (i = 1, n = 1; i < 8; i++)
		if (v[i - 1] > v[i])
			n = 0;
	check(11, n && v[0] == -8 && v[7] == 9);

	/* sprintf: integers */
	u = 60000;		/* a constant this big is a long; pass an unsigned */
	sprintf(buf, "%d %d %u", 42, -42, u);
	check(12, same(buf, "42 -42 60000"));
	sprintf(buf, "%o %x %c%c", 0755, 0xbeef, 'o', 'k');
	check(13, same(buf, "755 beef ok"));
	sprintf(buf, "%ld %lx %D", 123456789L, 0x12345678L, -70000L);
	check(14, same(buf, "123456789 12345678 -70000"));
	sprintf(buf, "[%5d] [%-5d] [%05d] [%5d]", 42, 42, 42, -42);
	check(15, same(buf, "[   42] [42   ] [00042] [  -42]"));
	sprintf(buf, "[%s] [%8s] [%-8s] [%.2s] [%*d]", "abc", "abc", "abc", "abc", 4, 7);
	check(16, same(buf, "[abc] [     abc] [abc     ] [ab] [   7]"));
	sprintf(buf, "100%% %d%s", 3, "x");
	check(17, same(buf, "100% 3x"));

	/* floating point: conversion both ways */
	d = atof("3.25");
	check(18, d == 3.25);
	check(19, atof("-1.5e2") == -150.0 && atof("0.0625") == 0.0625);
	sprintf(buf, "%f", 3.25);
	check(20, same(buf, "3.250000"));
	sprintf(buf, "%.2f %.0f %.3f", 3.14159, 2.5, -0.5);
	check(21, same(buf, "3.14 3 -0.500") || same(buf, "3.14 2 -0.500"));
	sprintf(buf, "%e", 1234.5);
	check(22, same(buf, "1.234500e+03"));
	sprintf(buf, "%g %g", 0.5, 100.0);
	check(23, same(buf, ".5 100"));		/* V7's gcvt writes no leading zero */
	sprintf(buf, "%8.3f|%-8.1f|", 2.5, 2.5);
	check(24, same(buf, "   2.500|2.5     |"));

	/* sscanf */
	n = sscanf("17 abc -5 2.5", "%d %s %ld %lf", &i, word, &l, &d);
	check(25, n == 4 && i == 17 && same(word, "abc") && l == -5L && d == 2.5);
	n = sscanf("ff 17", "%x %o", &i, &v[0]);
	check(26, n == 2 && i == 255 && v[0] == 15);

	/* files */
	f = fopen("/tmp/libctest", "w");
	check(27, f != NULL);
	if (f != NULL) {
		fprintf(f, "line %d\n", 1);
		fputs("second line\n", f);
		putc('x', f);
		check(28, fclose(f) == 0);
	}
	f = fopen("/tmp/libctest", "r");
	check(29, f != NULL);
	if (f != NULL) {
		check(30, fgets(buf, sizeof buf, f) != NULL && same(buf, "line 1\n"));
		check(31, fgets(buf, sizeof buf, f) != NULL && same(buf, "second line\n"));
		check(32, getc(f) == 'x' && getc(f) == EOF && feof(f));
		fclose(f);
	}
	check(33, unlink("/tmp/libctest") == 0 && fopen("/tmp/libctest", "r") == NULL);

	/* Startup ABI, shared syscall errno, populated and empty environments. */
	check(34, argc == 1 && argv[argc] == 0 && envp == environ &&
	    environ == argv + argc + 1);
	errno = 0;
	check(35, close(-1) == -1 && errno == EBADF);
	check(36, startup(0));
	check(37, startup(1));

	printf("libc: %d passed, %d failed\n", passed, failed);
	return (failed != 0);
}
