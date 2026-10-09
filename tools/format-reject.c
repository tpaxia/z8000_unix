/* Check exec itself, without the shell's ENOEXEC script fallback. */
#include <errno.h>
extern int errno;
char *args[] = { "probe", 0 };
char *valid[] = { "/tmp/combined", "/tmp/split" };
char *obsolete[] = { "/tmp/old0407", "/tmp/old0410", "/tmp/old0411",
    "/tmp/old0405", "/tmp/sid0407", "/tmp/sid0410", "/tmp/sid0411",
    "/tmp/sid0405" };
check(path, reject)
char *path;
int reject;
{
    int pid, status;
    pid = fork();
    if (pid < 0) return 1;
    if (pid == 0) {
        errno = 0;
        execve(path, args, 0);
        exit(reject && errno == ENOEXEC ? 0 : 1);
    }
    return wait(&status) != pid || status != (reject ? 0 : 42<<8);
}
main()
{
    int i;
    for (i=0; i<2; i++) if (check(valid[i], 0)) return 1;
    for (i=0; i<8; i++) if (check(obsolete[i], 1)) return 2;
    write(1, "exec: s.out accepted, obsolete formats rejected\n", 46);
    return 0;
}
