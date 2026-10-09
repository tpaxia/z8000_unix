/* nroff uses a common array with the same name as libc's nlist function. */
int nlist[20];
main()
{
    nlist[0] = 17;
    nlist[19] = 42;
    return nlist[0] + nlist[19] != 59;
}
