int shared[7];
extern int small;

main(argc, argv)
int argc;
char **argv;
{
    if (shared[0] || shared[6] || small) return 1;
    if (shared[1] != (argc > 1 ? 1234 : 0)) return 2;
    shared[6] = 41;
    putcommon();
    return shared[0] != 17 || shared[6] != 41 || small != 9;
}
