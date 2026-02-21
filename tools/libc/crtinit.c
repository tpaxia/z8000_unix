/*
 * crtinit.c - C runtime initialization called from crt0.s.
 * Sets up environ pointer before main() is called.
 */
extern char **environ;

void
_crtinit(argc, argv)
int argc;
char **argv;
{
	environ = argv + argc + 1;
}
