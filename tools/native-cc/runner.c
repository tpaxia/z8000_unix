#include <stdio.h>

char line[512];
char *args[40];
char *envlist[] = {"CC_TEST=inherited", 0};

/* Plan lines: expected-failure(0/1), stdout-file(or -), executable, args. */
runplan(name, directory)
char *name, *directory;
{
    FILE *plan;
    char *p, *capture;
    int n, pid, status, failed, count;

    if (chdir(directory) < 0) return 1;
    plan = fopen(name, "r");
    if (plan == NULL) return 2;
    count = 0;
    while (fgets(line, sizeof line, plan)) {
        failed = line[0] == '1';
        p = line + 2;
        capture = p;
        while (*p && *p != ' ') p++;
        if (*p == 0) return 3;
        *p++ = 0;
        n = 0;
        while (*p && *p != '\n') {
            if (n >= 39) return 4;
            args[n++] = p;
            while (*p && *p != ' ' && *p != '\n') p++;
            if (*p) *p++ = 0;
        }
        args[n] = 0;
        if (n == 0) return 5;
        pid = fork();
        if (pid < 0) return 6;
        if (pid == 0) {
            if (*capture != '-') {
                close(1);
                if (creat(capture, 0600) != 1) exit(7);
            }
            execve(args[0], args, envlist);
            exit(127);
        }
        if (wait(&status) != pid || (status & 255) ||
            (status != 0) != failed || status == (127 << 8)) {
            printf("FAILED command %d %s status %d\n", count, args[0], status);
            return 8;
        }
        count++;
        printf("command %d OK\n", count);
        fflush(stdout);
    }
    fclose(plan);
    printf("NATIVE CC PASS\n");
    return 0;
}

main(argc, argv)
int argc;
char **argv;
{
    int status;
    status = runplan(argc > 1 ? argv[1] : "plan", argc > 2 ? argv[2] : "/tmp");
    sync();
    printf("NATIVE CC DONE\n");
    return status;
}
