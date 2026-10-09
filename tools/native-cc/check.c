#include <stdio.h>
#include <sys/types.h>
#include <sys/dir.h>

char buf[2048];
char buf2[2048];

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
    int fd, n, other, c;
    long offset;
    struct direct entry;

    fd = open("/tmp", 0);
    if (fd < 0) return 1;
    while (read(fd, &entry, sizeof entry) == sizeof entry)
        if (entry.d_ino && (strncmp(entry.d_name, "ctm", 3) == 0 ||
            (strncmp(entry.d_name, "oz", 2) == 0 &&
             entry.d_name[2] >= '0' && entry.d_name[2] <= '9'))) {
            printf("temporary file leaked: %.14s\n", entry.d_name);
            return 2;
        }
    close(fd);
    if (argc < 3) return 0;
    if (strcmp(argv[1], "same") == 0 && argc == 4) {
        fd = open(argv[2], 0); other = open(argv[3], 0);
        if (fd < 0 || other < 0) return 6;
        offset = 0;
        while ((n=read(fd,buf,sizeof buf)) > 0) {
            if (read(other,buf2,n) != n) {
                printf("short comparison read at %ld\n",offset);
                return 7;
            }
            for (c=0; c<n; c++)
                if (buf2[c]!=buf[c]) {
                    printf("comparison differs at %ld: %d versus %d\n",
                           offset+c,buf[c]&255,buf2[c]&255);
                    fwrite(buf,1,n,stdout); puts("\nEXPECTED:");
                    fwrite(buf2,1,n,stdout); return 7;
                }
            offset += n;
        }
        c = read(other,buf2,1);
        close(fd); close(other);
        return n < 0 || c != 0;
    }
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
    if (strcmp(argv[1], "e707") == 0)
        return (buf[0]&255) != 0xe7 || buf[1] != 7;
    if (strcmp(argv[1], "e711") == 0)
        return (buf[0]&255) != 0xe7 || buf[1] != 0x11;
    return 5;
}
