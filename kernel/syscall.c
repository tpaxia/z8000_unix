extern int cons_write(char *buf, int count);

int syscall_handler(int num, int *regs)
{
    switch (num) {
    case 4: {  /* write(fd, buf, count) */
        int fd    = regs[1];
        char *buf = (char *)regs[2];
        int count = regs[3];
        if (fd == 1 || fd == 2)
            return cons_write(buf, count);
        return -1;
    }
    default:
        return -1;
    }
}
