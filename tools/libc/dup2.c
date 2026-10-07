/* V7 dup's two-descriptor form uses bit 0100 in the first argument. */
dup2(old, new)
int old, new;
{
    return dup(old | 0100, new);
}
