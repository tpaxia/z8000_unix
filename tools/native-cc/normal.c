#include <stdio.h>

/* Test V7 commands whose main falls through without an explicit return.
 * Output is checked separately; a signal or failed exec must still fail.
 */
main(argc, argv)
int argc;
char **argv;
{
    int pid, status;
    if (argc < 2) return 1;
    pid = fork();
    if (pid < 0) return 1;
    if (pid == 0) {
        execv(argv[1], argv + 1);
        _exit(127);
    }
    if (wait(&status) != pid || (status & 255) || status == (127 << 8))
        return 1;
    fprintf(stderr, "%s exit %d\n", argv[1], status >> 8);
    return 0;
}
