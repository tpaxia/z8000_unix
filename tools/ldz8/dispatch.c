/* The installed linker accepts only Z8000 s.out objects and executables. */
extern int ldso();
int main(argc, argv)
int argc;
char **argv;
{
    return ldso(argc, argv);
}
