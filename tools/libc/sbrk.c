/*
 * sbrk - increment program break.
 * Uses brk() syscall to set the new break address.
 * Returns pointer to old break on success, (char *)-1 on failure.
 *
 * Initializes curbrk using brk(0) which returns the current break
 * address from the kernel.  This is more reliable than using &end
 * because the ACK linker may place common symbols after _end.
 */
static char *curbrk = 0;

char *
sbrk(incr)
int incr;
{
	char *old;

	if (curbrk == 0)
		curbrk = (char *)brk(0);
	old = curbrk;
	if (brk(curbrk + incr) == -1)
		return ((char *)-1);
	curbrk += incr;
	return (old);
}
