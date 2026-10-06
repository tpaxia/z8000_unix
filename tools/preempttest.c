/*
 * A child that never enters the kernel must not monopolize the CPU, and
 * signals must be delivered at interrupt return even without a syscall.
 * Run in the foreground: no /dev/null or shell background support needed.
 */
#include <stdio.h>
#include <signal.h>

long time();

fail(what)
char *what;
{
	printf("preempt: FAIL %s\n", what);
	exit(1);
}

/* Live counters and stack data must survive interrupts and switches. */
cpuwork(seed)
int seed;
{
	int values[16];
	register int i, j;

	for (j = 0; j < 16; j++)
		values[j] = seed + j;
	for (i = 1; i <= 2000; i++)
		for (j = 0; j < 16; j++) {
			values[j]++;
			if (values[j] != seed + j + i)
				exit(1);
		}
	exit(0);
}

main(argc, argv)
int argc;
char **argv;
{
	int pid, status, round, fd[2], first, second;
	char buf[8];
	long before[4], after[4], start;

	if (argc > 1) {
		pid = fork();
		if (pid < 0)
			fail("console fork");
		if (pid == 0)
			for (;;)
				;
		/* The driver waits for this marker, then lets the child spin
		 * before typing. read() needs VI to wake and preempt the child. */
		write(1, "preempt: waiting for input\n", 27);
		if (read(0, buf, sizeof(buf)) != 3 || buf[0] != 'g' ||
		    buf[1] != 'o' || buf[2] != '\n')
			fail("console read");
		if (kill(pid, SIGKILL) < 0 || wait(&status) != pid ||
		    status != SIGKILL)
			fail("console child");
		printf("preempt: console wakeup passed\n");
		exit(0);
	}

	for (round = 0; round < 3; round++) {
		if (times(before) < 0)
			fail("times");
		pid = fork();
		if (pid < 0)
			fail("fork");
		if (pid == 0)
			for (;;)
				;

		/* Allow several quanta, then kill the syscall-free child. */
		start = time(0);
		while (time(0) - start < 3)
			;
		if (kill(pid, SIGKILL) < 0)
			fail("kill");
		if (wait(&status) != pid || status != SIGKILL)
			fail("kill status");
		if (times(after) < 0 || after[2] - before[2] < 60)
			fail("child did not run for a quantum");
	}

	pid = fork();
	if (pid < 0)
		fail("alarm fork");
	if (pid == 0) {
		signal(SIGALRM, SIG_DFL);
		alarm(2);
		for (;;)
			;
	}
	if (wait(&status) != pid || status != SIGALRM)
		fail("alarm status");

	/* Signals to a blocked syscall must unwind through u_qsav, and the
	 * sleeper must leave the sleep queue before joining the run queue. */
	if (pipe(fd) < 0)
		fail("pipe");
	pid = fork();
	if (pid < 0)
		fail("sleeping fork");
	if (pid == 0) {
		close(fd[1]);
		read(fd[0], buf, 1);
		exit(1);
	}
	start = time(0);
	while (time(0) - start < 2)
		;
	if (kill(pid, SIGKILL) < 0 || wait(&status) != pid ||
	    status != SIGKILL)
		fail("sleeping kill");
	close(fd[0]);
	close(fd[1]);

	first = fork();
	if (first < 0)
		fail("context fork");
	if (first == 0)
		cpuwork(123);
	second = fork();
	if (second < 0)
		fail("context fork");
	if (second == 0)
		cpuwork(321);
	pid = wait(&status);
	if ((pid != first && pid != second) || status != 0)
		fail("first context");
	pid = pid == first ? second : first;
	if (wait(&status) != pid || status != 0)
		fail("second context");

	printf("preempt: scheduling, signals and context passed\n");
	exit(0);
}
