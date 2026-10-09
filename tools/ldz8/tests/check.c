#include <errno.h>
#include <stdio.h>
extern int errno;
int canary = 12345;

main()
{
    int i, p, status, go[2], ready[2];
    char byte;
    char *program;
    if (pipe(go) || pipe(ready)) return 1;
    for (i = 0; i < 8; i++) {
        program = i&1 ? "/bin/split" : "/bin/combined";
        p = fork();
        if (p < 0) return 1;
        if (p == 0) {
            close(0); dup(go[0]); close(1); dup(ready[1]);
            close(go[0]); close(go[1]); close(ready[0]); close(ready[1]);
            execl(program, program, "wait", 0); exit(99);
        }
    }
    close(go[0]); close(ready[1]);
    /* All eight images must be resident before releasing any child. */
    for (i = 0; i < 8; i++) if (read(ready[0], &byte, 1) != 1) return 4;
    for (i = 0; i < 8; i++) if (write(go[1], &byte, 1) != 1) return 5;
    close(go[1]); close(ready[0]);
    for (i = 0; i < 8; i++) {
        if (wait(&status) < 0 || status) return 2;
    }
    for (i = 0; i < 10; i++) {
        char name[20];
        sprintf(name, "/bin/bad%d", i);
        if (execl(name, name, 0) != -1 || errno != ENOEXEC || canary != 12345) return 3;
    }
    printf("s.out exec PASS\n");
    return 0;
}
