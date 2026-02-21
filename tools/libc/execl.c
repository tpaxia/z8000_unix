/*
 * execl - exec with inline argument list.
 * execl(name, arg0, arg1, ..., 0)
 *
 * In K&R C, the args are contiguous on the stack,
 * so &arg0 is a valid argv array pointer.
 */
execl(f, a)
char *f, *a;
{
	execve(f, &a, 0);
}
