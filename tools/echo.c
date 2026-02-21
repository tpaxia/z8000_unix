/*
 * /bin/echo - print arguments.
 */

main(argc, argv)
int argc;
char **argv;
{
	int i;
	char *s;
	int n;

	for (i = 1; i < argc; i++) {
		s = argv[i];
		/* compute strlen */
		for (n = 0; s[n]; n++)
			;
		write(1, s, n);
		if (i < argc - 1)
			write(1, " ", 1);
	}
	write(1, "\n", 1);
}
