#include <stdio.h>

char buf[32], hex[65];
char digits[] = "0123456789abcdef";
char *asargs[] = {"asz8k", "-c", "-o", "/tmp/input.b", "/tmp/input.az8", 0};
char *ldargs[] = {"ldz8", "-x", "-x", "/lib/crt0.b", "/tmp/input.b",
                 "/tmp/lib.a", "/lib/libv7.a", "-o", "/tmp/result", 0};
char *program[] = {"result", 0};

execute(path, args)
char *path, **args;
{
    int pid, status;
    fflush(stdout);
    pid = fork();
    if (pid < 0) exit(1);
    if (pid == 0) {
        execve(path, args, 0);
        exit(2);
    }
    if (wait(&status) != pid || status) {
        printf("FAILED %s %d\n", path, status);
        exit(3);
    }
}

dump(path)
char *path;
{
    int fd, n, i, c;
    fd = open(path, 0);
    if (fd < 0) exit(4);
    printf("BEGIN %s\n", path);
    fflush(stdout);
    while ((n = read(fd, buf, 32)) > 0) {
        for (i = 0; i < n; i++) {
            c = buf[i] & 255;
            hex[2*i] = digits[c >> 4];
            hex[2*i+1] = digits[c & 15];
        }
        hex[2*n] = '\n';
        write(1, hex, 2*n+1);
    }
    if (n < 0) exit(5);
    close(fd);
    printf("END %s\n", path);
}

main(argc, argv)
int argc;
char **argv;
{
    if (argc > 1) ldargs[2] = argv[1];
    execute("/bin/asz8k", asargs);
    execute("/bin/ldz8", ldargs);
    chmod("/tmp/result", 0755);
    execute("/tmp/result", program);
    dump("/tmp/input.b");
    dump("/tmp/result");
    printf("NATIVE TOOLS PASS\n");
    return 0;
}
