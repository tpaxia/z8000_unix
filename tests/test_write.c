/* Quick test: write() outputs to console, _exit() halts. */
int main(void)
{
    write(1, "@@FINISHED\n", 11);
    _exit(0);
    return 0;
}
