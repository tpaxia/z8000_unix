/* Coordinated V7 syscall-number and exec environment regression. */
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
extern char **environ;
extern int errno;
int failed;
check(name, ok) char *name;
{
	if (!ok) { printf("abi: FAIL %s errno=%d\n", name, errno); failed++; }
}
startup(mode)
{
	char *args[3], *env[2];
	int pid, status;
	args[0] = "abitest";
	args[1] = (mode == 0 || mode == 2) ? "empty" : "environment";
	args[2] = 0;
	env[0] = "ABI_ENV=present"; env[1] = 0;
	pid = fork();
	if (pid == 0) {
		environ = env;
		switch (mode) {
		case 0: rawex11("/bin/abitest", args, (char **)0177777); break;
		case 1: execve("/bin/abitest", args, env); break;
		case 2: execve("/bin/abitest", args, (char **)0); break;
		case 3: execv("/bin/abitest", args); break;
		case 4: execl("/bin/abitest", "abitest", "environment", (char *)0); break;
		}
		_exit(1);
	}
	return(pid > 0 && wait(&status) == pid && status == 0);
}
jail(raw)
{
	int pid, status;
	struct stat st;
	pid = fork();
	if (pid == 0) {
		if ((raw ? rawroot("/jail") : chroot("/jail")) != 0 ||
		    chdir("/") != 0 || stat("/inside", &st) != 0 ||
		    stat("/bin/abitest", &st) != -1 || errno != ENOENT)
			_exit(1);
		_exit(0);
	}
	return(pid > 0 && wait(&status) == pid && status == 0);
}
/* Recursive make needs more than the old eight process slots. */
capacity()
{
	int p[2], pid, n, status, ok, i;
	char c;
	if (pipe(p) < 0) return(0);
	for (n = 0; n < 32; n++) {
		pid = fork();
		if (pid < 0) break;
		if (pid == 0) {
			close(p[1]);
			_exit(read(p[0], &c, 1) != 0);
		}
	}
	ok = n >= 9 && n < 32 && errno == EAGAIN;
	close(p[1]); close(p[0]);
	for (i = 0; i < n; i++)
		if (wait(&status) < 0 || status != 0) ok = 0;
	return(ok);
}
main(argc, argv, envp)
char **argv, **envp;
{
	int fd, old;
	struct stat st;
	if (argc == 2) {
		if (argv[2] != 0 || envp != environ || environ != argv + argc + 1 ||
		    strcmp(argv[0], "abitest") != 0)
			return(1);
		if (strcmp(argv[1], "empty") == 0) return(environ[0] != 0);
		return(strcmp(argv[1], "environment") != 0 || environ[0] == 0 ||
		    strcmp(environ[0], "ABI_ENV=present") != 0 || environ[1] != 0);
	}
	check("process capacity and exhaustion", capacity());
	check("SC11 ignores third register", startup(0));
	check("execve environment", startup(1));
	check("execve empty environment", startup(2));
	check("execv inherits environment", startup(3));
	check("execl inherits environment", startup(4));
	check("execve failure", execve("/missing", argv, environ) == -1 && errno == ENOENT);
	check("execl failure", execl("/missing", "missing", (char *)0) == -1 && errno == ENOENT);
	old = rawmask(027);
	check("SC60 and libc umask agree", umask(077) == 027 && rawmask(027) == 077);
	fd = creat("/tmp/mode", 0666);
	check("creation mask", fd >= 0 && fstat(fd, &st) == 0 && (st.st_mode & 0777) == 0640);
	close(fd); umask(old);
	check("SC61 chroot", jail(1));
	check("libc chroot", jail(0));
	check("parent root unchanged", stat("/bin/abitest", &st) == 0);
	check("chroot rejects file", chroot("/bin/abitest") == -1 && errno == ENOTDIR);
	check("SC52 reserved", oldphys() == -1 && errno == 35 /* port ENOSYS, absent in original V7 libc headers */);
	printf("abi: %s\n", failed ? "FAILED" : "passed");
	return(failed != 0);
}
