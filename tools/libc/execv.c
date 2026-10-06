/* V7 execv: the Z8000 counterpart of libc/sys/execv.s. */
execv(file, argv)
char *file, **argv;
{
	extern char **environ;
	return(execve(file, argv, environ));
}
