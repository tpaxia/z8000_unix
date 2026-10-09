int extdata = 11;
extern int *pd, *pd2, *pd3, *pb, (*pt)();
extern long wide;
extern char small;
int helper() { return 13; }
int main()
{
    if (probe() != 31 || *pb != 31 || *pd != 7) return 1;
    if ((*pt)() != 31 || getabs() != 9) return 2;
    if (*pd2 != 8 || *pd3 != 9) return 4;
    if (wide != 9L || small != 9) return 3;
    return 0;
}
