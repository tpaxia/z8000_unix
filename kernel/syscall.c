extern void putc(int ch);

int syscall_handler(void)
{
    putc('H');
    putc('i');
    putc('\n');
    return 7;
}
