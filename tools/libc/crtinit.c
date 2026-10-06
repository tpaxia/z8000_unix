/*
 * Historical ACK startup helper, called from the unbuilt crt0.s.
 * The active PCC crt0.az8 initializes environ directly.
 */
extern char **environ;

void
_crtinit(argc, argv)
int argc;
char **argv;
{
	environ = argv + argc + 1;
}
