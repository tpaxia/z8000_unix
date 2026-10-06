/*
 * exit - flush and close stdio, then terminate the process.
 * (Seventh Edition: libc/gen/cuexit.s.) _cleanup is stdio's when the
 * program uses stdio and the dummy in fakcu.c otherwise.
 */
exit(code)
{
	_cleanup();
	_exit(code);
}
