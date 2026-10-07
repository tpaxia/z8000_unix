/* Exhaust process storage, then verify reaping, reuse and fork isolation. */
#include <stdio.h>
#include <errno.h>
extern int errno;
int value = 123;
int cleared;
main(argc, argv)
char **argv;
{
	int expected, round, p[2], n, pid, first, status, i, failed;
	char c;
	unsigned old;
	if (argc == 3) {
		char next[4];
		i = atoi(argv[2]);
		if (value != 123 || cleared != 0) { printf("execmem: FAIL image\n"); return(1); }
		if (!i) { printf("execmem: passed\n"); return(0); }
		value = 456; cleared = 789;
		sprintf(next, "%d", i-1);
		execl(i & 1 ? "/bin/memoryn" : "/bin/memoryi", "memory", "flip", next, 0);
		printf("execmem: FAIL exec %d\n", errno); return(1);
	}
	if (argc != 2) return(1);
	if (argv[1][0] == 'u') { printf("execmem: FAIL unexpected success\n"); return(1); }
	if (argv[1][0] == 'd') {
		execl("/bin/huge", "memory", "unexpected", 0);
		if (errno != ENOMEM || value != 123 || cleared != 0) {
			printf("execmem: FAIL rollback %d\n", errno); return(1);
		}
		printf("execmem: passed\n"); return(0);
	}
	if (argv[1][0] == 'r') {
		if (setuid(1) < 0) return(3);
		expected = atoi(argv[1]+1);
	} else expected = atoi(argv[1]);
	failed = 0;
	for (round = 0; round < 3; round++) {
		if (pipe(p) < 0) return(2);
		first = 0;
		for (n = 0; n < 24; n++) {
			pid = fork();
			if (pid < 0) break;
			if (pid == 0) {
				value = 456; close(p[1]);
				_exit(read(p[0], &c, 1) != 0 || value != 456);
			}
			if (!first) first = pid;
		}
		if ((expected >= 0 ? n != expected : n >= 12) || errno != EAGAIN || value != 123) {
			printf("memory: FAIL round %d children %d expected %d errno %d\n", round, n, expected, errno);
			failed = 1;
		}
		if (expected < 0) {
			old = brk(0);
			if (brk(0xe800) != -1 || errno != ENOMEM ||
			    (unsigned)brk(0) != old || value != 123) {
				printf("memory: FAIL growth rollback\n"); failed = 1;
			}
		}
		if (n) {
			kill(first, 15);
			if (wait(&status) != first || status != 15) failed = 1;
			pid = fork();
			if (pid == 0) { close(p[1]); _exit(read(p[0], &c, 1) != 0); }
			if (pid < 0) { failed = 1; n--; }
		}
		close(p[1]); close(p[0]);
		for (i = 0; i < n; i++)
			if (wait(&status) < 0 || status != 0) failed = 1;
		if (expected < 0 && (brk(old+2048) < 0 || brk(old) < 0)) {
			printf("memory: FAIL growth after reclaim\n"); failed = 1;
		}
	}
	printf("memory: %s\n", failed ? "FAILED" : "passed");
	return(failed);
}
