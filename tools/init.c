/*
 * /etc/init - simplified V7 init for Z8000.
 *
 * Opens console as fd 0/1/2, then forks.
 * Parent waits for children; when child exits, forks again.
 * Child exec's /bin/sh as login shell ("-sh").
 */

main()
{
	int pid;

	/* Set up fd 0/1/2 on console */
	close(0); close(1); close(2);
	open("/dev/console", 2);	/* fd 0 = stdin */
	dup(0);				/* fd 1 = stdout */
	dup(0);				/* fd 2 = stderr */

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
