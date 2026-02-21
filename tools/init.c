/*
 * /etc/init - simplified V7 init for Z8000.
 *
 * Opens console as fd 0/1/2, then forks.
 * Parent waits for children; when child exits, forks again.
 * Child exec's /bin/sh as login shell ("-sh").
 */

main()
{
	int fd;
	int pid;

	/* Open console as stdin */
	fd = open("/dev/console", 2);	/* O_RDWR */
	if (fd < 0)
		_exit(1);
	/* Close any inherited fd 0 */
	close(0);
	dup(fd);		/* fd 0 = stdin */
	close(1);
	dup(fd);		/* fd 1 = stdout */
	close(2);
	dup(fd);		/* fd 2 = stderr */
	if (fd > 2)
		close(fd);

	for (;;) {
		pid = fork();
		if (pid < 0) {
			write(2, "init: fork failed\n", 18);
			_exit(1);
		}
		if (pid == 0) {
			/* child: exec shell */
			execl("/bin/sh", "-sh", 0);
			write(2, "init: no shell\n", 15);
			_exit(1);
		}
		/* parent: wait for child */
		while (wait(0) != pid)
			;
	}
}
