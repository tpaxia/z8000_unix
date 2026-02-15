extern void putc();

int cons_write(buf, count)
    char *buf;
    int count;
{
    int i;
    for (i = 0; i < count; i++)
        putc(buf[i]);
    return count;
}

int sys_nosys();
int sys_exit();
int sys_write();

struct sysent {
    int (*sy_call)();
    int sy_narg;
};

struct sysent sysent[64] = {
    { sys_nosys, 0 },   /*  0 = indir */
    { sys_exit,  1 },   /*  1 = exit */
    { 0,         0 },   /*  2 = fork */
    { 0,         0 },   /*  3 = read */
    { sys_write, 3 }    /*  4 = write */
    /* 5-63: zero-initialized { NULL, 0 } */
};

int syscall_handler(num, regs)
    int num;
    unsigned *regs;
{
    int (*fn)();
    if (num < 0 || num > 63)
        return -1;
    fn = sysent[num].sy_call;
    if (fn == 0)
        return -1;
    return (*fn)(regs);
}

int sys_exit(regs)
    unsigned *regs;
{
    return regs[1];
}

int sys_write(regs)
    unsigned *regs;
{
    int fd;
    char *buf;
    int count;
    fd    = regs[1];
    buf   = (char *)regs[2];
    count = regs[3];
    if (fd == 1 || fd == 2)
        return cons_write(buf, count);
    return -1;
}

int sys_nosys(regs)
    unsigned *regs;
{
    return -1;
}
