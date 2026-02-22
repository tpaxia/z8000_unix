/* cat - concatenate files (minimal, no stdio) */
main(argc, argv)
char **argv;
{
	char buf[512];
	int fd, n;

	if (argc <= 1) {
		/* stdin -> stdout */
		while ((n = read(0, buf, 512)) > 0)
			write(1, buf, n);
	} else {
		while (--argc > 0) {
			if ((fd = open(*++argv, 0)) < 0) {
				write(2, "cat: cannot open\n", 17);
				continue;
			}
			while ((n = read(fd, buf, 512)) > 0)
				write(1, buf, n);
			close(fd);
		}
	}
}
