#include <stdio.h>
#include <errno.h>
extern int errno;
/* A combined-space controller verifies the shared executable's inode lifetime.
 * Pipe descriptors survive exec; the split child stays resident until released.
 */
main(argc, argv)
char **argv;
{
	int a[2], b[2], fd, pid, status, failed;
	char c;
	if (argc > 1 && argv[1][1] == 'h') {
		write(4, "r", 1); read(5, &c, 1); return(0);
	}
	failed = 0;
	fd = open("/bin/texti", 1);
	if (fd < 0) return(3);
	pid = fork();
	if (!pid) {
		execl("/bin/texti", "memory", "th", 0);
		_exit(errno != ETXTBSY);
	}
	if (pid < 0 || wait(&status) != pid || status) failed++;
	close(fd);
	if (pipe(a) < 0 || pipe(b) < 0) return(4);
	if (a[0] != 3 || a[1] != 4 || b[0] != 5 || b[1] != 6) return(5);
	pid = fork();
	if (!pid) {
		execl("/bin/texti", "memory", "th", 0); _exit(99);
	}
	close(a[1]); close(b[0]);
	if (read(a[0], &c, 1) != 1) failed++;
	errno = 0;
	fd = open("/bin/texti", 1);
	if (fd != -1 || errno != ETXTBSY) failed++;
	if (fd >= 0) close(fd);
	write(b[1], "g", 1);
	if (wait(&status) != pid || status) failed++;
	close(a[0]); close(b[1]);
	fd = open("/bin/texti", 1);
	if (fd < 0) failed++;
	else close(fd);
	printf("text: %s\n", failed ? "FAILED" : "passed");
	return(failed);
}
