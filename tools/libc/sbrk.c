/* V7 keeps one exact program break shared by brk and sbrk.
 * The syscall wrapper owns curbrk, initialized to the linker's end symbol.
 */
extern char *curbrk;

char *
sbrk(incr)
int incr;
{
	char *old;

	old = curbrk;
	if (incr && brk(old + incr) == -1)
		return ((char *)-1);
	return (old);
}
