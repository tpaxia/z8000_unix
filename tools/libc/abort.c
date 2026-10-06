/*
 * abort - end the program abnormally. Seventh Edition executes an IOT
 * instruction (libc/gen/abort.s); here the process sends itself SIGIOT
 * and, should that return, exits with a failure status.
 */
#include <signal.h>

abort()
{
	kill(getpid(), SIGIOT);
	_exit(1);
}
