#include <stdio.h>
char payload[30000];
main()
{
	int i, j, pid, status, p[2], failed;
	char c;
	failed = 0;
	for (i = 0; i < 30000; i++) payload[i] = i % 113;
	if (pipe(p) < 0) return(1);
	for (j = 0; j < 4; j++) {
		pid = fork();
		if (pid < 0) { failed++; break; }
		if (!pid) {
			close(p[1]);
			for (i = 0; i < 30000; i += 127)
				if (payload[i] != i % 113) _exit(2);
			payload[j] = 120;
			read(p[0], &c, 1);
			_exit(payload[j] != 120);
		}
	}
	close(p[0]); close(p[1]);
	for (i = 0; i < j; i++)
		if (wait(&status) < 0 || status) failed++;
	for (i = 0; i < 30000; i += 127)
		if (payload[i] != i % 113) { failed++; break; }
	printf("swapfork: %s\n", failed ? "FAILED" : "passed");
	sync();
	read(0, &c, 1); /* Hold the verified image while the harness observes idle. */
	return(failed);
}
