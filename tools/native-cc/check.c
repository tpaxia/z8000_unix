#include <stdio.h>
#include <sys/types.h>
#include <sys/dir.h>

char buf[2048];

has(s, needle)
char *s, *needle;
{
    while (*s) {
        if (strncmp(s, needle, strlen(needle)) == 0) return 1;
        s++;
    }
    return 0;
}

main(argc, argv)
int argc;
char **argv;
{
    int fd, n;
    struct direct entry;

    fd = open("/tmp", 0);
    if (fd < 0) return 1;
    while (read(fd, &entry, sizeof entry) == sizeof entry)
        if (entry.d_ino && strncmp(entry.d_name, "ctm", 3) == 0) {
            printf("temporary file leaked: %.14s\n", entry.d_name);
            return 2;
        }
    close(fd);
    if (argc < 3) return 0;
    if (strcmp(argv[1], "absent") == 0)
        return access(argv[2], 0) == 0;
    fd = open(argv[2], 0);
    if (fd < 0) return 3;
    n = read(fd, buf, sizeof buf - 1);
    close(fd);
    if (n <= 0) return 4;
    buf[n] = 0;
    if (strcmp(argv[1], "pre") == 0)
        return !has(buf, "DRIVER_OK") || has(buf, "#include") || has(buf, "#define");
    if (strcmp(argv[1], "asm") == 0)
        return !has(buf, "_main:");
    if (strcmp(argv[1], "0407") == 0)
        return buf[0] != 1 || buf[1] != 7;
    if (strcmp(argv[1], "0411") == 0)
        return buf[0] != 1 || buf[1] != 9;
    return 5;
}
