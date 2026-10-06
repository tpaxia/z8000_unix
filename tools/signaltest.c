#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <setjmp.h>

extern int errno;
int seen, nested, failed;
jmp_buf escape;
int clobber();

check(name, ok)
char *name;
{
	if (!ok) {
		printf("signal: FAIL %s\n", name);
		failed++;
	}
}

handler(sig)
{
	seen = sig;
	/* Exercise a syscall while the signal frame is on the user stack. */
	getpid();
}

inner(sig)
{
	nested = sig;
}

outer(sig)
{
	seen = sig;
	kill(getpid(), SIGINT);
	if (nested != SIGINT)
		failed++;
}

jumpout(sig)
{
	longjmp(escape, sig);
}

main(argc, argv)
int argc;
char **argv;
{
	int pid, status, fd[2], n;
	char c;
	int (*old)();
	char *args[3];

	if (argc == 2 && strcmp(argv[1], "exit") == 0)
		return(SIGTERM);
	if (argc == 2 && strcmp(argv[1], "term") == 0) {
		signal(SIGTERM, SIG_DFL);
		kill(getpid(), SIGTERM);
		return(1);
	}
	if (argc == 2)
		return(signal(SIGTERM, SIG_IGN) != SIG_DFL ||
		    signal(SIGINT, SIG_DFL) != SIG_IGN);

	old = signal(SIGTERM, handler);
	check("initial disposition", old == SIG_DFL);
	check("old handler", signal(SIGTERM, inner) == handler);
	check("replacement", signal(SIGTERM, handler) == inner);
	kill(getpid(), SIGTERM);
	check("handler and return", seen == SIGTERM);
	check("one-shot reset", signal(SIGTERM, SIG_IGN) == SIG_DFL);
	seen = 0;
	kill(getpid(), SIGTERM);
	check("ignore", seen == 0 && signal(SIGTERM, SIG_DFL) == SIG_IGN);
	errno = 0;
	check("uncatchable kill", signal(SIGKILL, handler) == (int (*)())-1 && errno == EINVAL);
	check("invalid signal", signal(NSIG, handler) == (int (*)())-1 && errno == EINVAL);

	seen = 0;
	signal(SIGALRM, clobber);
	alarm(2);
	check("asynchronous registers and flags", regtest() == 0 && seen == 1);
	alarm(0);

	seen = 0;
	check("pipe", pipe(fd) == 0);
	signal(SIGALRM, handler);
	/* V7 rounds to the next seconds boundary: 1 could fire before read. */
	alarm(2);
	/* Keep the writer open so this read blocks until the alarm. */
	n = read(fd[0], &c, 1);
	check("interrupted read", n == -1 && errno == EINTR && seen == SIGALRM);
	close(fd[0]);
	close(fd[1]);

	seen = nested = 0;
	signal(SIGINT, inner);
	signal(SIGTERM, outer);
	kill(getpid(), SIGTERM);
	check("nested delivery", seen == SIGTERM && nested == SIGINT);

	n = setjmp(escape);
	if (n == 0) {
		signal(SIGTERM, jumpout);
		kill(getpid(), SIGTERM);
		check("longjmp did not jump", 0);
	} else
		check("longjmp", n == SIGTERM);

	/* Normal exit and signal death use different bytes of wait status. */
	pid = fork();
	if (pid == 0)
		_exit(SIGTERM);
	check("normal exit status", pid > 0 && wait(&status) == pid &&
	    status == (SIGTERM << 8));

	/* Fork inherits the libc handler table and kernel disposition. */
	seen = 0;
	signal(SIGTERM, handler);
	pid = fork();
	if (pid == 0) {
		kill(getpid(), SIGTERM);
		if (seen != SIGTERM)
			_exit(1);
		/* The second delivery must take the default termination action. */
		kill(getpid(), SIGTERM);
		_exit(2);
	}
	n = wait(&status);
	if (n != pid || status != SIGTERM || seen != 0)
		printf("signal: fork pid=%d wait=%d status=%x seen=%d\n", pid, n, status, seen);
	check("fork and default second delivery", pid > 0 &&
	    n == pid && status == SIGTERM && seen == 0);
	signal(SIGTERM, SIG_DFL);

	/* V7 keeps SIGILL/SIGTRAP handlers installed after delivery. */
	signal(SIGILL, handler);
	kill(getpid(), SIGILL);
	seen = 0;
	kill(getpid(), SIGILL);
	check("persistent SIGILL", seen == SIGILL && signal(SIGILL, SIG_DFL) == handler);

	signal(SIGTERM, handler);
	signal(SIGINT, SIG_IGN);
	pid = fork();
	if (pid == 0) {
		args[0] = "signaltest";
		args[1] = "exec";
		args[2] = 0;
		execve("/bin/signaltest", args, (char **)0);
		_exit(1);
	}
	check("exec resets caught and preserves ignored", pid > 0 &&
	    wait(&status) == pid && status == 0);
	signal(SIGINT, SIG_DFL);

	pid = fork();
	if (pid == 0) {
		badstk(getpid());
		_exit(1);
	}
	check("unusable signal stack", pid > 0 && wait(&status) == pid &&
	    status == SIGSEGV);
	signal(SIGTERM, SIG_DFL);

	if (!failed)
		printf("signal: all checks passed\n");
	printf("signal: complete\n");
	return(failed != 0);
}
