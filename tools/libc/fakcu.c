/*
 * Dummy _cleanup, used when the program does not use stdio.
 * (Seventh Edition: libc/gen/fakcu.s.) It must come after flsbuf in the
 * archive so that stdio's own _cleanup is the one chosen when present.
 */
_cleanup()
{
}
