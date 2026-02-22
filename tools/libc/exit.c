/*
 * exit() - terminate process.
 * Calls _exit() directly (no stdio to flush).
 */
exit(code)
{
	_exit(code);
}
