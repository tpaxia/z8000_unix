#include <stdio.h>
int value=5678;
main(argc,argv)
char **argv;
{
	char *args[3];
	if (argc!=2 || value!=5678) return(1);
	if (strcmp(argv[1],"normal")==0) {
		args[0]="idnext"; args[1]="back"; args[2]=0;
		execve("/bin/idnext",args,0);
		return(2);
	}
	if (strcmp(argv[1],"split") && strcmp(argv[1],"back")) return(3);
	return(0);
}
