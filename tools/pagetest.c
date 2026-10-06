/* Exercise real mappings through syscalls and user faults in both layouts. */
#include <stdio.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
extern int errno;
int failed, guard = 77;
check(s, ok) char *s;
{ if (!ok) { printf("pages: FAIL %s errno=%d\n", s, errno); failed++; } }
deep(n)
{
	char space[512];
	space[0] = n;
	if (n) return(deep(n-1)+space[0]);
	return(space[0]);
}
leap(n)
{
	char space[6144];
	space[0] = n; space[6143] = n+1;
	if (n) return(leap(n-1)+space[0]+space[6143]);
	return(space[0]+space[6143]);
}
workspace()
{
	int fd, n, i, total, file;
	char buf[128];
	for (file = 1; file <= 2; file++) {
		fd = open(file == 1 ? "/tmp/word" : "/tmp/here", 0);
		check("workspace file", fd >= 0);
		total = 0;
		while ((n = read(fd, buf, sizeof(buf))) > 0)
			for (i = 0; i < n; i++, total++)
				if (buf[i] != (total == file*3000 ? '\n' : "abC123"[total%6]))
					failed++;
		check("workspace length", total == file*3000+1);
		close(fd);
	}
	printf("workspace:%s\n", failed ? "FAILED" : "passed");
	return(failed);
}
main(argc, argv)
char **argv;
{
	unsigned start, end, saved;
	int i, fd, pid, status;
	char *p;
	struct stat st;
	if (argc > 1) return(workspace());
	start = ((unsigned)brk(0)+2047) & ~2047;
	end = start+6144;
	check("grow", brk(end) == 0);
	p = (char *)start;
	for (i = 0; i < 6144; i++) {
		if (p[i] != 0) { check("new pages cleared", 0); break; }
		p[i] = 90;
	}
	check("shrink", brk(start+64) == 0);
	check("regrow", brk(end) == 0);
	for (i = 0; i < 6144; i++)
		if (p[i] != (i < 64 ? 90 : 0)) { check("retained prefix and cleared regrowth", 0); break; }
	pid = fork();
	if (!pid) { p[0] = 33; p[6143] = 44; _exit(guard != 77); }
	check("fork", pid > 0 && wait(&status) == pid && status == 0);
	check("parent isolation", p[0] == 90 && p[6143] == 0 && guard == 77);
	saved = brk(0);
	check("stack overlap rejected", brk(0xff00) == -1 && errno == ENOMEM &&
	    (unsigned)brk(0) == saved && p[0] == 90);
	check("release pages", brk(start) == 0);
	fd = creat("/tmp/pages", 0600);
	check("create", fd >= 0);
	errno = 0;
	check("gap write", write(fd, p, 1) == -1 && errno == EFAULT);
	errno = 0;
	check("cross gap", write(fd, p-1, 2) == -1 && errno == EFAULT);
	errno = 0;
	check("gap structure copy", fstat(fd, p) == -1 && errno == EFAULT);
	check("valid copy after fault", fstat(fd, &st) == 0 && st.st_size == 0);
	pid = fork();
	if (!pid) _exit(*p);
	check("user gap is SIGSEGV", pid > 0 && wait(&status) == pid && status == SIGSEGV);
	close(fd);
	pid = fork();
	if (!pid) { *(int *)0xf002 = 123; _exit(*(int *)0xf002 != 123); }
	check("warning preserves valid mapped store", pid > 0 && wait(&status) == pid && status == 0);
	pid = fork();
	if (!pid) _exit(deep(12) != 78);
	check("automatic stack growth", pid > 0 && wait(&status) == pid && status == 0);
	pid = fork();
	if (!pid) _exit(leap(3) != 16);
	check("fault backout across whole pages", pid > 0 && wait(&status) == pid && status == 0);
	pid = fork();
	if (!pid) _exit(callgrow());
	check("CALL backout", pid > 0 && wait(&status) == pid && status == 0);
	pid = fork();
	if (!pid) _exit(pushgrow() != 0x1234);
	check("PUSH backout", pid > 0 && wait(&status) == pid && status == 0);
	pid = fork();
	if (!pid) { rmwgrow(); _exit(99); }
	check("unsafe RMW rejected", pid > 0 && wait(&status) == pid && status == SIGSEGV);
	printf("pages: %s\n", failed ? "FAILED" : "passed");
	return(failed);
}
